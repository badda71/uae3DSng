/*
 * Host (PC) test for src/3ds/whdload3ds.cpp - see run.sh.
 *
 *   whdload_host_test <dir with .lzh/.lha files, searched recursively> <empty work dir>
 *
 * 1. unit tests of the name/path helpers, game list, launch preparation
 * 2. installs every archive found into its own root and prints OK/FAIL. Every successful extraction has been verified
 *    against the CRC-16 and the length stored in the archive. All files named "gpl-2" (the same text, packed by
 *    ~40 different LHA programs and methods) must additionally be byte-identical, and "gpl-2.gz" (stored) must
 *    gunzip to the very same bytes.
 */
#include "../../src/3ds/whdload3ds.h"

#include <dirent.h>
#include <unistd.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <map>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(cond) do { if (!(cond)) { printf("  CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); g_fail++; } } while (0)

static bool is_dir(const std::string &p) { struct stat st; return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode); }

static void walk(const std::string &dir, std::vector<std::string> &out)
{
    DIR *d = opendir(dir.c_str());
    if (!d) return;
    std::vector<std::string> names;
    while (dirent *e = readdir(d))
        if (e->d_name[0] != '.') names.push_back(e->d_name);
    closedir(d);
    for (size_t i = 0; i < names.size(); i++)
        for (size_t j = i + 1; j < names.size(); j++)
            if (names[j] < names[i]) names[i].swap(names[j]);
    for (auto &n : names) {
        std::string p = dir + "/" + n;
        if (is_dir(p)) walk(p, out);
        else if (n.size() > 4 && (!strcasecmp(n.c_str() + n.size() - 4, ".lzh") || !strcasecmp(n.c_str() + n.size() - 4, ".lha")))
            out.push_back(p);
    }
}

static unsigned long long fnv_file(const std::string &path, long *size = nullptr)
{
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) return 0;
    unsigned long long h = 1469598103934665603ULL;
    long n = 0;
    int c;
    while ((c = fgetc(f)) != EOF) { h = (h ^ (unsigned char)c) * 1099511628211ULL; n++; }
    fclose(f);
    if (size) *size = n;
    return h;
}

static unsigned long long fnv_gunzip(const std::string &path)
{
    std::string cmd = "gzip -dc '" + path + "' 2>/dev/null";
    FILE *p = popen(cmd.c_str(), "r");
    if (!p) return 0;
    unsigned long long h = 1469598103934665603ULL;
    int c;
    while ((c = fgetc(p)) != EOF) h = (h ^ (unsigned char)c) * 1099511628211ULL;
    pclose(p);
    return h;
}

static void write_file(const std::string &p, const char *text)
{
    FILE *f = fopen(p.c_str(), "wb"); fputs(text, f); fclose(f);
}

static std::string slurp(const std::string &p)
{
    std::string s; FILE *f = fopen(p.c_str(), "rb"); if (!f) return s;
    char b[4096]; size_t n; while ((n = fread(b, 1, sizeof b, f)) > 0) s.append(b, n); fclose(f); return s;
}

