 /*
  * UAE - The Un*x Amiga Emulator
  *
  * OS specific functions
  *
  * Copyright 1995, 1996, 1997 Bernd Schmidt
  * Copyright 1996 Marcus Sundberg
  * Copyright 1996 Manfred Thole
  */


#define UNROLL_LOOPS
#define UNROLL_AUDIO_HANDLER

#include "sysconfig.h"
#include "sysdeps.h"

#include "config.h"
#include "uae.h"
#include "options.h"
#include "memory-uae.h"
#include "custom.h"
#include "m68k/m68k_intrf.h"
#include "debug_uae4all.h"
#include "autoconf.h"
#include "savestate.h"
#include "gensound.h"
#include "sound.h"
#include "events.h"
#include "audio.h"
#include "debug_uae4all.h"
#include "menu_config.h"

#define MAX_EV ~0ul
#define maxhpos MAXHPOS

struct audio_channel_data audio_channel[4] UAE4ALL_ALIGN;
int audio_channel_current_sample[4] UAE4ALL_ALIGN;
int audio_channel_vol[4] UAE4ALL_ALIGN;
unsigned long audio_channel_adk_mask[4] UAE4ALL_ALIGN;
int audio_channel_state[4] UAE4ALL_ALIGN;
unsigned long audio_channel_evtime[4] UAE4ALL_ALIGN;

int sound_available = 1;

unsigned long sample_evtime, scaled_sample_evtime;
int scaled_sample_evtime_ok;

static unsigned long last_cycles;
unsigned long next_sample_evtime;

typedef uae_s8 sample8_t;

#define PUT_SOUND_WORD(b) \
{ \
	*(uae_u16 *)sndbufpt = b; \
	sndbufpt = (uae_u16 *)(((uae_u8 *)sndbufpt) + 2); \
}

#ifdef __SWITCH__
#define CHECK_SOUND_BUFFERS() \
{ \
      if ((hostptr)sndbufpt - (hostptr)render_sndbuff >= SNDBUFFER_LEN*2) { \
  	    finish_sound_buffer (); } \
} \

#define SAMPLE_HANDLER \
	{ \
		register uae_u32 d0 = audio_channel_current_sample[0]; \
		register uae_u32 d1 = audio_channel_current_sample[1]; \
		register uae_u32 d2 = audio_channel_current_sample[2]; \
		register uae_u32 d3 = audio_channel_current_sample[3]; \
		d0 *= audio_channel_vol[0]; \
		d1 *= audio_channel_vol[1]; \
		d2 *= audio_channel_vol[2]; \
		d3 *= audio_channel_vol[3]; \
		d0 &= audio_channel_adk_mask[0]; \
		d1 &= audio_channel_adk_mask[1]; \
		d2 &= audio_channel_adk_mask[2]; \
		d3 &= audio_channel_adk_mask[3]; \
		if (mainMenu_soundStereo) { \
			switch (mainMenu_soundStereoSep) { \
				case 3: \
					PUT_SOUND_WORD (d0+d3) \
					PUT_SOUND_WORD (d1+d2) \
					break; \
				case 2: \
					PUT_SOUND_WORD (((d0+d3)*28+(d1+d2)*4)/32) \
					PUT_SOUND_WORD (((d1+d2)*28+(d0+d3)*4)/32) \
					break; \
				case 1: \
					PUT_SOUND_WORD (((d0+d3)*24+(d1+d2)*8)/32) \
					PUT_SOUND_WORD (((d1+d2)*24+(d0+d3)*8)/32) \
					break; \
				case 0: \
					PUT_SOUND_WORD (((d0+d3)*20+(d1+d2)*12)/32) \
					PUT_SOUND_WORD (((d1+d2)*20+(d0+d3)*12)/32) \
					break; \
				default: \
					PUT_SOUND_WORD (d0+d3) \
					PUT_SOUND_WORD (d1+d2) \
					break; } \
		} else { \
			PUT_SOUND_WORD (((d0+d3)*16+(d1+d2)*16)/32) \
			PUT_SOUND_WORD (((d1+d2)*16+(d0+d3)*16)/32) \
		} \
		CHECK_SOUND_BUFFERS(); \
	} \

