/*
 * Independent checker / reader for OFS hard disk images, used by the host tests of src/3ds/ofs_image.cpp.
 *
 * It does not share any code with the writer. The rules it enforces come from the block layouts of the Amiga file system
 * as implemented by amitools (the validator that checks real Amiga disks): checksums, block types, own-block numbers,
 * parent links, hash table placement, data block chain (sequence numbers, sizes, next pointers), extension blocks, the
 * bitmap (every block used exactly once and marked in the bitmap, every other block free) and the position of the root.
 * Every block of the volume must be reachable and no block may be used twice.
 */
#ifndef OFS_READER_H
#define OFS_READER_H

#include <stdint.h>
#include <stdio.h>
#include <map>
#include <set>
#include <string>
#include <vector>

struct OfsRead {
    std::string volname;
    uint32_t nblocks = 0, root = 0, bitmap_blocks = 0, bitmap_ext_blocks = 0, free_blocks = 0, files_seen = 0;
    std::map<std::string, std::vector<uint8_t>> files;     /* "dir/sub/name" (names as stored) -> contents */
    std::set<std::string> dirs;
};

class OfsChecker {
public:
    bool run(const std::string &path, OfsRead &r, std::string &err)
    {
        FILE *f = fopen(path.c_str(), "rb");
        if (!f)
            return fail(err, "cannot open image");
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz <= 0 || sz % 512) {
            fclose(f);
            return fail(err, "size is not a multiple of 512");
        }
        img_.resize((size_t)sz);
        if (fread(img_.data(), 1, img_.size(), f) != img_.size()) {
            fclose(f);
            return fail(err, "short read");
        }
        fclose(f);
        N_ = (uint32_t)(sz / 512);
        r.nblocks = N_;
        if (N_ % 32)
            return fail(err, "block count is not a multiple of 32 (emulator geometry 32x1)");
        used_.assign(N_, 0);

        /* boot blocks: DOS\0, checksum (carry-around sum of both blocks), root pointer */
        const uint8_t *b0 = blk(0);
        if (L(b0, 0) != 0x444f5300u)
            return fail(err, "boot block is not DOS\\0");
        uint32_t sum = 0;
        for (int bb = 0; bb < 2; bb++)
            for (int i = 0; i < 128; i++) {
                if (bb == 0 && i == 1)
                    continue;
                uint32_t v = L(blk(bb), i);
                sum += v;
                if (sum < v)
                    sum++;
            }
        if (L(b0, 1) != ~sum)
            return fail(err, "boot block checksum");
        used_[0] = used_[1] = 1;
        uint32_t root = (2 + N_ - 1) / 2;                    /* (reserved + blocks - 1) / 2 */
        r.root = root;
        if (L(b0, 2) != root)
            return fail(err, "boot block root pointer");

        const uint8_t *rb = blk(root);
        if (!claim(root, err, "root"))
            return false;
        if (L(rb, 0) != 2 || L(rb, -1) != 1 || L(rb, 3) != 72 || !csum_ok(rb, 5))
            return fail(err, "root block header/checksum");
        if (L(rb, 1) != 0 || L(rb, -3) != 0)
            return fail(err, "root block own/parent");
        if (L(rb, -50) != 0xffffffffu)
            return fail(err, "root bitmap flag not valid");
        r.volname = bstr(rb, -20);
        if (r.volname.empty() || r.volname.size() > 30)
            return fail(err, "root name length");

        if (!walk(root, "", r, err))
            return false;

        /* bitmap blocks: 25 pointers in the root, then the extension chain */
        std::vector<uint32_t> bm;
        for (int i = 0; i < 25; i++)
            if (L(rb, -49 + i))
                bm.push_back(L(rb, -49 + i));
        uint32_t ext = L(rb, -24);
        int guard = 0;
        while (ext) {
            if (++guard > 100000 || !claim(ext, err, "bitmap ext"))
                return fail(err, "bitmap extension chain");
            r.bitmap_ext_blocks++;
            const uint8_t *x = blk(ext);
            for (int i = 0; i < 127; i++)
                if (L(x, i))
                    bm.push_back(L(x, i));
            ext = L(x, 127);
        }
        uint32_t words = (N_ - 2 + 31) / 32;
        uint32_t want_bm = (words + 126) / 127;
        if (bm.size() != want_bm)
            return fail(err, "number of bitmap blocks");
        r.bitmap_blocks = (uint32_t)bm.size();
        for (uint32_t p : bm)
            if (!claim(p, err, "bitmap"))
                return false;
        for (uint32_t j = 0; j < bm.size(); j++) {
            const uint8_t *bb = blk(bm[j]);
            if (!csum_ok(bb, 0))
                return fail(err, "bitmap block checksum");
            for (uint32_t t = 0; t < 127; t++) {
                uint32_t g = j * 127 + t;
                if (g >= words) {
                    if (L(bb, 1 + (int)t) != 0)
                        return fail(err, "unused bitmap longs not zero");
                    continue;
                }
                uint32_t expect = 0;
                for (uint32_t k = 0; k < 32; k++) {
                    uint32_t b = 2 + g * 32 + k;
                    if (b < N_ && !used_[b])
                        expect |= 1u << k;                  /* bit set = free */
                }
                if (L(bb, 1 + (int)t) != expect) {
                    char m[120];
                    snprintf(m, sizeof m, "bitmap word %u: got %08x expected %08x", g, L(bb, 1 + (int)t), expect);
                    return fail(err, m);
                }
            }
        }
        for (uint32_t b = 2; b < N_; b++)
            if (!used_[b])
                r.free_blocks++;
        return true;
    }