static void unit_tests(const std::string &work)
{
    printf("== unit tests\n");
    char out[300]; unsigned ren;

    ren = 0;
    CHECK(whd_safe_relative_path("a/b/c.txt", out, sizeof out, &ren) && !strcmp(out, "a/b/c.txt") && ren == 0);
    CHECK(whd_safe_relative_path("/abs/x", out, sizeof out, &ren) && !strcmp(out, "abs/x"));
    CHECK(whd_safe_relative_path("./a//b/", out, sizeof out, &ren) && !strcmp(out, "a/b"));
    CHECK(whd_safe_relative_path("a\\b", out, sizeof out, &ren) && !strcmp(out, "a/b"));
    CHECK(!whd_safe_relative_path("../x", out, sizeof out, &ren));
    CHECK(!whd_safe_relative_path("a/../../x", out, sizeof out, &ren));
    CHECK(!whd_safe_relative_path("", out, sizeof out, &ren));
    CHECK(!whd_safe_relative_path("/", out, sizeof out, &ren));
    ren = 0;
    CHECK(whd_safe_relative_path("Game:Data/Lev?l*1", out, sizeof out, &ren) && !strcmp(out, "Game_Data/Lev_l_1") && ren == 3);
    ren = 0;
    CHECK(whd_safe_relative_path("dir./file ", out, sizeof out, &ren) && !strcmp(out, "dir_/file_") && ren == 2);
    CHECK(whd_safe_relative_path("caf\xe9", out, sizeof out, &ren) && !strcmp(out, "caf_"));
    CHECK(!whd_safe_relative_path("a/b/c/d/e", out, 6, &ren));   /* does not fit */

    char f[128];
    whd_folder_name_from_archive("sdmc:/roms/ShadowOfTheBeast_v1.2.lha", f, sizeof f); CHECK(!strcmp(f, "ShadowOfTheBeast_v1.2"));
    whd_folder_name_from_archive("C:\\x\\Game One.LZH", f, sizeof f);                   CHECK(!strcmp(f, "Game One"));
    whd_folder_name_from_archive("/a/C.lha", f, sizeof f);                              CHECK(!strcmp(f, "C_game"));
    whd_folder_name_from_archive("/a/..lha", f, sizeof f);                              CHECK(!strcmp(f, "game"));
    whd_folder_name_from_archive("/a/we?ird:name.lha", f, sizeof f);                    CHECK(!strcmp(f, "we_ird_name") || !strcmp(f, "name"));

    /* game list + launch preparation on a fake tree */
    std::string root = work + "/fake";
    mkdir(root.c_str(), 0777);
    for (const char *d : { "/Zebra", "/alpha", "/S", "/C", "/Devs", "/.hidden", "/Beta" }) mkdir((root + d).c_str(), 0777);
    write_file(root + "/notadir", "x");
    char names[16][WHD_NAME_MAX];
    int n = whd_list_games(root.c_str(), names, 16);
    CHECK(n == 3 && !strcmp(names[0], "alpha") && !strcmp(names[1], "Beta") && !strcmp(names[2], "Zebra"));

    char err[200];
    CHECK(!whd_prepare_launch(root.c_str(), "nogame", nullptr, nullptr, err, sizeof err) && strstr(err, "not found"));
    CHECK(!whd_prepare_launch(root.c_str(), "../x", nullptr, nullptr, err, sizeof err));
    CHECK(!whd_prepare_launch(root.c_str(), "alpha", nullptr, nullptr, err, sizeof err) && strstr(err, "WHDLoad is missing"));
    write_file(root + "/C/WHDLoad", "binary");
    CHECK(!whd_prepare_launch(root.c_str(), "alpha", nullptr, nullptr, err, sizeof err) && strstr(err, "No .slave"));

    mkdir((root + "/alpha/Game").c_str(), 0777);
    write_file(root + "/alpha/Game/Game.slave", "s");
    write_file(root + "/alpha/Game/GameCD.slave", "s");
    write_file(root + "/alpha/Other.slave", "s");
    std::string kdir = work + "/kicks";
    mkdir(kdir.c_str(), 0777);
    write_file(kdir + "/kick13.rom", "ROM13");
    write_file(kdir + "/Kick34005.A500.RTB", "rtb");
    write_file(kdir + "/kick31.rom", "ROM31");
    CHECK(whd_prepare_launch(root.c_str(), "alpha", kdir.c_str(), nullptr, err, sizeof err));
    std::string ss = slurp(root + "/S/Startup-Sequence");
    printf("  Startup-Sequence:\n%s", ss.c_str());
    CHECK(ss.find("CD \"SYS:alpha\"\n") != std::string::npos);        /* Other.slave is closest to the top */
    CHECK(ss.compare(0, 27, "; uae3DSng WHDLoad launcher") == 0);
    CHECK(ss.find("C:WHDLoad \"Other.slave\" PRELOAD\n") != std::string::npos);
    CHECK(slurp(root + "/Devs/Kickstarts/kick34005.A500") == "ROM13");
    CHECK(slurp(root + "/Devs/Kickstarts/kick40068.A1200") == "ROM31");
    CHECK(slurp(root + "/Devs/Kickstarts/Kick34005.A500.RTB") == "rtb");
    CHECK(access((root + "/S/Startup-Sequence.orig").c_str(), 0) != 0);

    /* user's own startup sequence with SetPatch is kept and extended; our own is simply replaced */
    write_file(root + "/S/Startup-Sequence", "C:SetPatch QUIET\nVersion\n  EndCLI >NIL:\n");
    CHECK(whd_prepare_launch(root.c_str(), "alpha", kdir.c_str(), "NOVBRMOVE", err, sizeof err));
    ss = slurp(root + "/S/Startup-Sequence");
    CHECK(ss == "C:SetPatch QUIET\nVersion\n; uae3DSng WHDLoad launcher (rewritten for every start)\nCD \"SYS:alpha\"\nC:WHDLoad \"Other.slave\" NOVBRMOVE\n  EndCLI >NIL:\n");
    CHECK(slurp(root + "/S/Startup-Sequence.orig") == "C:SetPatch QUIET\nVersion\n  EndCLI >NIL:\n");
    CHECK(whd_prepare_launch(root.c_str(), "alpha", kdir.c_str(), nullptr, err, sizeof err));   /* idempotent */
    CHECK(slurp(root + "/S/Startup-Sequence") == "C:SetPatch QUIET\nVersion\n; uae3DSng WHDLoad launcher (rewritten for every start)\nCD \"SYS:alpha\"\nC:WHDLoad \"Other.slave\" PRELOAD\n  EndCLI >NIL:\n");
    printf("  unit tests done, %d failure(s) so far\n", g_fail);
}