#else

#define CHECK_SOUND_BUFFERS() \
{ \
    if(mainMenu_soundStereo) { \
      if ((hostptr)sndbufpt - (hostptr)render_sndbuff >= SNDBUFFER_LEN*2) { \
  	    finish_sound_buffer (); } \
    } else { \
      if ((hostptr)sndbufpt - (hostptr)render_sndbuff >= SNDBUFFER_LEN) { \
  	    finish_sound_buffer (); } \
  	 } \
} \

#define SAMPLE_HANDLER \
	{ \
		register uae_u32 d0 = audio_channel_current_sample[0]; \
		register uae_u32 d1 = audio_channel_current_sample[1]; \
		register uae_u32 d2 = audio_channel_current_sample[2]; \
		register uae_u32 d3 = audio_channel_current_sample[3]; \
		d0 *= audio_channel_vol[0]; \
		d1 *= audio_channel_vol[1]; \
		d2 *= audio_channel_vol[2]; \
		d3 *= audio_channel_vol[3]; \
		d0 &= audio_channel_adk_mask[0]; \
		d1 &= audio_channel_adk_mask[1]; \
		d2 &= audio_channel_adk_mask[2]; \
		d3 &= audio_channel_adk_mask[3]; \
		if (mainMenu_soundStereo) { \
			switch (mainMenu_soundStereoSep) { \
				case 3: \
					PUT_SOUND_WORD (d0+d3) \
					PUT_SOUND_WORD (d1+d2) \
					break; \
				case 2: \
					PUT_SOUND_WORD (((d0+d3)*28+(d1+d2)*4)/32) \
					PUT_SOUND_WORD (((d1+d2)*28+(d0+d3)*4)/32) \
					break; \
				case 1: \
					PUT_SOUND_WORD (((d0+d3)*24+(d1+d2)*8)/32) \
					PUT_SOUND_WORD (((d1+d2)*24+(d0+d3)*8)/32) \
					break; \
				case 0: \
					PUT_SOUND_WORD (((d0+d3)*20+(d1+d2)*12)/32) \
					PUT_SOUND_WORD (((d1+d2)*20+(d0+d3)*12)/32) \
					break; \
				default: \
					PUT_SOUND_WORD (d0+d3) \
					PUT_SOUND_WORD (d1+d2) \
					break; } \
		} else { \
			PUT_SOUND_WORD (d0+d1+d2+d3) } \
		CHECK_SOUND_BUFFERS(); \
	} \

#endif

#define SCHEDULE_AUDIO(CHN) \
	if (audio_channel_state[CHN]) { \
	    if (best > audio_channel_evtime[CHN]) { \
		best = audio_channel_evtime[CHN]; \
		eventtab[ev_audio].active = 1; \
	    } \
	}

void schedule_audio (void)
{
#ifdef UNROLL_LOOPS
    unsigned long best = MAX_EV;
    eventtab[ev_audio].active = 0;
    eventtab[ev_audio].oldcycles = get_cycles ();
    SCHEDULE_AUDIO(0)
    SCHEDULE_AUDIO(1)
    SCHEDULE_AUDIO(2)
    SCHEDULE_AUDIO(3)
    eventtab[ev_audio].evtime = get_cycles () + best;
#else
    unsigned long best = MAX_EV;
    int i;

    eventtab[ev_audio].active = 0;
    eventtab[ev_audio].oldcycles = get_cycles ();
    for (i = 0; i < 4; i++) {
	struct audio_channel_data *cdp;
    	SCHEDULE_AUDIO(i)
    }
    eventtab[ev_audio].evtime = get_cycles () + best;
#endif
}

#ifdef UNROLL_AUDIO_HANDLER

#define AUDIO_HANDLER_CASE_1(NR) \
	struct audio_channel_data *cdp = &audio_channel[NR]; \
	audio_channel_evtime[NR] = maxhpos * CYCLE_UNIT; \
	audio_channel_state[NR] = 5; \
	INTREQ((uae_u16)(0x8000 | (0x80 << NR))); \
	if (cdp->wlen != 1) \
	    cdp->wlen = (cdp->wlen - 1) & 0xFFFF; \
	cdp->nextdat = CHIPMEM_WGET_CUSTOM (cdp->pt); \
	cdp->pt += 2;

