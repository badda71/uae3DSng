// Host test of the hard file read-ahead cache (src/3ds/hfcache.cpp): the cache must return exactly what the file holds,
// whatever mix of reads and writes the Amiga does, and it must save reads from the file when the access is sequential.
#include "hfcache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vector>

static int checks = 0, fails = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static unsigned long rng_state = 12345;
static unsigned rnd(unsigned n) { rng_state = rng_state * 6364136223846793005ull + 1442695040888963407ull; return (unsigned)((rng_state >> 33) % n); }

// what the hard file code does for a read request of len bytes (multiple of 512) at offset: fill dst, return false on error
static bool hf_read(HfCache *c, FILE *fd, unsigned long size, unsigned long offset, unsigned long len, unsigned char *dst)
{
    bool ok = true;
    while (len) {
        unsigned avail, n;
        const unsigned char *p = hfcache_get(c, fd, size, offset, &avail);
        if (p) {
            n = avail < len ? avail : (unsigned)len;
            memcpy(dst, p, n);
        } else {
            n = 512 - (offset & 511);
            if (n > len) n = (unsigned)len;
            memset(dst, 0, n);
            ok = false;
        }
        dst += n; offset += n; len -= n;
    }
    return ok;
}

static void hf_write(HfCache *c, FILE *fd, unsigned long offset, const unsigned char *src, unsigned long len)
{
    hfcache_written(c, offset, len);
    fseek(fd, (long)offset, SEEK_SET);
    fwrite(src, 1, len, fd);
}

