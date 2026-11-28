/*
 * uae3DSng - writer for Amiga OFS ("DOS\0") hard disk images. See ofs_image.h.
 *
 * Block layouts (512-byte blocks, 128 big-endian longs, negative indices count from the end of the block):
 *   boot block 0/1   0: 'DOS\0'   1: checksum (carry-around sum, inverted)   2: root block number
 *   root block       0: 2 (T_HEADER)  3: 72 (hash table size)  5: checksum  6..77: hash table  -50: bitmap valid (-1)
 *                    -49..-25: bitmap block numbers  -24: bitmap extension  -23..-21: last change  -20: name (BCPL)
 *                    -10..-8: last disk change  -7..-5: creation  -1: 1 (ST_ROOT)
 *   folder block     0: 2  1: own number  5: checksum  6..77: hash table  -48: protection  -23..-21: date  -20: name
 *                    -4: next entry with the same hash  -3: parent  -1: 2 (ST_USERDIR)
 *   file header      0: 2  1: own number  2: number of data block pointers here (max 72)  4: first data block
 *                    5: checksum  -51-i: pointer to the (i+1)th data block  -48: protection  -47: size in bytes
 *                    -23..-21: date  -20: name  -4: hash chain  -3: parent  -2: first extension block  -1: -3 (ST_FILE)
 *   extension block  0: 16 (T_LIST)  1: own number  2: pointer count (max 72)  5: checksum  -51-i: data pointers
 *                    -3: file header  -2: next extension block  -1: -3
 *   OFS data block   0: 8 (T_DATA)  1: file header  2: sequence number (1..)  3: bytes in this block (max 488)
 *                    4: next data block (0 = last)  5: checksum  6..: data (488 bytes)
 *   bitmap block     0: checksum  1..127: one bit per block, 1 = free, bit k of long g = block 2 + 32 g + k
 *   bitmap extension -: 127 bitmap block numbers (no checksum), last long = next extension block
 * Checksum rule (all but the boot block): the sum of all 128 longs including the checksum is 0.
 */
#include "ofs_image.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BS              512
#define LONGS           128
#define PAYLOAD         488                 /* data bytes in one OFS data block */
#define HT_SIZE         72
#define PTRS_PER_BLOCK  72                  /* data block pointers in a file header / extension block */
#define BM_PER_BLOCK    127                 /* longs of bitmap per bitmap block = 4064 blocks */
#define BM_IN_ROOT      25
#define OUT_BLOCKS      128                 /* write buffer: 64 KB */

typedef struct {
    char name[OFS_NAME_MAX + 1];
    uint8_t len, is_dir;
    int32_t parent;                         /* entry id, -1 = root */
    uint32_t size;
    uint32_t days, mins, ticks;
    uint32_t hdr_blk;                       /* layout */
    uint32_t data_first;                    /* index of the first data block (all data blocks are numbered in order) */
    uint32_t n_data, n_ext, ext_first;
} OfsEntry;

struct OfsImage {
    char volname[OFS_NAME_MAX + 1];
    OfsEntry *e;
    uint32_t n, cap;
    int32_t *tab;                           /* name lookup: entry id + 1, 0 = empty (open addressing) */
    uint32_t tab_cap;

    int laid_out;
    uint32_t N, R, M, B, Bext, D;           /* blocks in the image, root block, metadata blocks, bitmap blocks,
                                               bitmap extension blocks, data blocks */
    uint8_t *meta;                          /* M blocks: root, bitmap, extensions, folder and file headers; at block R */
    uint32_t days, mins, ticks;             /* "now", for the root block */

    FILE *f;
    int failed, finished;
    uint32_t pos;                           /* next block to be written */
    uint8_t *out;
    uint32_t out_used;
    uint32_t next_d;                        /* next data block to be written */
    int cur;                                /* file being written */
    uint32_t cur_written, cur_seq, part_len;
    uint8_t part[PAYLOAD];
};

/* ---------------------------------------------------------------------------------------------------------------- */

static void seterr(char *err, size_t err_size, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static void seterr(char *err, size_t err_size, const char *fmt, ...)
{
    if (!err || err_size == 0)
        return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err, err_size, fmt, ap);
    va_end(ap);
}