#define AUDIO_HANDLER_CASE_5(NR) \
	struct audio_channel_data *cdp = &audio_channel[NR]; \
	audio_channel_evtime[NR] = cdp->per; \
	cdp->dat = cdp->nextdat; \
	audio_channel_current_sample[NR] = (sample8_t)(cdp->dat >> 8); \
	audio_channel_state[NR] = 2; \
	{ \
	    int audav = adkcon & (1 << NR); \
	    int audap = adkcon & (16 << NR); \
	    int napnav = (!audav && !audap) || audav; \
	    if (napnav) \
		cdp->data_written = 2; \
	}

#define AUDIO_HANDLER_CASE_2_0(NR) \
	struct audio_channel_data *cdp = &audio_channel[NR]; \
	audio_channel_current_sample[NR] = (sample8_t)(cdp->dat & 0xFF); \
	audio_channel_evtime[NR] = cdp->per; \
	audio_channel_state[NR] = 3; \
	if (adkcon & (0x10 << NR)) { \
	    if (cdp->intreq2 && cdp->dmaen) \
		INTREQ ((uae_u16)(0x8000 | (0x80 << NR))); \
	    cdp->intreq2 = 0; \
	    cdp->dat = cdp->nextdat; \
	    if (cdp->dmaen) \
		cdp->data_written = 2; \
	}

#define AUDIO_HANDLER_CASE_2_1(NR) \
	struct audio_channel_data *cdp = &audio_channel[NR]; \
	audio_channel_current_sample[NR] = (sample8_t)(cdp->dat & 0xFF); \
	audio_channel_evtime[NR] = cdp->per; \
	audio_channel_state[NR] = 3; \
	if (adkcon & (0x10 << NR)) { \
	    if (cdp->intreq2 && cdp->dmaen) \
		INTREQ ((uae_u16)(0x8000 | (0x80 << NR))); \
	    cdp->intreq2 = 0; \
	    cdp->dat = cdp->nextdat; \
	    if (cdp->dmaen) \
		cdp->data_written = 2; \
	    if (cdp->dat == 0) \
	       (cdp+1)->per = PERIOD_MAX; \
	    else if (cdp->dat < maxhpos * CYCLE_UNIT / 2) \
	       (cdp+1)->per = maxhpos * CYCLE_UNIT / 2; \
	    else \
	       (cdp+1)->per = cdp->dat * CYCLE_UNIT; \
	}

#define AUDIO_HANDLER_CASE_3_0(NR) \
	struct audio_channel_data *cdp = &audio_channel[NR]; \
	audio_channel_evtime[NR] = cdp->per; \
	if ((INTREQR() & (0x80 << NR)) && !cdp->dmaen) { \
	    audio_channel_state[NR] = 0; \
	    audio_channel_current_sample[NR] = 0; \
	} else { \
	    int audav = adkcon & (1 << NR); \
	    int audap = adkcon & (16 << NR); \
	    int napnav = (!audav && !audap) || audav; \
	    audio_channel_state[NR] = 2; \
	    if ((cdp->intreq2 && cdp->dmaen && napnav) \
		|| (napnav && !cdp->dmaen)) \
		INTREQ((uae_u16)(0x8000 | (0x80 << NR))); \
	    cdp->intreq2 = 0; \
	    cdp->dat = cdp->nextdat; \
	    audio_channel_current_sample[NR] = (sample8_t)(cdp->dat >> 8); \
	    if (cdp->dmaen && napnav) \
		cdp->data_written = 2; \
	} 

