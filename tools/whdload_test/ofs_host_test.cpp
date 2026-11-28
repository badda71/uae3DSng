/*
 * Host test of src/3ds/ofs_image.cpp: builds images, reads them back with the independent checker (ofs_reader.h) and
 * compares every file byte for byte. Built with ASan/UBSan by run.sh.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <map>
#include <string>
#include <vector>

#include "ofs_image.h"
#include "ofs_reader.h"

static int g_checks = 0, g_fail = 0;
#define CHECK(c) do { g_checks++; if (!(c)) { g_fail++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static uint32_t rng_state = 12345;
static uint32_t rnd() { rng_state = rng_state * 1664525u + 1013904223u; return rng_state >> 8; }

static std::vector<uint8_t> make_data(size_t n)
{
    std::vector<uint8_t> v(n);
    for (size_t i = 0; i < n; i++)
        v[i] = (uint8_t)rnd();
    return v;
}

struct Spec {
    std::string path;
    std::vector<uint8_t> data;
    bool dir = false;
};

/* Build an image from the specs (written in chunks of odd sizes), check it. expect_names = path as stored. */
static bool build_and_check(const std::string &tmp, const std::vector<Spec> &specs, uint64_t spare, OfsRead *out_read,
                            const char *label, size_t chunk = 1000)
{
    char err[200] = "";
    OfsImage *img = ofs_new("WHDGame");
    std::vector<int> ids;
    unsigned renamed = 0;
    for (const Spec &s : specs) {
        int id = s.dir ? ofs_add_dir(img, s.path.c_str(), 0, &renamed, err, sizeof err)
                       : ofs_add_file(img, s.path.c_str(), (uint32_t)s.data.size(), 0, &renamed, err, sizeof err);
        if (id < 0) {
            fprintf(stderr, "%s: add failed: %s\n", label, err);
            ofs_free(img);
            return false;
        }
        ids.push_back(id);
    }
    if (!ofs_layout(img, spare, err, sizeof err)) {
        fprintf(stderr, "%s: layout failed: %s\n", label, err);
        ofs_free(img);
        return false;
    }
    uint64_t bytes = ofs_image_bytes(img);
    if (!ofs_begin_write(img, tmp.c_str(), err, sizeof err)) {
        fprintf(stderr, "%s: begin failed: %s\n", label, err);
        ofs_free(img);
        return false;
    }
    for (size_t i = 0; i < specs.size(); i++) {
        if (specs[i].dir)
            continue;
        const std::vector<uint8_t> &d = specs[i].data;
        for (size_t off = 0; off < d.size(); off += chunk) {
            size_t n = d.size() - off < chunk ? d.size() - off : chunk;
            if (!ofs_write_file(img, ids[i], d.data() + off, n, err, sizeof err)) {
                fprintf(stderr, "%s: write failed: %s\n", label, err);
                ofs_abort(img);
                ofs_free(img);
                return false;
            }
        }
    }
    if (!ofs_finish(img, err, sizeof err)) {
        fprintf(stderr, "%s: finish failed: %s\n", label, err);
        ofs_free(img);
        return false;
    }
    ofs_free(img);

    struct stat st;
    if (stat(tmp.c_str(), &st) != 0 || (uint64_t)st.st_size != bytes) {
        fprintf(stderr, "%s: image size %lld != announced %llu\n", label, (long long)st.st_size, (unsigned long long)bytes);
        return false;
    }
    OfsRead r;
    std::string e;
    OfsChecker chk;
    if (!chk.run(tmp, r, e)) {
        fprintf(stderr, "%s: CHECKER: %s\n", label, e.c_str());
        return false;
    }
    if (out_read)
        *out_read = r;
    return true;
}

static std::string cleaned(const std::string &p)   /* what the writer is expected to store */
{
    std::string out;
    for (char c : p)
        out += (c == ':') ? '_' : c;
    return out;
}