static FILE *make_file(const char *path, std::vector<unsigned char> &mirror, unsigned long size)
{
    mirror.resize(size);
    for (unsigned long i = 0; i < size; i++) mirror[i] = (unsigned char)rnd(256);
    FILE *f = fopen(path, "w+b");
    fwrite(mirror.data(), 1, size, f);
    fflush(f);
    return f;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "/tmp";
    char path[512];
    snprintf(path, sizeof path, "%s/hfcache_test.bin", dir);

    // 1. a sequential read of 11,606 blocks (the loading phase of Elvira 2 in the log) reads the file in few large pieces
    {
        std::vector<unsigned char> mirror;
        unsigned long size = 6u * 1024 * 1024;
        FILE *f = make_file(path, mirror, size);
        HfCache *c = hfcache_new();
        CHECK(c != NULL);
        unsigned char buf[512];
        bool ok = true;
        for (unsigned long off = 0; off < 11606ul * 512; off += 512) {
            ok &= hf_read(c, f, size, off, 512, buf);
            if (memcmp(buf, &mirror[off], 512) != 0) { CHECK(!"sequential data"); break; }
        }
        CHECK(ok);
        printf("sequential 11606 x 512 B: %lu reads from the file (%lu bytes)\n", hfcache_fills(c), hfcache_filled_bytes(c));
        CHECK(hfcache_fills(c) < 120);
        CHECK(hfcache_filled_bytes(c) >= 11606ul * 512 && hfcache_filled_bytes(c) <= 11606ul * 512 + HFCACHE_MAX);
        hfcache_free(c);
        fclose(f);
    }

    // 2. random reads of every size, mixed with writes, compared with a copy in RAM
    for (int round = 0; round < 6; round++) {
        std::vector<unsigned char> mirror;
        unsigned long size = 512ul * (1 + rnd(round < 3 ? 40 : 2000));       // from one block to ~1 MB
        FILE *f = make_file(path, mirror, size);
        HfCache *c = hfcache_new();
        std::vector<unsigned char> buf(size + 512);
        bool all_ok = true, data_ok = true;
        for (int i = 0; i < 20000; i++) {
            unsigned blocks = (unsigned)(size / 512);
            unsigned long off = 512ul * rnd(blocks);
            unsigned long maxb = blocks - off / 512;
            unsigned long len = 512ul * (1 + (rnd(8) == 0 ? rnd((unsigned)maxb) : rnd(maxb < 16 ? (unsigned)maxb : 16)));
            if (len > size - off) len = size - off;
            unsigned what = rnd(10);
            if (what == 0) {                                        // write
                for (unsigned long k = 0; k < len; k++) buf[k] = (unsigned char)rnd(256);
                hf_write(c, f, off, buf.data(), len);
                memcpy(&mirror[off], buf.data(), len);
            } else if (what < 5) {                                  // sequential: continue behind the previous read
                static unsigned long next = 0;
                off = next < size ? next : 0;
                len = 512;
                all_ok &= hf_read(c, f, size, off, len, buf.data());
                data_ok &= memcmp(buf.data(), &mirror[off], len) == 0;
                next = off + len;
            } else {
                all_ok &= hf_read(c, f, size, off, len, buf.data());
                data_ok &= memcmp(buf.data(), &mirror[off], len) == 0;
            }
        }
        CHECK(all_ok);
        CHECK(data_ok);
        // the file itself holds what the Amiga wrote
        fflush(f);
        std::vector<unsigned char> back(size);
        fseek(f, 0, SEEK_SET);
        CHECK(fread(back.data(), 1, size, f) == size);
        CHECK(back == mirror);
        hfcache_free(c);
        fclose(f);
    }

    // 3. a write must drop exactly what it overlaps: read, write over the window's end, read again
    {
        std::vector<unsigned char> mirror;
        unsigned long size = 256 * 1024;
        FILE *f = make_file(path, mirror, size);
        HfCache *c = hfcache_new();
        unsigned char buf[1024];
        CHECK(hf_read(c, f, size, 4096, 512, buf));                                 // window 4096..8191
        unsigned char blk[512]; memset(blk, 0xA5, sizeof blk);
        hf_write(c, f, 8192, blk, 512);                                             // behind the window: it stays
        unsigned long fills = hfcache_fills(c);
        CHECK(hf_read(c, f, size, 5120, 512, buf));
        CHECK(hfcache_fills(c) == fills);
        hf_write(c, f, 8191 - 511, blk, 512);                                       // the last block of the window
        memcpy(&mirror[8191 - 511], blk, 512);
        memcpy(&mirror[8192], blk, 512);
        CHECK(hf_read(c, f, size, 8191 - 511, 1024, buf));
        CHECK(memcmp(buf, &mirror[8191 - 511], 1024) == 0);
        hf_write(c, f, 0, blk, 512);                                                // in front of the window: it stays
        hfcache_free(c);
        fclose(f);
    }

    // 4. the end of the file, a read that cannot be done, and no cache at all
    {
        std::vector<unsigned char> mirror;
        unsigned long size = 3 * 512;
        FILE *f = make_file(path, mirror, size);
        HfCache *c = hfcache_new();
        unsigned avail = 0;
        const unsigned char *p = hfcache_get(c, f, size, 1024, &avail);
        CHECK(p && avail == 512 && memcmp(p, &mirror[1024], 512) == 0);
        CHECK(hfcache_get(c, f, size, 1536, &avail) == NULL);                       // behind the end
        CHECK(hfcache_get(c, f, size, 0, &avail) != NULL && avail == size);         // a window never goes past the end
        CHECK(hfcache_get(NULL, f, size, 0, &avail) == NULL);
        CHECK(hfcache_get(c, NULL, size, 0, &avail) == NULL);
        hfcache_free(c);
        fclose(f);
        // a file that cannot be read (opened for writing only)
        FILE *w = fopen(path, "wb");
        c = hfcache_new();
        std::vector<unsigned char> buf(1024, 0xEE);
        CHECK(!hf_read(c, w, size, 0, 1024, buf.data()));
        CHECK(buf[0] == 0 && buf[1023] == 0);                                       // zero, not what was in the buffer
        hfcache_free(c);
        fclose(w);
        hfcache_free(NULL);
        hfcache_written(NULL, 0, 512);
    }

    unlink(path);
    printf("hfcache_host_test: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