#define AUDIO_HANDLER_CASE_3_1(NR) \
	struct audio_channel_data *cdp = &audio_channel[NR]; \
	audio_channel_evtime[NR] = cdp->per; \
	if ((INTREQR() & (0x80 << NR)) && !cdp->dmaen) { \
	    audio_channel_state[NR] = 0; \
	    audio_channel_current_sample[NR] = 0; \
	} else { \
	    int audav = adkcon & (1 << NR); \
	    int audap = adkcon & (16 << NR); \
	    int napnav = (!audav && !audap) || audav; \
	    audio_channel_state[NR] = 2; \
	    if ((cdp->intreq2 && cdp->dmaen && napnav) \
		|| (napnav && !cdp->dmaen)) \
		INTREQ((uae_u16)(0x8000 | (0x80 << NR))); \
	    cdp->intreq2 = 0; \
	    cdp->dat = cdp->nextdat; \
	    audio_channel_current_sample[NR] = (sample8_t)(cdp->dat >> 8); \
	    if (cdp->dmaen && napnav) \
		cdp->data_written = 2; \
	    if (audav) { \
		    audio_channel_vol[NR+1]= cdp->dat; \
	    } \
	} 

#define AUDIO_HANDLER_OTHER(NR) \
	audio_channel_state[NR] = 0;


static void audio_handler_dummy(void) { }
static void audio0_handler_case_1(void) { AUDIO_HANDLER_CASE_1(0) }
static void audio1_handler_case_1(void) { AUDIO_HANDLER_CASE_1(1) }
static void audio2_handler_case_1(void) { AUDIO_HANDLER_CASE_1(2) }
static void audio3_handler_case_1(void) { AUDIO_HANDLER_CASE_1(3) }
static void audio0_handler_case_2(void) { AUDIO_HANDLER_CASE_2_1(0) }
static void audio1_handler_case_2(void) { AUDIO_HANDLER_CASE_2_1(1) }
static void audio2_handler_case_2(void) { AUDIO_HANDLER_CASE_2_1(2) }
static void audio3_handler_case_2(void) { AUDIO_HANDLER_CASE_2_0(3) }
static void audio0_handler_case_3(void) { AUDIO_HANDLER_CASE_3_1(0) }
static void audio1_handler_case_3(void) { AUDIO_HANDLER_CASE_3_1(1) }
static void audio2_handler_case_3(void) { AUDIO_HANDLER_CASE_3_1(2) }
static void audio3_handler_case_3(void) { AUDIO_HANDLER_CASE_3_0(3) }
static void audio0_handler_case_5(void) { AUDIO_HANDLER_CASE_5(0) }
static void audio1_handler_case_5(void) { AUDIO_HANDLER_CASE_5(1) }
static void audio2_handler_case_5(void) { AUDIO_HANDLER_CASE_5(2) }
static void audio3_handler_case_5(void) { AUDIO_HANDLER_CASE_5(3) }
static void audio0_handler_other(void)  { AUDIO_HANDLER_OTHER(0) }
static void audio1_handler_other(void)  { AUDIO_HANDLER_OTHER(1) }
static void audio2_handler_other(void)  { AUDIO_HANDLER_OTHER(2) }
static void audio3_handler_other(void)  { AUDIO_HANDLER_OTHER(3) }

typedef void (*audio_handler_func)(void);

static audio_handler_func audio0_table[8] UAE4ALL_ALIGN = {
	audio_handler_dummy, audio0_handler_case_1, audio0_handler_case_2, audio0_handler_case_3,
	audio0_handler_other, audio0_handler_case_5, audio0_handler_other, audio0_handler_other
};
static audio_handler_func audio1_table[8] UAE4ALL_ALIGN = {
	audio_handler_dummy, audio1_handler_case_1, audio1_handler_case_2, audio1_handler_case_3,
	audio1_handler_other, audio1_handler_case_5, audio1_handler_other, audio1_handler_other
};
static audio_handler_func audio2_table[8] UAE4ALL_ALIGN = {
	audio_handler_dummy, audio2_handler_case_1, audio2_handler_case_2, audio2_handler_case_3,
	audio2_handler_other, audio2_handler_case_5, audio2_handler_other, audio2_handler_other
};
static audio_handler_func audio3_table[8] UAE4ALL_ALIGN = {
	audio_handler_dummy, audio3_handler_case_1, audio3_handler_case_2, audio3_handler_case_3,
	audio3_handler_other, audio3_handler_case_5, audio3_handler_other, audio3_handler_other
};

