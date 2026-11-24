/*
 * uae3DSng - crash-safe debug log on the SD card (sdmc:/3ds/uae3DSng/uae3DSng.log)
 *
 * Every line is flushed (fflush + fsync) before returning, so the last line survives a freeze/hard reset
 * and shows where the emulator stopped. stdout/stderr (printf from the core and the SDL fork) go to
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

#ifdef __cplusplus
}
#endif
#endif