static inline uint32_t get32(const uint8_t *b, int idx)
{
    if (idx < 0)
        idx += LONGS;
    const uint8_t *p = b + idx * 4;
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static inline void put32(uint8_t *b, int idx, uint32_t v)
{
    if (idx < 0)
        idx += LONGS;
    uint8_t *p = b + idx * 4;
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static void put_bstr(uint8_t *b, int idx, const char *s, size_t n)   /* BCPL string: length byte + characters */
{
    if (idx < 0)
        idx += LONGS;
    b[idx * 4] = (uint8_t)n;
    memcpy(b + idx * 4 + 1, s, n);
}

static void set_checksum(uint8_t *b, int chk_idx)
{
    uint32_t sum = 0;
    put32(b, chk_idx, 0);
    for (int i = 0; i < LONGS; i++)
        sum += get32(b, i);
    put32(b, chk_idx, 0u - sum);
}

static inline unsigned upper(unsigned char c)
{
    return (c >= 'a' && c <= 'z') ? (unsigned)(c - 32) : c;
}

unsigned ofs_name_hash(const char *name, size_t len)
{
    uint32_t h = (uint32_t)len;
    for (size_t i = 0; i < len; i++)
        h = (h * 13 + upper((unsigned char)name[i])) & 0x7ff;
    return h % HT_SIZE;
}

uint32_t ofs_unix_to_days(long unix_time, uint32_t *mins, uint32_t *ticks)
{
    const int64_t epoch = 252460800;        /* 1978-01-01 00:00:00 UTC, the Amiga epoch */
    int64_t t = unix_time;
    if (t <= 0)
        t = (int64_t)time(NULL);
    if (t < epoch)
        t = epoch;
    int64_t s = t - epoch;
    if (mins)
        *mins = (uint32_t)((s % 86400) / 60);
    if (ticks)
        *ticks = (uint32_t)((s % 60) * 50);
    return (uint32_t)(s / 86400);
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* describing the contents                                                                                          */
/* ---------------------------------------------------------------------------------------------------------------- */

OfsImage *ofs_new(const char *volume_name)
{
    OfsImage *img = (OfsImage *)calloc(1, sizeof(OfsImage));
    if (!img)
        return NULL;
    size_t n = volume_name ? strlen(volume_name) : 0;
    if (n > OFS_NAME_MAX)
        n = OFS_NAME_MAX;
    if (n == 0) {
        memcpy(img->volname, "Volume", 7);
    } else {
        for (size_t i = 0; i < n; i++) {
            char c = volume_name[i];
            img->volname[i] = (c == ':' || c == '/') ? '_' : c;
        }
    }
    img->cur = -1;
    img->days = ofs_unix_to_days(0, &img->mins, &img->ticks);
    return img;
}

void ofs_abort(OfsImage *img)
{
    if (img && img->f) {
        fclose(img->f);
        img->f = NULL;
    }
    if (img)
        img->failed = 1;
}

void ofs_free(OfsImage *img)
{
    if (!img)
        return;
    if (img->f)
        fclose(img->f);
    free(img->e);
    free(img->tab);
    free(img->meta);
    free(img->out);
    free(img);
}

static uint32_t key_hash(int32_t parent, const char *name, size_t len)
{
    uint32_t h = 2166136261u ^ (uint32_t)(parent + 1);
    h *= 16777619u;
    for (size_t i = 0; i < len; i++) {
        h ^= upper((unsigned char)name[i]);
        h *= 16777619u;
    }
    return h;
}

static int names_equal(const char *a, size_t na, const char *b, size_t nb)
{
    if (na != nb)
        return 0;
    for (size_t i = 0; i < na; i++)
        if (upper((unsigned char)a[i]) != upper((unsigned char)b[i]))
            return 0;
    return 1;
}

static int find_child(const OfsImage *img, int32_t parent, const char *name, size_t len)
{
    if (img->tab_cap == 0)
        return -1;
    uint32_t i = key_hash(parent, name, len) & (img->tab_cap - 1);
    while (img->tab[i]) {
        const OfsEntry *c = &img->e[img->tab[i] - 1];
        if (c->parent == parent && names_equal(c->name, c->len, name, len))
            return img->tab[i] - 1;
        i = (i + 1) & (img->tab_cap - 1);
    }
    return -1;
}

static void tab_insert(OfsImage *img, int id)
{
    const OfsEntry *c = &img->e[id];
    uint32_t i = key_hash(c->parent, c->name, c->len) & (img->tab_cap - 1);
    while (img->tab[i])
        i = (i + 1) & (img->tab_cap - 1);
    img->tab[i] = id + 1;
}

static int new_entry(OfsImage *img, int32_t parent, const char *name, size_t len, int is_dir, uint32_t size,
                     long unix_time, char *err, size_t err_size)
{
    if (img->n >= OFS_MAX_ENTRIES) {
        seterr(err, err_size, "Too many files and folders for one image");
        return -1;
    }
    if (img->n == img->cap) {
        uint32_t nc = img->cap ? img->cap * 2 : 256;
        OfsEntry *ne = (OfsEntry *)realloc(img->e, nc * sizeof(OfsEntry));
        if (!ne) {
            seterr(err, err_size, "Out of memory");
            return -1;
        }
        img->e = ne;
        img->cap = nc;
    }
    if ((img->n + 1) * 2 > img->tab_cap) {          /* keep the lookup table at most half full */
        uint32_t nc = img->tab_cap ? img->tab_cap * 2 : 512;
        int32_t *nt = (int32_t *)calloc(nc, sizeof(int32_t));
        if (!nt) {
            seterr(err, err_size, "Out of memory");
            return -1;
        }
        free(img->tab);
        img->tab = nt;
        img->tab_cap = nc;
        for (uint32_t k = 0; k < img->n; k++)
            tab_insert(img, (int)k);
    }
    OfsEntry *c = &img->e[img->n];
    memset(c, 0, sizeof(*c));
    memcpy(c->name, name, len);
    c->name[len] = '\0';
    c->len = (uint8_t)len;
    c->is_dir = (uint8_t)is_dir;
    c->parent = parent;
    c->size = size;
    c->days = ofs_unix_to_days(unix_time, &c->mins, &c->ticks);
    int id = (int)img->n++;
    tab_insert(img, id);
    return id;
}

/* Walk "a/b/c": create the folders, then the final entry (file or folder). */
static int add_entry(OfsImage *img, const char *path, int is_dir, uint32_t size, long unix_time, unsigned *renamed,
                     char *err, size_t err_size)
{
    if (!img || img->laid_out) {
        seterr(err, err_size, "Internal error: image already laid out");
        return -1;
    }
    int32_t parent = -1;
    const char *p = path;
    for (;;) {
        while (*p == '/')
            p++;
        if (!*p)
            break;
        const char *q = p;
        while (*q && *q != '/')
            q++;
        size_t n = (size_t)(q - p);
        const char *r = q;
        while (*r == '/')
            r++;
        int last = (*r == '\0');
        if (n == 1 && p[0] == '.') {
            p = r;
            if (last)
                break;
            continue;
        }
        if (n == 2 && p[0] == '.' && p[1] == '.') {
            seterr(err, err_size, "Unsafe path in the archive (..)");
            return -1;
        }
        char name[OFS_NAME_MAX + 1];
        size_t len = n;
        size_t skip_from = n, skip_to = n;       /* the part of the name that is dropped (none) */
        if (len > OFS_NAME_MAX) {
            /* Too long for an Amiga file system: cut the name, but keep a short extension, so that "<long>.slave" is
               still a .slave file (WHDLoad starts it by this very name). */
            size_t dot = n;
            while (dot > 0 && p[dot - 1] != '.')
                dot--;
            size_t ext = (dot > 1) ? n - (dot - 1) : 0;          /* length of ".ext" */
            if (ext > 0 && ext <= OFS_EXT_KEEP) {
                skip_from = OFS_NAME_MAX - ext;
                skip_to = n - ext;
            } else {
                skip_from = OFS_NAME_MAX;
            }
            len = OFS_NAME_MAX;
            if (renamed)
                (*renamed)++;
        }
        for (size_t i = 0, o = 0; i < n && o < len; i++) {
            if (i >= skip_from && i < skip_to)
                continue;
            name[o++] = (p[i] == ':') ? '_' : p[i];
        }
        name[len] = '\0';

        int id = find_child(img, parent, name, len);
        if (last) {
            if (id >= 0) {
                if (is_dir && img->e[id].is_dir)
                    return id;
                seterr(err, err_size, "Duplicate or conflicting name in the archive");
                return OFS_ERR_EXISTS;
            }
            return new_entry(img, parent, name, len, is_dir, size, unix_time, err, err_size);
        }
        if (id >= 0) {
            if (!img->e[id].is_dir) {
                seterr(err, err_size, "A file and a folder have the same name in the archive");
                return OFS_ERR_EXISTS;
            }
            parent = id;
        } else {
            parent = new_entry(img, parent, name, len, 1, 0, unix_time, err, err_size);
            if (parent < 0)
                return -1;
        }
        p = r;
    }
    seterr(err, err_size, "Empty path in the archive");
    return -1;
}

int ofs_add_dir(OfsImage *img, const char *path, long unix_time, unsigned *renamed, char *err, size_t err_size)
{
    return add_entry(img, path, 1, 0, unix_time, renamed, err, err_size);
}

int ofs_add_file(OfsImage *img, const char *path, uint32_t size, long unix_time, unsigned *renamed, char *err,
                 size_t err_size)
{
    return add_entry(img, path, 0, size, unix_time, renamed, err, err_size);
}

int ofs_entry_path(const OfsImage *img, int id, char *out, size_t out_size)
{
    if (!img || id < 0 || (uint32_t)id >= img->n || out_size == 0)
        return 0;
    int chain[64], depth = 0;
    for (int i = id; i >= 0; i = img->e[i].parent) {
        if (depth >= 64)
            return 0;
        chain[depth++] = i;
    }
    size_t used = 0;
    for (int k = depth - 1; k >= 0; k--) {
        const OfsEntry *c = &img->e[chain[k]];
        if (used + c->len + 2 > out_size)
            return 0;
        if (used)
            out[used++] = '/';
        memcpy(out + used, c->name, c->len);
        used += c->len;
    }
    out[used] = '\0';
    return 1;
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* layout: where every block goes, and all the metadata blocks                                                      */
/* ---------------------------------------------------------------------------------------------------------------- */

static inline uint32_t data_block_number(const OfsImage *img, uint32_t d)
{
    uint32_t b = 2 + d;
    if (b >= img->R)
        b += img->M;                        /* the metadata sits at [R, R + M) */
    return b;
}

static inline uint8_t *meta_block(const OfsImage *img, uint32_t blk)
{
    return img->meta + (size_t)(blk - img->R) * BS;
}

static uint32_t round32(uint64_t v)
{
    return (uint32_t)((v + 31) / 32 * 32);
}

/* hash an entry into its parent's table */
static void link_entry(OfsImage *img, const OfsEntry *c)
{
    uint8_t *pb = c->parent < 0 ? img->meta : meta_block(img, img->e[c->parent].hdr_blk);
    uint8_t *cb = meta_block(img, c->hdr_blk);
    int slot = 6 + (int)ofs_name_hash(c->name, c->len);
    put32(cb, -4, get32(pb, slot));         /* new entries go to the head of the chain */
    put32(pb, slot, c->hdr_blk);
}

int ofs_layout(OfsImage *img, uint64_t spare_bytes, char *err, size_t err_size)
{
    if (!img || img->laid_out) {
        seterr(err, err_size, "Internal error: layout done twice");
        return 0;
    }
    uint64_t D = 0, files = 0, dirs = 0, exts = 0;
    for (uint32_t i = 0; i < img->n; i++) {
        OfsEntry *c = &img->e[i];
        if (c->is_dir) {
            dirs++;
            continue;
        }
        files++;
        c->n_data = (uint32_t)(((uint64_t)c->size + PAYLOAD - 1) / PAYLOAD);
        c->n_ext = c->n_data > PTRS_PER_BLOCK ? (c->n_data - PTRS_PER_BLOCK + PTRS_PER_BLOCK - 1) / PTRS_PER_BLOCK : 0;
        c->data_first = (uint32_t)D;
        D += c->n_data;
        exts += c->n_ext;
    }
    uint64_t base = 1 + dirs + files + exts;                 /* root + folders + file headers + extension blocks */
    uint64_t spare = (spare_bytes + BS - 1) / BS;

    uint64_t N = round32(2 + base + D + spare), B = 0, Bext = 0, M = 0;
    for (;;) {
        uint64_t words = (N - 2 + 31) / 32;
        B = (words + BM_PER_BLOCK - 1) / BM_PER_BLOCK;
        Bext = B > BM_IN_ROOT ? (B - BM_IN_ROOT + BM_PER_BLOCK - 1) / BM_PER_BLOCK : 0;
        M = base + B + Bext;
        uint64_t need = 2 + M + D + spare;
        if (need < 2 * M)
            need = 2 * M;                                     /* the metadata [N/2, N/2 + M) must fit */
        uint64_t nn = round32(need);
        if (nn <= N)
            break;
        N = nn;
    }
    if (N * BS >= 0x7fffffffULL) {
        seterr(err, err_size, "The game is too big for one image (limit 2 GB)");
        return 0;
    }
    img->N = (uint32_t)N;
    img->R = (uint32_t)(N / 2);
    img->M = (uint32_t)M;
    img->B = (uint32_t)B;
    img->Bext = (uint32_t)Bext;
    img->D = (uint32_t)D;
    if (D > 0 && data_block_number(img, img->D - 1) >= img->N) {
        seterr(err, err_size, "Internal error: layout does not fit");
        return 0;
    }
    img->meta = (uint8_t *)calloc((size_t)M, BS);
    if (!img->meta) {
        seterr(err, err_size, "Out of memory (%u metadata blocks)", img->M);
        return 0;
    }

    /* block numbers of the metadata: root, bitmap blocks, bitmap extensions, then folder/file headers in order */
    uint32_t idx = 1 + img->B + img->Bext;
    for (uint32_t i = 0; i < img->n; i++) {
        OfsEntry *c = &img->e[i];
        c->hdr_blk = img->R + idx++;
        if (!c->is_dir) {
            c->ext_first = c->n_ext ? img->R + idx : 0;
            idx += c->n_ext;
        }
    }

    /* root block */
    uint8_t *root = img->meta;
    put32(root, 0, 2);
    put32(root, 3, HT_SIZE);
    put32(root, -50, 0xffffffffu);
    for (uint32_t i = 0; i < img->B && i < BM_IN_ROOT; i++)
        put32(root, -49 + (int)i, img->R + 1 + i);
    put32(root, -24, img->Bext ? img->R + 1 + img->B : 0);
    put32(root, -23, img->days); put32(root, -22, img->mins); put32(root, -21, img->ticks);
    put_bstr(root, -20, img->volname, strlen(img->volname));
    put32(root, -10, img->days); put32(root, -9, img->mins); put32(root, -8, img->ticks);
    put32(root, -7, img->days); put32(root, -6, img->mins); put32(root, -5, img->ticks);
    put32(root, -1, 1);

    /* folders and files */
    for (uint32_t i = 0; i < img->n; i++) {
        OfsEntry *c = &img->e[i];
        uint8_t *b = meta_block(img, c->hdr_blk);
        put32(b, 0, 2);
        put32(b, 1, c->hdr_blk);
        put32(b, -23, c->days); put32(b, -22, c->mins); put32(b, -21, c->ticks);
        put_bstr(b, -20, c->name, c->len);
        put32(b, -3, c->parent < 0 ? img->R : img->e[c->parent].hdr_blk);
        if (c->is_dir) {
            put32(b, -1, 2);                                  /* ST_USERDIR */
            continue;
        }
        put32(b, -1, 0xfffffffdu);                            /* ST_FILE (-3) */
        put32(b, -47, c->size);
        uint32_t in_header = c->n_data < PTRS_PER_BLOCK ? c->n_data : PTRS_PER_BLOCK;
        put32(b, 2, in_header);
        put32(b, 4, c->n_data ? data_block_number(img, c->data_first) : 0);
        for (uint32_t k = 0; k < in_header; k++)
            put32(b, -51 - (int)k, data_block_number(img, c->data_first + k));
        put32(b, -2, c->ext_first);
        for (uint32_t x = 0; x < c->n_ext; x++) {
            uint32_t first = PTRS_PER_BLOCK + x * PTRS_PER_BLOCK;
            uint32_t cnt = c->n_data - first < PTRS_PER_BLOCK ? c->n_data - first : PTRS_PER_BLOCK;
            uint8_t *xb = meta_block(img, c->ext_first + x);
            put32(xb, 0, 16);
            put32(xb, 1, c->ext_first + x);
            put32(xb, 2, cnt);
            for (uint32_t k = 0; k < cnt; k++)
                put32(xb, -51 - (int)k, data_block_number(img, c->data_first + first + k));
            put32(xb, -3, c->hdr_blk);
            put32(xb, -2, x + 1 < c->n_ext ? c->ext_first + x + 1 : 0);
            put32(xb, -1, 0xfffffffdu);
            set_checksum(xb, 5);
        }
    }
    for (uint32_t i = 0; i < img->n; i++)
        link_entry(img, &img->e[i]);
    for (uint32_t i = 0; i < img->n; i++)
        set_checksum(meta_block(img, img->e[i].hdr_blk), 5);
    set_checksum(root, 5);

    /* bitmap: everything free, then clear what is used */
    uint32_t words = (img->N - 2 + 31) / 32;
    uint32_t *bm = (uint32_t *)malloc((size_t)words * sizeof(uint32_t));
    if (!bm) {
        seterr(err, err_size, "Out of memory");
        return 0;
    }
    for (uint32_t w = 0; w < words; w++)
        bm[w] = 0xffffffffu;
    for (uint32_t b = img->N; b < 2 + words * 32; b++)      /* bits of blocks beyond the end of the volume */
        bm[(b - 2) / 32] &= ~(1u << ((b - 2) % 32));
    #define USE(blk) (bm[((blk) - 2) / 32] &= ~(1u << (((blk) - 2) % 32)))
    for (uint32_t b = img->R; b < img->R + img->M; b++)
        USE(b);
    for (uint32_t d = 0; d < img->D; d++) {
        uint32_t b = data_block_number(img, d);
        USE(b);
    }
    #undef USE
    for (uint32_t j = 0; j < img->B; j++) {
        uint8_t *bb = meta_block(img, img->R + 1 + j);
        for (uint32_t t = 0; t < BM_PER_BLOCK; t++) {
            uint32_t g = j * BM_PER_BLOCK + t;
            if (g >= words)
                break;
            put32(bb, 1 + (int)t, bm[g]);
        }
        set_checksum(bb, 0);
    }
    free(bm);
    for (uint32_t x = 0; x < img->Bext; x++) {              /* the bitmap block numbers beyond the 25 in the root */
        uint8_t *xb = meta_block(img, img->R + 1 + img->B + x);
        for (uint32_t t = 0; t < BM_PER_BLOCK; t++) {
            uint32_t j = BM_IN_ROOT + x * BM_PER_BLOCK + t;
            if (j >= img->B)
                break;
            put32(xb, (int)t, img->R + 1 + j);
        }
        put32(xb, -1, x + 1 < img->Bext ? img->R + 1 + img->B + x + 1 : 0);
    }
    img->laid_out = 1;
    return 1;
}

uint64_t ofs_image_bytes(const OfsImage *img)
{
    return img && img->laid_out ? (uint64_t)img->N * BS : 0;
}

uint32_t ofs_image_blocks(const OfsImage *img)
{
    return img && img->laid_out ? img->N : 0;
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* writing                                                                                                          */
/* ---------------------------------------------------------------------------------------------------------------- */

static int flush_out(OfsImage *img)
{
    if (img->out_used) {
        size_t bytes = (size_t)img->out_used * BS;
        if (fwrite(img->out, 1, bytes, img->f) != bytes)
            img->failed = 1;
        img->out_used = 0;
    }
    return !img->failed;
}

static void emit(OfsImage *img, const uint8_t *blk)      /* the next block of the image */
{
    if (img->failed)
        return;
    if (blk)
        memcpy(img->out + (size_t)img->out_used * BS, blk, BS);
    else
        memset(img->out + (size_t)img->out_used * BS, 0, BS);
    img->pos++;
    if (++img->out_used == OUT_BLOCKS)
        flush_out(img);
}

static void fill_to(OfsImage *img, uint32_t target)      /* zero blocks / the metadata, up to block `target` */
{
    while (img->pos < target && !img->failed) {
        if (img->pos >= img->R && img->pos < img->R + img->M)
            emit(img, meta_block(img, img->pos));
        else
            emit(img, NULL);
    }
}

int ofs_begin_write(OfsImage *img, const char *file_path, char *err, size_t err_size)
{
    if (!img || !img->laid_out || img->f) {
        seterr(err, err_size, "Internal error: cannot start writing");
        return 0;
    }
    img->out = (uint8_t *)malloc((size_t)OUT_BLOCKS * BS);
    if (!img->out) {
        seterr(err, err_size, "Out of memory");
        return 0;
    }
    img->f = fopen(file_path, "wb");
    if (!img->f) {
        seterr(err, err_size, "Cannot create the image file");
        return 0;
    }
    img->pos = 0;
    img->out_used = 0;
    img->next_d = 0;
    img->cur = -1;

    uint8_t boot[BS];
    memset(boot, 0, sizeof(boot));
    put32(boot, 0, 0x444f5300u);                              /* DOS\0 */
    put32(boot, 2, img->R);
    uint32_t sum = 0;                                         /* carry-around sum over both boot blocks */
    for (int i = 0; i < LONGS; i++) {
        if (i == 1)
            continue;
        uint32_t v = get32(boot, i);
        sum += v;
        if (sum < v)
            sum++;
    }
    put32(boot, 1, ~sum);
    emit(img, boot);
    emit(img, NULL);
    if (img->failed) {
        seterr(err, err_size, "Cannot write the image file (SD card full or locked?)");
        return 0;
    }
    return 1;
}

static void emit_data_block(OfsImage *img, const OfsEntry *e, uint32_t entry_id)
{
    (void)entry_id;
    uint32_t d = img->next_d;
    uint32_t b = data_block_number(img, d);
    fill_to(img, b);
    uint8_t blk[BS];
    memset(blk, 0, sizeof(blk));
    img->cur_seq++;
    put32(blk, 0, 8);
    put32(blk, 1, e->hdr_blk);
    put32(blk, 2, img->cur_seq);
    put32(blk, 3, img->part_len);
    put32(blk, 4, img->cur_seq < e->n_data ? data_block_number(img, d + 1) : 0);
    memcpy(blk + 24, img->part, img->part_len);
    set_checksum(blk, 5);
    emit(img, blk);
    img->next_d++;
    img->part_len = 0;
}

int ofs_write_file(OfsImage *img, int id, const void *data, size_t len, char *err, size_t err_size)
{
    if (!img || !img->f || img->failed || id < 0 || (uint32_t)id >= img->n || img->e[id].is_dir) {
        seterr(err, err_size, "Internal error: bad write request");
        return 0;
    }
    const OfsEntry *e = &img->e[id];
    if (img->cur != id) {
        if (img->cur >= 0 && img->cur_written != img->e[img->cur].size) {
            seterr(err, err_size, "Internal error: previous file incomplete");
            return 0;
        }
        if (e->n_data && e->data_first != img->next_d) {
            seterr(err, err_size, "Internal error: files written out of order");
            return 0;
        }
        img->cur = id;
        img->cur_written = 0;
        img->cur_seq = 0;
        img->part_len = 0;
    }
    const uint8_t *p = (const uint8_t *)data;
    while (len > 0) {
        if (img->cur_written >= e->size) {
            seterr(err, err_size, "Internal error: more data than announced");
            return 0;
        }
        size_t take = PAYLOAD - img->part_len;
        if (take > len)
            take = len;
        if (take > e->size - img->cur_written)
            take = e->size - img->cur_written;
        memcpy(img->part + img->part_len, p, take);
        img->part_len += (uint32_t)take;
        img->cur_written += (uint32_t)take;
        p += take;
        len -= take;
        if (img->part_len == PAYLOAD || img->cur_written == e->size)
            emit_data_block(img, e, (uint32_t)id);
    }
    if (img->failed) {
        seterr(err, err_size, "Cannot write the image file (SD card full?)");
        return 0;
    }
    return 1;
}

int ofs_finish(OfsImage *img, char *err, size_t err_size)
{
    if (!img || !img->f) {
        seterr(err, err_size, "Internal error: not writing");
        return 0;
    }
    int ok = 1;
    if ((img->cur >= 0 && img->cur_written != img->e[img->cur].size) || img->next_d != img->D) {
        seterr(err, err_size, "Internal error: not all file data was written");
        ok = 0;
    }
    if (ok) {
        fill_to(img, img->N);
        flush_out(img);
        if (img->failed) {
            seterr(err, err_size, "Cannot write the image file (SD card full?)");
            ok = 0;
        }
    }
    if (fclose(img->f) != 0 && ok) {
        seterr(err, err_size, "Cannot close the image file");
        ok = 0;
    }
    img->f = NULL;
    img->finished = ok;
    return ok;
}