#define audio_handler_0() audio0_table[audio_channel_state[0]]()
#define audio_handler_1() audio1_table[audio_channel_state[1]]()
#define audio_handler_2() audio2_table[audio_channel_state[2]]()
#define audio_handler_3() audio3_table[audio_channel_state[3]]()

#else

#define audio_handler_0() audio_handler(0)
#define audio_handler_1() audio_handler(1)
#define audio_handler_2() audio_handler(2)
#define audio_handler_3() audio_handler(3)

static __inline__ void audio_handler (int nr)
{
    struct audio_channel_data *cdp = audio_channel + nr;
    switch (audio_channel_state[nr]) {
     case 0:
	write_log ("Bug in sound code\n");
	break;
     case 1:
	audio_channel_evtime[nr] = maxhpos * CYCLE_UNIT;
	audio_channel_state[nr] = 5;
	INTREQ(0x8000 | (0x80 << nr));
	if (cdp->wlen != 1)
	    cdp->wlen = (cdp->wlen - 1) & 0xFFFF;
	cdp->nextdat = CHIPMEM_WGET_CUSTOM (cdp->pt);
	cdp->pt += 2;
	break;
     case 5:
	audio_channel_evtime[nr] = cdp->per;
	cdp->dat = cdp->nextdat;
	cdp->last_sample = audio_channel_current_sample[nr];
	audio_channel_current_sample[nr] = (sample8_t)(cdp->dat >> 8);
	audio_channel_state[nr] = 2;
	{
	    int audav = adkcon & (1 << nr);
	    int audap = adkcon & (16 << nr);
	    int napnav = (!audav && !audap) || audav;
	    if (napnav)
		cdp->data_written = 2;
	}
	break;
     case 2:
	cdp->last_sample = audio_channel_current_sample[nr];
	audio_channel_current_sample[nr] = (sample8_t)(cdp->dat & 0xFF);
	audio_channel_evtime[nr] = cdp->per;
	audio_channel_state[nr] = 3;
	if (adkcon & (0x10 << nr)) {
	    if (cdp->intreq2 && cdp->dmaen)
		INTREQ (0x8000 | (0x80 << nr));
	    cdp->intreq2 = 0;
	    cdp->dat = cdp->nextdat;
	    if (cdp->dmaen)
		cdp->data_written = 2;
	    if (nr < 3) {
		if (cdp->dat == 0)
		    (cdp+1)->per = PERIOD_MAX;
		else if (produce_sound<3 && cdp->dat < maxhpos * CYCLE_UNIT / 2)
		    (cdp+1)->per = maxhpos * CYCLE_UNIT / 2;
		else
		    (cdp+1)->per = cdp->dat * CYCLE_UNIT;
	    }
	}
	break;
     case 3:
	audio_channel_evtime[nr] = cdp->per;
	if ((INTREQR() & (0x80 << nr)) && !cdp->dmaen) {
	    audio_channel_state[nr] = 0;
	    cdp->last_sample = 0;
	    audio_channel_current_sample[nr] = 0;
	    break;
	} else {
	    int audav = adkcon & (1 << nr);
	    int audap = adkcon & (16 << nr);
	    int napnav = (!audav && !audap) || audav;
	    audio_channel_state[nr] = 2;
	    if ((cdp->intreq2 && cdp->dmaen && napnav)
		|| (napnav && !cdp->dmaen))
		INTREQ(0x8000 | (0x80 << nr));
	    cdp->intreq2 = 0;
	    cdp->dat = cdp->nextdat;
	    cdp->last_sample = audio_channel_current_sample[nr];
	    audio_channel_current_sample[nr] = (sample8_t)(cdp->dat >> 8);
	    if (cdp->dmaen && napnav)
		cdp->data_written = 2;
	    if (audav) {
		if (nr < 3) {
		    audio_channel_vol[nr+1] = cdp->dat;
		}
	    }
	}
	break;
     default:
	audio_channel_state[nr] = 0;
	break;
    }
}

#endif


void aud0_handler(void) {
	audio_handler_0();
}

void aud1_handler(void) {
	audio_handler_1();
}

void aud2_handler (void) {
	audio_handler_2();
}

