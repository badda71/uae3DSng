/*
 * uae3DSng - sound output for the 3DS DSP (libctru ndsp), replacing src/sound_sdl_new.cpp on 3DS.
 *
 * Why not SDL audio: with the SDL 3DS audio driver the SDL audio thread never reached the emulator's
 * callback (callbacks stayed 0 while the DSP was alive), and upstream's blocking producer/consumer hand-off
 * then froze the emulator. Here there is no audio thread and no waiting at all:
 *
 *   - SND_SLOTS buffers live in linear memory (DSP-readable). The emulator writes samples directly into
 *     the current slot (sndbufpt/render_sndbuff, same contract as upstream's audio.cpp macros).
 *   - finish_sound_buffer() flushes the slot, queues it on DSP channel 0 and moves on to a free slot.
 *   - If the DSP is too far behind (SND_MAX_QUEUED buffers pending) the new buffer is dropped (bounded
 *     latency); if it runs dry the channel simply stops until the next buffer arrives (brief silence).
 *   - Emulation speed is NOT paced by audio; the frame limiter in sdlgfx.cpp does that.
 */
#include <sys/types.h>
#include <string.h>
#include <stdint.h>

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "uae.h"
#include "options.h"
#include "memory-uae.h"
#include "debug_uae4all.h"
#include "audio.h"
#include "gensound.h"
#include "sound.h"
#include "custom.h"
#include "menu_config.h"

#include <3ds.h>

extern unsigned long next_sample_evtime;

#define SND_SLOTS       8     /* buffers in the ring (linear memory) */
#define SND_MAX_QUEUED  5     /* at most this many handed to the DSP at once (~5 x 23 ms of latency) */
#define SND_PRIME       2     /* silent buffers queued at start/resume as jitter margin */

int produce_sound = 0;
int changed_produce_sound = 0;
unsigned int sound_rate = DEFAULT_SOUND_FREQ;

unsigned n_callback_sndbuff, n_render_sndbuff;
static uae_u16 sndbuffer_idle[SNDBUFFER_LEN * DEFAULT_SOUND_CHANNELS + 32] UAE4ALL_ALIGN;  /* used while the DSP is not open */
uae_u16 *sndbufpt = sndbuffer_idle;
uae_u16 *render_sndbuff = sndbuffer_idle;

static int have_sound = 0;          /* init_sound() succeeded */
static int dsp_ok = 0;              /* ndsp initialised and channel configured */
static int snd_rate = 0, snd_stereo = -1;
static size_t snd_bytes = 0;        /* bytes of audio per buffer (depends on stereo) */
static size_t snd_stride = 0;       /* distance between slots in linear memory */
static uint8_t *snd_mem = NULL;     /* linearAlloc'd, SND_SLOTS * snd_stride */
static ndspWaveBuf snd_wb[SND_SLOTS];
static int snd_cur = 0;             /* slot the emulator is filling right now */
static unsigned st_submitted = 0, st_dropped = 0, st_silent = 0, st_calls = 0;

void sound_default_evtime(void)
{
	int pal = beamcon0 & 0x20;
	if (pal)
		scaled_sample_evtime = (MAXHPOS_PAL*313*VBLANK_HZ_PAL*CYCLE_UNIT)/sound_rate;
	else
		scaled_sample_evtime = (MAXHPOS_NTSC*MAXVPOS_NTSC*VBLANK_HZ_NTSC*CYCLE_UNIT)/sound_rate + 1;

	schedule_audio();
}

static inline uae_u16 *slot_ptr(int s) { return (uae_u16 *)(snd_mem + (size_t)s * snd_stride); }
static inline int wb_status(int s)     { return *(volatile u8 *)&snd_wb[s].status; }   /* written by the ndsp thread */
static inline int slot_free(int s)     { int st = wb_status(s); return st == NDSP_WBUF_FREE || st == NDSP_WBUF_DONE; }

static int queued_count(void)
{
	int n = 0;
	for (int s = 0; s < SND_SLOTS; s++) if (!slot_free(s)) n++;
	return n;
}

static void log_stats(const char *tag)
{
	char st[SND_SLOTS + 1];
	for (int s = 0; s < SND_SLOTS; s++) st[s] = '0' + wb_status(s);   /* 0 free, 1 queued, 2 playing, 3 done */
	st[SND_SLOTS] = 0;
	write_log("audio[%s]: submitted=%u dropped=%u silent=%u | slots=%s | ndsp playing=%d seq=%u pos=%u\n",
		tag, st_submitted, st_dropped, st_silent, st,
		(int)ndspChnIsPlaying(0), (unsigned)ndspChnGetWaveBufSeq(0), (unsigned)ndspChnGetSamplePos(0));
}

/* Hand the slot the emulator just filled to the DSP and continue with a free slot. */
static void submit_current(void)
{
	int s = snd_cur;
	if (queued_count() < SND_MAX_QUEUED) {
		DSP_FlushDataCache(slot_ptr(s), snd_bytes);
		ndspChnWaveBufAdd(0, &snd_wb[s]);
		st_submitted++;
	} else {
		st_dropped++;            /* DSP far behind: drop instead of building up latency */
	}
	int next = s;
	for (int i = 1; i <= SND_SLOTS; i++) {
		int c = (s + i) % SND_SLOTS;
		if (slot_free(c)) { next = c; break; }
	}
	snd_cur = next;
	sndbufpt = render_sndbuff = slot_ptr(next);
}

