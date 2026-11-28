/*
 * uae3DSng - read-ahead cache for hard files. See hfcache.h.
 */
#include "hfcache.h"

#include <stdlib.h>

struct HfCache {
    unsigned char *buf;         /* HFCACHE_MAX bytes */
    unsigned long start;        /* file offset of buf[0] */
    unsigned len;               /* valid bytes in buf (0 = empty) */
    unsigned window;            /* size of the next read when the access is sequential */
    unsigned long fills, filled_bytes;
};

struct HfCache *hfcache_new(void)
{
    struct HfCache *c = (struct HfCache *)calloc(1, sizeof(*c));
    if (!c)
        return NULL;
    c->buf = (unsigned char *)malloc(HFCACHE_MAX);
    if (!c->buf) {
        free(c);
        return NULL;
    }
    c->window = HFCACHE_MIN;
    return c;
}

void hfcache_free(struct HfCache *c)
{
    if (!c)
        return;
    free(c->buf);
    free(c);
}

const unsigned char *hfcache_get(struct HfCache *c, FILE *fd, unsigned long file_size, unsigned long pos, unsigned *avail)
{
    if (!c || !fd || pos >= file_size)
        return NULL;
    if (c->len == 0 || pos < c->start || pos >= c->start + c->len) {
        /* miss: read a window that starts at pos */
        int sequential = c->len != 0 && pos == c->start + c->len;     /* continues right behind the window */
        unsigned want;
        if (sequential) {
            want = c->window < HFCACHE_MAX ? c->window * 2 : HFCACHE_MAX;
            if (want > HFCACHE_MAX)
                want = HFCACHE_MAX;
        } else {
            want = HFCACHE_MIN;
        }
        c->window = want;
        if ((unsigned long)want > file_size - pos)
            want = (unsigned)(file_size - pos);
        c->len = 0;                                 /* the window is gone, whatever the read does */
        if (fseek(fd, (long)pos, SEEK_SET) != 0)
            return NULL;
        size_t n = fread(c->buf, 1, want, fd);
        if (n == 0)
            return NULL;
        c->start = pos;
        c->len = (unsigned)n;
        c->fills++;
        c->filled_bytes += n;
    }
    unsigned off = (unsigned)(pos - c->start);
    *avail = c->len - off;
    return c->buf + off;
}

void hfcache_written(struct HfCache *c, unsigned long offset, unsigned long len)
{
    if (!c || c->len == 0)
        return;
    if (offset < c->start + c->len && offset + len > c->start)
        c->len = 0;
}

unsigned long hfcache_fills(const struct HfCache *c) { return c ? c->fills : 0; }
unsigned long hfcache_filled_bytes(const struct HfCache *c) { return c ? c->filled_bytes : 0; }