void aud3_handler (void) {
	audio_handler_3();
}


#define audio_channel_enable_dma_1(NCHAN) \
{ \
    struct audio_channel_data *cdp = &audio_channel[NCHAN]; \
    if (audio_channel_state[NCHAN] == 0) { \
	audio_channel_state[NCHAN] = 1; \
	cdp->pt = cdp->lc; \
	cdp->wlen = cdp->len; \
	cdp->data_written = 2; \
	audio_channel_evtime[NCHAN] = eventtab[ev_hsync].evtime - get_cycles (); \
    } \
}

void audio_channel_enable_dma(int n_channel)
{
	audio_channel_enable_dma_1(n_channel)
}

#define audio_channel_disable_dma_1(NCHAN) \
{ \
    if (audio_channel_state[NCHAN] == 1 || audio_channel_state[NCHAN] == 5) { \
	audio_channel_state[NCHAN] = 0; \
	audio_channel_current_sample[NCHAN] = 0; \
    } \
}

void audio_channel_disable_dma(int n_channel)
{
	audio_channel_disable_dma_1(n_channel)
}

void audio_reset (void)
{
    int i;

  if (savestate_state != STATE_RESTORE)
  {
  	memset (audio_channel, 0, 4 * sizeof *audio_channel);
  	audio_channel[0].per = PERIOD_MAX;
  	audio_channel[1].per = PERIOD_MAX;
  	audio_channel[2].per = PERIOD_MAX;
  	audio_channel[3].per = PERIOD_MAX;
  }
  else
	    for (i = 0; i < 4; i++)
		    audio_channel[i].dmaen = (dmacon & 0x200) && (dmacon & (1 << i));

    last_cycles = 0;
    next_sample_evtime = scaled_sample_evtime;

    schedule_audio ();
}

static __inline__ int sound_prefs_changed (void)
{
    return (changed_produce_sound != produce_sound);
}

void check_prefs_changed_audio (void)
{
    if (sound_available && sound_prefs_changed ()) {
	close_sound ();

	produce_sound = changed_produce_sound;
	if (produce_sound) {
	    if (init_audio ()) {
		last_cycles = get_cycles () - 1;
		next_sample_evtime = scaled_sample_evtime;
	    } else
		if (! sound_available) {
		    write_log ("Sound is not supported.\n");
		} else {
		    write_log ("Sorry, can't initialize sound.\n");
		    produce_sound = 0;
		    /* So we don't do this every frame */
		    produce_sound = 0;
		}
	}
    }
    if (!produce_sound) {
	eventtab[ev_audio].active = 0;
	events_schedule ();
    }
}

#define STATE0 audio_channel_state[0]
#define STATE1 audio_channel_state[1]
#define STATE2 audio_channel_state[2]
#define STATE3 audio_channel_state[3]

#define EVTIME0 audio_channel_evtime[0]
#define EVTIME1 audio_channel_evtime[1]
#define EVTIME2 audio_channel_evtime[2]
#define EVTIME3 audio_channel_evtime[3]


#define DEFINE_STATE \
	register unsigned long int best_evtime = n_cycles + 1; \


#define CHECK_STATE \
	if (STATE0 && best_evtime > EVTIME0) \
	    best_evtime = EVTIME0; \
	if (STATE1 && best_evtime > EVTIME1) \
	    best_evtime = EVTIME1; \
	if (STATE2 && best_evtime > EVTIME2) \
	    best_evtime = EVTIME2; \
	if (STATE3 && best_evtime > EVTIME3) \
	    best_evtime = EVTIME3; \
	if (best_evtime > next_sample_evtime) \
	    best_evtime = next_sample_evtime; \
	if (best_evtime > n_cycles) \
	    break; \


#define SUB_EVTIME \
	next_sample_evtime -= best_evtime; \
	EVTIME0 -= best_evtime; \
	EVTIME1 -= best_evtime; \
	EVTIME2 -= best_evtime; \
	EVTIME3 -= best_evtime; \
	n_cycles -= best_evtime; \


#define IF_SAMPLE \
	if (!next_sample_evtime) { \
		next_sample_evtime = scaled_sample_evtime; \
		if (produce_sound >= 2) { \
			SAMPLE_HANDLER \
		} \
	} \


