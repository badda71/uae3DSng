/*
 * uae3DSng - read-ahead cache for hard files (no emulator or 3DS dependencies, so it can be tested on a PC).
 *
 * The Amiga reads a hard file in requests of one 512 byte block (the ROM's filesystem asks for the blocks of a file
 * one after the other). Every such request used to be a fseek() + fread() of 512 bytes on the SD card, which costs
 * milliseconds on the 3DS: loading 5 MB took about 37 s, most of it spent in the file service, not in the emulation.
 *
 * The cache reads ahead: a miss reads a window from the file into RAM and the following requests are served from it.
 * The window is small (HFCACHE_MIN) after a jump, because the directory and header blocks are read here and there,
 * and doubles with every miss that continues right where the last request ended, up to HFCACHE_MAX (file data).
 * Reads and writes of the same hard file go through the same FILE*; a write that touches the window drops it.
 */
#ifndef HFCACHE_H
#define HFCACHE_H

#include <stdio.h>

#define HFCACHE_MIN  4096u
#define HFCACHE_MAX  (64u * 1024u)

struct HfCache;

/* NULL when out of memory */
struct HfCache *hfcache_new(void);
void hfcache_free(struct HfCache *c);

/* A pointer to the data of the file at offset pos (the cache is filled from fd when it does not hold pos). *avail = the
   number of bytes that can be read from the pointer (at least 1), at most up to the end of the window; the pointer is
   valid until the next call. NULL at/after the end of the file or on a read error (pos >= file_size is always an error). */
const unsigned char *hfcache_get(struct HfCache *c, FILE *fd, unsigned long file_size, unsigned long pos, unsigned *avail);

/* A write of len bytes at offset: the cached data of that range is no longer valid */
void hfcache_written(struct HfCache *c, unsigned long offset, unsigned long len);

/* Statistics (the host test uses them) */
unsigned long hfcache_fills(const struct HfCache *c);          /* reads from the file so far */
unsigned long hfcache_filled_bytes(const struct HfCache *c);   /* bytes read from the file so far */

#endif