static void prime(int n)
{
	for (int i = 0; i < n; i++) {
		memset(slot_ptr(snd_cur), 0, snd_bytes);
		submit_current();
		st_silent++;
	}
}

static int dsp_open(int rate, int stereo)
{
	if (dsp_ok && rate == snd_rate && stereo == snd_stereo)
		return 0;                                            /* nothing changed */

	if (!dsp_ok) {
		Result rc = ndspInit();
		if (R_FAILED(rc)) {
			write_log("ndspInit failed: 0x%08lX%s -> sound disabled\n", (unsigned long)rc,
				(R_SUMMARY(rc) == RS_NOTFOUND && R_MODULE(rc) == RM_DSP) ? " (sdmc:/3ds/dspfirm.cdc missing)" : "");
			return -1;
		}
	}

	if (!snd_mem) {
		snd_stride = ((SNDBUFFER_LEN * 2) + 64 + 31) & ~(size_t)31;   /* stereo size + overshoot margin, cache-line aligned */
		snd_mem = (uint8_t *)linearAlloc(snd_stride * SND_SLOTS);
		if (!snd_mem) {
			write_log("linearAlloc(%u) for audio failed -> sound disabled\n", (unsigned)(snd_stride * SND_SLOTS));
			return -1;
		}
		memset(snd_mem, 0, snd_stride * SND_SLOTS);
	}

	int reconfig = dsp_ok;
	ndspChnWaveBufClear(0);
	ndspChnReset(0);
	if (reconfig) svcSleepThread(15 * 1000 * 1000);          /* let the DSP drop references to the old buffers */

	ndspSetOutputMode(stereo ? NDSP_OUTPUT_STEREO : NDSP_OUTPUT_MONO);
	ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
	ndspChnSetRate(0, (float)rate);
	ndspChnSetFormat(0, stereo ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
	float mix[12];
	memset(mix, 0, sizeof(mix));
	mix[0] = 1.0f;
	mix[1] = 1.0f;
	ndspChnSetMix(0, mix);

	snd_bytes = stereo ? SNDBUFFER_LEN * 2 : SNDBUFFER_LEN;
	for (int i = 0; i < SND_SLOTS; i++) {
		memset(&snd_wb[i], 0, sizeof(snd_wb[i]));
		snd_wb[i].data_vaddr = slot_ptr(i);
		snd_wb[i].nsamples   = snd_bytes / (stereo ? 4 : 2);
		snd_wb[i].status     = NDSP_WBUF_FREE;
	}
	snd_rate = rate;
	snd_stereo = stereo;
	snd_cur = 0;
	sndbufpt = render_sndbuff = slot_ptr(0);
	st_submitted = st_dropped = st_silent = st_calls = 0;
	dsp_ok = 1;

	write_log("DSP audio open: %d Hz, %s, %u bytes (%u frames) per buffer, %d slots\n",
		rate, stereo ? "stereo" : "mono", (unsigned)snd_bytes, (unsigned)snd_wb[0].nsamples, SND_SLOTS);
	prime(SND_PRIME);

	/* record whether the DSP actually picks the buffers up */
	svcSleepThread(30 * 1000 * 1000);
	log_stats("30 ms after open");
	svcSleepThread(270 * 1000 * 1000);
	log_stats("300 ms after open");
	return 0;
}

/* called when the menu changes sound settings and on exit */
void gp2x_stop_sound(void)
{
	if (dsp_ok) {
		write_log("DSP audio: stop\n");
		ndspChnWaveBufClear(0);
		ndspExit();
		dsp_ok = 0;
	}
	if (snd_mem) { linearFree(snd_mem); snd_mem = NULL; }
	sndbufpt = render_sndbuff = sndbuffer_idle;
}

void finish_sound_buffer(void)
{
	if (!dsp_ok) {                         /* no audio device: just reuse the idle buffer */
		sndbufpt = render_sndbuff = sndbuffer_idle;
		return;
	}
	submit_current();
	st_calls++;
	if (st_calls == 1 || st_calls == 50 || st_calls == 250 || st_calls % 2000 == 0) {
		char tag[32];
		snprintf(tag, sizeof(tag), "after %u buffers", st_calls);
		log_stats(tag);
	}
}

/* Try to determine whether sound is available.  This is only for GUI purposes. */
int setup_sound(void)
{
	return 1;
}

static int open_sound(void)
{
	if (dsp_open(sound_rate, mainMenu_soundStereo ? 1 : 0) != 0)
		return 0;

	sound_default_evtime();
	have_sound = 1;
	scaled_sample_evtime_ok = 1;
	sound_available = 1;
	return 1;
}

void close_sound(void)
{
	if (!have_sound)
		return;
	/* the DSP stays open until exit (reopening is slow and unnecessary) */
	have_sound = 0;
}

int init_sound(void)
{
	have_sound = open_sound();
	return have_sound;
}

void pause_sound(void)
{
	/* nothing to do: with no audio being produced the queue just drains into silence */
}

void resume_sound(void)
{
	if (!dsp_ok)
		return;
	int q = queued_count();
	if (q < SND_PRIME)
		prime(SND_PRIME - q);              /* restore the jitter margin after a pause */
}

void uae4all_init_sound(void) { }
void uae4all_play_click(void) { }
void uae4all_pause_music(void) { }
void uae4all_resume_music(void) { }