#define RUN_HANDLERS \
	if (!EVTIME0 && STATE0) \
	    audio_handler_0(); \
	if (!EVTIME1 && STATE1) \
	    audio_handler_1(); \
	if (!EVTIME2 && STATE2) \
	    audio_handler_2(); \
	if (!EVTIME3 && STATE3) \
	    audio_handler_3(); \


void update_audio (void)
{
    unsigned long int n_cycles;

    n_cycles = get_cycles () - last_cycles;
	for (;;) {
		DEFINE_STATE
		CHECK_STATE
		SUB_EVTIME
		IF_SAMPLE
		RUN_HANDLERS
	}

	last_cycles = get_cycles () - n_cycles;
}

void audio_evhandler (void)
{
	if (produce_sound)
	{
		update_audio ();
		schedule_audio ();
	}
	else
		eventtab[ev_audio].evtime = get_cycles () + (MAX_EV);
}

void AUDxDAT (int nr, uae_u16 v)
{
    struct audio_channel_data *cdp = audio_channel + nr;

    if (produce_sound)
    	update_audio ();

    cdp->dat = v;
    if (audio_channel_state[nr] == 0 && !(INTREQR() & (0x80 << nr))) {
	audio_channel_state[nr] = 2;
	INTREQ((uae_u16)(0x8000 | (0x80 << nr)));
	/* data_written = 2 ???? */
	audio_channel_evtime[nr] = cdp->per;
	schedule_audio ();
	events_schedule ();
    }
}

void AUDxLCH (int nr, uae_u16 v)
{
    if (produce_sound)
    	update_audio ();

    audio_channel[nr].lc = (audio_channel[nr].lc & 0xffff) | ((uae_u32)v << 16);
}

void AUDxLCL (int nr, uae_u16 v)
{
    if (produce_sound)
    	update_audio ();

    audio_channel[nr].lc = (audio_channel[nr].lc & ~0xffff) | (v & 0xFFFE);
}

#define MIN_ALLOWED_PERIOD 16
void AUDxPER (int nr, uae_u16 v)
{
    unsigned long per = v * CYCLE_UNIT;

    if (produce_sound)
    	update_audio ();

    if (per == 0)
	per = PERIOD_MAX;

    if (produce_sound<3 && per < maxhpos * CYCLE_UNIT / 2)
	per = maxhpos * CYCLE_UNIT / 2;

    if (produce_sound == 3 && per < MIN_ALLOWED_PERIOD * CYCLE_UNIT)
	per = MIN_ALLOWED_PERIOD * CYCLE_UNIT;

    if (audio_channel[nr].per == PERIOD_MAX
	&& per != PERIOD_MAX)
    {
	audio_channel_evtime[nr] = CYCLE_UNIT;
	if (produce_sound > 0) {
	    schedule_audio ();
	    events_schedule ();
	}
    }
    audio_channel[nr].per = per;
}

void AUDxLEN (int nr, uae_u16 v)
{
    if (produce_sound)
    	update_audio ();
    audio_channel[nr].len = v;
}

void AUDxVOL (int nr, uae_u16 v)
{
    int v2 = v & 64 ? 63 : v & 63;

    if (produce_sound)
    	update_audio ();

    audio_channel_vol[nr] = v2;
}

int init_audio (void)
{
    int retval;
    /* Some backward compatibility hacks until every port initializes
       scaled_sample_evtime...  */
    scaled_sample_evtime_ok = 0;
    retval = init_sound ();
    if (! scaled_sample_evtime_ok)
	scaled_sample_evtime = sample_evtime * CYCLE_UNIT;
    return retval;
}

#define CHECK_DMA_AUDIO(AUDIOCH); \
	cdp = &audio_channel[AUDIOCH]; \
	chan_ena = (dmacon & 0x200) && (dmacon & (1<<AUDIOCH)); \
	if (cdp->dmaen != chan_ena) \
	{ \
		cdp->dmaen = chan_ena; \
		if (cdp->dmaen) \
			audio_channel_enable_dma_1 (AUDIOCH) \
		else \
			audio_channel_disable_dma_1 (AUDIOCH) \
	}