/* Archives that are expected NOT to extract, and why. Everything else (all Amiga/WHDLoad-relevant formats: LHA 1.22 and
   2.12 on the Amiga, MorphOS, Unix, Windows, ... with header levels 0-2 and the methods lh0/lh1/lh4/lh5/lh6/lh7) must. */
static const char *expected_failure(const std::string &rel)
{
    if (rel.find("/regression/") == 0)                 return "corrupt by design";
    if (rel == "/unlha32/lh2.lzh" || rel == "/unlha32/lh3.lzh") return "method not supported by Lhasa (never used on the Amiga)";
    if (rel == "/lh2_222/easubdir.lzh")                return "OS/2 extended-attribute file clashes with a folder name";
    if (rel == "/lha_osk_201/h0_subdir.lzh" || rel == "/lha_unix114i/h0_subdir.lzh")
                                                       return "nameless folder entries: Lhasa's reader returns no files";
    if (rel.find("/maclha_224/") == 0)                 return "MacBinary wrapped (Macintosh), not used on the Amiga";
    return nullptr;
}

struct Progress { unsigned calls = 0; unsigned last_done = 0; bool monotonic = true; uint64_t last_bytes = 0; };
static int progress_cb(void *u, const char *item, unsigned done, unsigned total, uint64_t bd, uint64_t bt)
{
    Progress *p = (Progress *)u;
    p->calls++;
    if (bd < p->last_bytes || done > total || bd > bt) p->monotonic = false;
    p->last_bytes = bd;
    return 0;
}
static int cancel_cb(void *, const char *, unsigned, unsigned, uint64_t, uint64_t) { return 1; }

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s <archive dir> <work dir>\n", argv[0]); return 2; }
    std::string work = argv[2];
    mkdir(work.c_str(), 0777);
    unit_tests(work);

    std::vector<std::string> archives;
    walk(argv[1], archives);
    printf("== %zu archives\n", archives.size());
    int ok = 0, fail = 0, xfail = 0, idx = 0;
    std::map<unsigned long long, std::vector<std::string>> gpl;    /* hash of gpl-2 -> archives */
    std::vector<std::string> gz_mismatch;
    long gpl_size = 0;
    for (auto &a : archives) {
        std::string root = work + "/r" + std::to_string(idx++);
        mkdir(root.c_str(), 0777);
        char folder[WHD_NAME_MAX], err[256];
        whd_install_stats st;
        Progress pr;
        int r = whd_install_lha(root.c_str(), a.c_str(), progress_cb, &pr, folder, sizeof folder, &st, err, sizeof err);
        std::string rel = a.substr(strlen(argv[1]));
        if (r) {
            ok++;
            printf("OK   %-52s files=%u bytes=%llu renamed=%u calls=%u%s\n", rel.c_str(), st.files,
                   (unsigned long long)st.bytes, st.renamed, pr.calls, pr.monotonic ? "" : " PROGRESS-NOT-MONOTONIC");
            if (!pr.monotonic) g_fail++;
            std::string g = root + "/" + folder + "/gpl-2";
            long sz;
            if (!expected_failure(rel))      /* (Mac archives wrap the data in MacBinary: other bytes) */
                if (unsigned long long h = fnv_file(g, &sz)) { gpl[h].push_back(rel); gpl_size = sz; }
            std::string gz = root + "/" + folder + "/gpl-2.gz";
            if (!expected_failure(rel) && access(gz.c_str(), 0) == 0) {
                std::string gp = root + "/gz_" ; (void)gp;
                gz_mismatch.push_back(gz);            /* compared below, after the reference hash is known */
            }
        } else {
            fail++;
            if (const char *why = expected_failure(rel)) {
                xfail++;
                printf("XFAIL %-51s %s  [%s]\n", rel.c_str(), err, why);
            } else {
                printf("FAIL %-52s %s\n", rel.c_str(), err);
                g_fail++;
            }
        }
    }
    printf("== %d installed, %d failed (%d of them expected)\n", ok, fail, xfail);

    printf("== cross-check of gpl-2\n");
    printf("  %zu distinct content hash(es) over %d archives, size %ld\n", gpl.size(), (int)[&]{int c=0; for (auto &kv: gpl) c += kv.second.size(); return c;}(), gpl_size);
    CHECK(gpl.size() == 1);
    if (gpl.size() == 1) {
        unsigned long long ref = gpl.begin()->first;
        int gzok = 0;
        for (auto &gz : gz_mismatch) { if (fnv_gunzip(gz) == ref) gzok++; else { printf("  gz mismatch: %s\n", gz.c_str()); g_fail++; } }
        printf("  %d stored gpl-2.gz files gunzip to the same bytes\n", gzok);
    } else {
        for (auto &kv : gpl) { printf("  hash %016llx:", kv.first); for (auto &s : kv.second) printf(" %s", s.c_str()); printf("\n"); }
    }

    /* cancel */
    if (!archives.empty()) {
        std::string root = work + "/cancel"; mkdir(root.c_str(), 0777);
        char folder[WHD_NAME_MAX], err[256];
        int r = whd_install_lha(root.c_str(), archives[0].c_str(), cancel_cb, nullptr, folder, sizeof folder, nullptr, err, sizeof err);
        CHECK(!r && !strcmp(err, "Cancelled"));
    }
    /* not an archive / missing */
    {
        std::string root = work + "/bad"; mkdir(root.c_str(), 0777);
        char folder[WHD_NAME_MAX], err[256];
        write_file(work + "/garbage.lha", "this is not an lha archive, just text. ......................................");
        CHECK(!whd_install_lha(root.c_str(), (work + "/garbage.lha").c_str(), nullptr, nullptr, folder, sizeof folder, nullptr, err, sizeof err));
        printf("  garbage -> '%s'\n", err);
        CHECK(!whd_install_lha(root.c_str(), (work + "/missing.lha").c_str(), nullptr, nullptr, folder, sizeof folder, nullptr, err, sizeof err));
        printf("  missing -> '%s'\n", err);
    }
    printf("== result: %s (%d check failure(s))\n", g_fail ? "FAILED" : "all checks passed", g_fail);
    return g_fail ? 1 : 0;
}
