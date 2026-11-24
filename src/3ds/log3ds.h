/*
 * uae3DSng - crash-safe debug log on the SD card (sdmc:/3ds/uae3DSng/uae3DSng.log)
 *
 * Every line is written (fflush) before returning. Outside of emulation (start-up, menus, shutdown) it is also
 * fsync'ed, so the last line survives a freeze/hard reset and shows where the emulator stopped. While frames are
 * being emulated ("lazy" mode) the fsync - which costs 10..90 ms on the SD card - is deferred: log3ds_tick() does it
 * at most every 2 s, inside the frame limiter's idle time. stdout/stderr (printf from the core and the SDL fork) go to
 * stdout.log in the same folder. write_log() is mapped to log3ds() on 3DS (see include/sysdeps.h).
 */
#ifndef LOG3DS_H
#define LOG3DS_H

#ifdef __cplusplus
extern "C" {
#endif

void log3ds(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void log3ds_mem(const char *tag);   /* free application memory / linear heap / malloc usage */
unsigned log3ds_linear_free_kb(void);   /* free linear memory (textures, audio, bottom-screen images), in KB */

void log3ds_lazy(int on);                /* 1: do not fsync every line (emulation running); 0: sync now and from here on */
void log3ds_tick(unsigned slack_us);     /* once per emulated frame: enters lazy mode, syncs pending lines when due */

#ifdef __cplusplus
}
#endif
#endif