int main(int argc, char **argv)
{
    std::string work = argc > 1 ? argv[1] : "/tmp";
    mkdir(work.c_str(), 0777);
    std::string img = work + "/t.hdf";

    /* ---- pure helpers */
    CHECK(ofs_name_hash("", 0) == 0);
    CHECK(ofs_name_hash("abc", 3) == ofs_name_hash("ABC", 3));                       /* case-insensitive */
    { uint32_t m, t; CHECK(ofs_unix_to_days(252460800, &m, &t) == 0 && m == 0 && t == 0);
      CHECK(ofs_unix_to_days(252460800 + 86400 + 3600 + 3, &m, &t) == 1 && m == 60 && t == 150); }

    /* ---- empty volume */
    { OfsRead r; CHECK(build_and_check(img, {}, 1 << 20, &r, "empty")); CHECK(r.files.empty() && r.dirs.empty());
      CHECK(r.volname == "WHDGame" && r.nblocks >= 2048 && r.nblocks % 32 == 0); }

    /* ---- file sizes around the block and header/extension boundaries */
    {
        const size_t P = 488, K = 72;
        std::vector<size_t> sizes = { 0, 1, 487, 488, 489, 976, 977, K * P - 1, K * P, K * P + 1, (K + 1) * P,
                                      2 * K * P, 2 * K * P + 1, 3 * K * P + 5, 150 * P + 7, 1000 * P, 123457 };
        std::vector<Spec> specs;
        for (size_t i = 0; i < sizes.size(); i++) {
            Spec s; s.path = "f" + std::to_string(i); s.data = make_data(sizes[i]); specs.push_back(s);
        }
        OfsRead r;
        CHECK(build_and_check(img, specs, 1 << 20, &r, "sizes"));
        for (const Spec &s : specs) CHECK(r.files.count(s.path) && r.files[s.path] == s.data);
        CHECK(r.files.size() == specs.size());
        /* chunking must not matter */
        CHECK(build_and_check(img, specs, 1 << 20, &r, "sizes (chunk 1)", 1));
        CHECK(build_and_check(img, specs, 0, &r, "sizes (no spare, chunk 4096)", 4096));
        for (const Spec &s : specs) CHECK(r.files[s.path] == s.data);
    }

    /* ---- folders, implicit parents, names */
    {
        std::vector<Spec> specs;
        auto add = [&](const std::string &p, size_t n, bool dir = false) { Spec s; s.path = p; s.data = make_data(n); s.dir = dir; specs.push_back(s); };
        add("Game", 0, true);
        add("Game/Game.slave", 3000);
        add("Game/data/deep/er/file.bin", 5000);              /* parents created implicitly */
        add("Game/data", 0, true);                            /* explicit folder that already exists: fine */
        add("Game/EMPTYDIR", 0, true);
        add("Game/data/a:b", 10);                             /* ':' is not allowed in Amiga names */
        add("/lead//slash/./x", 7);
        add("123456789012345678901234567890", 1);             /* exactly 30 characters */
        std::string longname(40, 'L');
        add("Game/" + longname, 2);                           /* 40 characters: cut to 30 */
        std::string longslave = "FateGatesOfDawnUncensored.slave";       /* 31 characters: the extension must stay */
        add("Game/" + longslave, 4);
        std::string longext = std::string(26, 'E') + ".012345678901234";     /* extension too long to keep: plain cut */
        add("Game/" + longext, 5);
        add("Game/" + std::string(40, 'M') + ".info", 6);       /* extension kept, name part cut */
        OfsRead r;
        CHECK(build_and_check(img, specs, 1 << 20, &r, "folders", 77));
        CHECK(r.dirs.count("Game") && r.dirs.count("Game/data") && r.dirs.count("Game/data/deep") &&
              r.dirs.count("Game/data/deep/er") && r.dirs.count("Game/EMPTYDIR") && r.dirs.count("lead") && r.dirs.count("lead/slash"));
        CHECK(r.files["Game/Game.slave"] == specs[1].data);
        CHECK(r.files["Game/data/deep/er/file.bin"] == specs[2].data);
        CHECK(r.files.count("Game/data/a_b") && r.files["Game/data/a_b"] == specs[5].data);
        CHECK(r.files["lead/slash/x"] == specs[6].data);
        CHECK(r.files["123456789012345678901234567890"] == specs[7].data);
        CHECK(r.files.count("Game/" + longname.substr(0, 30)) && r.files["Game/" + longname.substr(0, 30)] == specs[8].data);
        CHECK(r.files.count("Game/FateGatesOfDawnUncensore.slave") && r.files["Game/FateGatesOfDawnUncensore.slave"] == specs[9].data);
        CHECK(r.files.count("Game/" + longext.substr(0, 30)) && r.files["Game/" + longext.substr(0, 30)] == specs[10].data);
        CHECK(r.files.count("Game/" + std::string(25, 'M') + ".info") && r.files["Game/" + std::string(25, 'M') + ".info"] == specs[11].data);
        (void)cleaned;
    }

    /* ---- many files in one folder, many with the same hash */
    {
        std::vector<Spec> specs;
        for (int i = 0; i < 700; i++) { Spec s; s.path = "big/file" + std::to_string(i); s.data = make_data(rnd() % 2000); specs.push_back(s); }
        int same = 0;                                        /* names that all hash to slot 5 */
        for (int i = 0; same < 40; i++) {
            std::string n = "same" + std::to_string(i);
            if (ofs_name_hash(n.c_str(), n.size()) == 5) { Spec s; s.path = "big/" + n; s.data = make_data(100 + i); specs.push_back(s); same++; }
        }
        OfsRead r;
        CHECK(build_and_check(img, specs, 1 << 20, &r, "many"));
        for (const Spec &s : specs) CHECK(r.files.count(s.path) && r.files[s.path] == s.data);
        CHECK(r.files.size() == specs.size());
    }

    /* ---- the metadata hole is crossed by data (small spare, data bigger than half the image) */
    {
        std::vector<Spec> specs;
        for (int i = 0; i < 30; i++) { Spec s; s.path = "d" + std::to_string(i / 10) + "/f" + std::to_string(i); s.data = make_data(100000 + rnd() % 50000); specs.push_back(s); }
        OfsRead r;
        CHECK(build_and_check(img, specs, 0, &r, "hole", 4000));
        for (const Spec &s : specs) CHECK(r.files[s.path] == s.data);
    }

    /* ---- more than 25 bitmap blocks (bitmap extension), one file of ~56 MB */
    {
        std::vector<Spec> specs;
        Spec s; s.path = "huge.bin"; s.data = make_data(56u * 1024 * 1024); specs.push_back(s);
        OfsRead r;
        bool ok = build_and_check(img, specs, 3 << 20, &r, "bitmap ext", 65536);
        CHECK(ok);
        if (ok) { CHECK(r.bitmap_blocks > 25 && r.bitmap_ext_blocks == 1); CHECK(r.files["huge.bin"] == s.data); }
    }
    /* ---- two bitmap extension blocks (> 152 bitmap blocks = ~300 MB), empty files, lots of spare space */
    {
        std::vector<Spec> specs;
        Spec s; s.path = "x"; s.data = make_data(10); specs.push_back(s);
        OfsRead r;
        bool ok = build_and_check(img, specs, 330ull << 20, &r, "bitmap ext chain", 65536);
        CHECK(ok);
        if (ok) { CHECK(r.bitmap_ext_blocks == 2); CHECK(r.files["x"] == s.data); }
    }

    /* ---- error handling */
    {
        char err[200];
        OfsImage *i2 = ofs_new("T");
        CHECK(ofs_add_file(i2, "a/b", 0, 0, nullptr, err, sizeof err) >= 0);
        CHECK(ofs_add_file(i2, "a/b", 5, 0, nullptr, err, sizeof err) < 0);          /* duplicate */
        CHECK(ofs_add_file(i2, "A/B", 5, 0, nullptr, err, sizeof err) < 0);          /* case-insensitive duplicate */
        CHECK(ofs_add_file(i2, "a/b/c", 5, 0, nullptr, err, sizeof err) < 0);        /* file used as a folder */
        CHECK(ofs_add_dir(i2, "a/b", 0, nullptr, err, sizeof err) < 0);              /* folder over a file */
        CHECK(ofs_add_file(i2, "../x", 5, 0, nullptr, err, sizeof err) < 0 && strstr(err, ".."));
        CHECK(ofs_add_file(i2, "a/../x", 5, 0, nullptr, err, sizeof err) < 0);
        CHECK(ofs_add_file(i2, "", 5, 0, nullptr, err, sizeof err) < 0);
        CHECK(ofs_add_file(i2, "//", 5, 0, nullptr, err, sizeof err) < 0);
        int c = ofs_add_file(i2, "c", 1000, 0, nullptr, err, sizeof err);
        int d = ofs_add_file(i2, "d", 1000, 0, nullptr, err, sizeof err);
        CHECK(ofs_layout(i2, 1 << 20, err, sizeof err));
        CHECK(ofs_add_file(i2, "late", 5, 0, nullptr, err, sizeof err) < 0);         /* after the layout */
        CHECK(ofs_begin_write(i2, (work + "/e.hdf").c_str(), err, sizeof err));
        std::vector<uint8_t> blob(1000, 7);
        CHECK(!ofs_write_file(i2, d, blob.data(), 1000, err, sizeof err));          /* out of order */
        CHECK(ofs_write_file(i2, c, blob.data(), 500, err, sizeof err));
        CHECK(!ofs_write_file(i2, d, blob.data(), 1000, err, sizeof err));          /* c is incomplete */
        CHECK(ofs_write_file(i2, c, blob.data(), 500, err, sizeof err));
        CHECK(!ofs_write_file(i2, c, blob.data(), 1, err, sizeof err));             /* more than announced */
        CHECK(!ofs_finish(i2, err, sizeof err) && strstr(err, "not all"));          /* d was never written */
        ofs_free(i2);
        unlink((work + "/e.hdf").c_str());

        OfsImage *i3 = ofs_new("T");
        CHECK(ofs_add_file(i3, "z", 0xffffffffu, 0, nullptr, err, sizeof err) >= 0);  /* 4 GB file: layout must refuse */
        CHECK(!ofs_layout(i3, 0, err, sizeof err) && strstr(err, "too big"));
        ofs_free(i3);
    }

    unlink(img.c_str());
    printf("== ofs_image: %d checks, %d failures\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