private:
    std::vector<uint8_t> img_;
    std::vector<uint8_t> used_;
    uint32_t N_ = 0;

    const uint8_t *blk(uint32_t b) const { return &img_[(size_t)b * 512]; }
    static uint32_t L(const uint8_t *b, int i)
    {
        if (i < 0)
            i += 128;
        const uint8_t *p = b + i * 4;
        return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
    }
    static bool csum_ok(const uint8_t *b, int)
    {
        uint32_t s = 0;
        for (int i = 0; i < 128; i++)
            s += L(b, i);
        return s == 0;
    }
    static std::string bstr(const uint8_t *b, int idx)
    {
        if (idx < 0)
            idx += 128;
        return std::string((const char *)b + idx * 4 + 1, b[idx * 4]);
    }
    static unsigned hash(const std::string &n)
    {
        uint32_t h = (uint32_t)n.size();
        for (unsigned char c : n) {
            if (c >= 'a' && c <= 'z')
                c -= 32;
            h = (h * 13 + c) & 0x7ff;
        }
        return h % 72;
    }
    static std::string upr(std::string s)
    {
        for (auto &c : s)
            if (c >= 'a' && c <= 'z')
                c -= 32;
        return s;
    }
    static bool fail(std::string &err, const std::string &m)
    {
        err = m;
        return false;
    }
    bool claim(uint32_t b, std::string &err, const char *what)
    {
        if (b < 2 || b >= N_) {
            err = std::string("block number out of range (") + what + ")";
            return false;
        }
        if (used_[b]) {
            err = std::string("block used twice (") + what + ")";
            return false;
        }
        used_[b] = 1;
        return true;
    }

    bool walk(uint32_t dir_blk, const std::string &prefix, OfsRead &r, std::string &err)
    {
        std::set<std::string> seen;
        const uint8_t *db = blk(dir_blk);
        for (int h = 0; h < 72; h++) {
            uint32_t b = L(db, 6 + h);
            uint32_t guard = 0;
            while (b) {
                if (++guard > N_)
                    return fail(err, "hash chain loop");
                if (!claim(b, err, "entry"))
                    return false;
                const uint8_t *e = blk(b);
                if (L(e, 0) != 2 || L(e, 1) != b || !csum_ok(e, 5))
                    return fail(err, "entry header/own key/checksum");
                if (L(e, -3) != dir_blk)
                    return fail(err, "entry parent");
                std::string name = bstr(e, -20);
                if (name.empty() || name.size() > 30)
                    return fail(err, "entry name length");
                if (hash(name) != (unsigned)h)
                    return fail(err, "entry in the wrong hash chain");
                if (!seen.insert(upr(name)).second)
                    return fail(err, "duplicate name in a folder");
                uint32_t sec = L(e, -1);
                if (sec == 2) {
                    r.dirs.insert(prefix + name);
                    if (!walk(b, prefix + name + "/", r, err))
                        return false;
                } else if (sec == 0xfffffffdu) {
                    if (!read_file(b, e, prefix + name, r, err))
                        return false;
                } else
                    return fail(err, "unknown entry type");
                b = L(e, -4);
            }
        }
        return true;
    }

    bool read_file(uint32_t hb, const uint8_t *e, const std::string &path, OfsRead &r, std::string &err)
    {
        uint32_t size = L(e, -47);
        std::vector<uint32_t> ptrs;
        uint32_t n_here = L(e, 2);
        if (n_here > 72)
            return fail(err, "file header pointer count");
        for (uint32_t i = 0; i < n_here; i++)
            ptrs.push_back(L(e, -51 - (int)i));
        uint32_t ext = L(e, -2);
        uint32_t guard = 0;
        if (ext && n_here != 72)
            return fail(err, "extension block used before the header is full");
        while (ext) {
            if (++guard > N_ || !claim(ext, err, "file extension"))
                return fail(err, "file extension chain");
            const uint8_t *x = blk(ext);
            if (L(x, 0) != 16 || L(x, 1) != ext || L(x, -3) != hb || L(x, -1) != 0xfffffffdu || !csum_ok(x, 5))
                return fail(err, "extension block header");
            uint32_t cnt = L(x, 2);
            if (cnt > 72)
                return fail(err, "extension pointer count");
            for (uint32_t i = 0; i < cnt; i++)
                ptrs.push_back(L(x, -51 - (int)i));
            uint32_t nx = L(x, -2);
            if (nx && cnt != 72)
                return fail(err, "extension block not full");
            ext = nx;
        }
        uint32_t want = (size + 487) / 488;
        if (ptrs.size() != want)
            return fail(err, "data block count does not match the file size");
        if (L(e, 4) != (ptrs.empty() ? 0 : ptrs[0]))
            return fail(err, "first_data");
        std::vector<uint8_t> data;
        data.reserve(size);
        for (uint32_t i = 0; i < ptrs.size(); i++) {
            if (!claim(ptrs[i], err, "data"))
                return false;
            const uint8_t *d = blk(ptrs[i]);
            uint32_t exp_len = (i + 1 < ptrs.size()) ? 488 : size - 488 * i;
            if (L(d, 0) != 8 || L(d, 1) != hb || L(d, 2) != i + 1 || L(d, 3) != exp_len ||
                L(d, 4) != (i + 1 < ptrs.size() ? ptrs[i + 1] : 0) || !csum_ok(d, 5))
                return fail(err, "data block header");
            data.insert(data.end(), d + 24, d + 24 + exp_len);
        }
        r.files_seen++;
        r.files[path] = std::move(data);
        return true;
    }
};

#endif