void check_dma_audio(void)
{
#ifdef UNROLL_LOOPS
	struct audio_channel_data *cdp;
	int chan_ena;
	CHECK_DMA_AUDIO(0);
	CHECK_DMA_AUDIO(1);
	CHECK_DMA_AUDIO(2);
	CHECK_DMA_AUDIO(3);
#else
	int i;
	for(i=0;i<4;i++)
	{
		struct audio_channel_data *cdp;
		int chan_ena;
		CHECK_DMA_AUDIO(i)
	}
#endif
}


#define FETCH_AUDIO(AUDIOCH) \
	cdp = &audio_channel[AUDIOCH]; \
	if (cdp->data_written == 2) { \
		cdp->data_written = 0; \
		cdp->nextdat = CHIPMEM_WGET_CUSTOM (cdp->pt); \
		cdp->pt += 2; \
		if (audio_channel_state[AUDIOCH] == 2 || audio_channel_state[AUDIOCH] == 3) { \
			if (cdp->wlen == 1) { \
				cdp->pt = cdp->lc; \
				cdp->wlen = cdp->len; \
				cdp->intreq2 = 1; \
			} else \
				cdp->wlen = (cdp->wlen - 1) & 0xFFFF; \
		} \
	} 


void fetch_audio(void)
{
#ifdef UNROLL_LOOPS
	struct audio_channel_data *cdp;
	FETCH_AUDIO(0)
	FETCH_AUDIO(1)
	FETCH_AUDIO(2)
	FETCH_AUDIO(3)
#else
	int i;
	for(i=0;i<4;i++)
	{
		struct audio_channel_data *cdp;
		FETCH_AUDIO(i)
	}
#endif
}

void update_adkmasks (void)
{
    unsigned long t;

    t = adkcon | (adkcon >> 4);
    audio_channel_adk_mask[0] = (((t >> 0) & 1) - 1);
    audio_channel_adk_mask[1] = (((t >> 1) & 1) - 1);
    audio_channel_adk_mask[2] = (((t >> 2) & 1) - 1);
    audio_channel_adk_mask[3] = (((t >> 3) & 1) - 1);
}

uae_u8 *restore_audio (uae_u8 *src, int i)
{
    struct audio_channel_data *acd;
    uae_u16 backper;

    acd = audio_channel + i;
    audio_channel_state[i]=restore_u8 ();
    audio_channel_vol[i]=restore_u8 ();
    acd->intreq2 = restore_u8 ();
    acd->data_written = restore_u8 ();
    acd->len = restore_u16 ();
    acd->wlen = restore_u16 ();
    backper = restore_u16 ();
    restore_u16 (); // wper unused -> removed
    acd->lc = restore_u32 ();
    acd->pt = restore_u32 ();
    audio_channel_evtime[i] = restore_u32 ();
    //AUDxPER(i,backper ? backper * CYCLE_UNIT : PERIOD_MAX);
    acd->per = backper == 0 ? PERIOD_MAX : backper * CYCLE_UNIT;
    audio_channel[i].dmaen = (dmacon & 0x200) && (dmacon & (1 << i));
    //AUDxDAT(i,0);
		acd->dat = 0;

    return src;
}


uae_u8 *save_audio (int *len, int i)
{
    struct audio_channel_data *acd;
    uae_u8 *dst = (uae_u8 *)malloc (100);
    uae_u8 *dstbak = dst;
    uae_u16 p;

    acd = audio_channel + i;
    save_u8 (audio_channel_state[i]);
    save_u8 (audio_channel_vol[i]);
    save_u8 (acd->intreq2);
    save_u8 (acd->data_written);
    save_u16 (acd->len);
    save_u16 (acd->wlen);
    p = acd->per == PERIOD_MAX ? 0 : acd->per / CYCLE_UNIT;
    save_u16 (p);
    save_u16 (0); // wper unused -> removed
    save_u32 (acd->lc);
    save_u32 (acd->pt);
    save_u32 (audio_channel_evtime[i]);
    
    *len = dst - dstbak;
    return dstbak;
}
