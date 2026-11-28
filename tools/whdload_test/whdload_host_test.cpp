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
#include "ofs_reader.h"

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
    CHECK(!whd_prepare_launch(root.c_str(), "nogame", nullptr, nullptr, nullptr, err, sizeof err) && strstr(err, "not found"));
    CHECK(!whd_prepare_launch(root.c_str(), "../x", nullptr, nullptr, nullptr, err, sizeof err));
    CHECK(!whd_prepare_launch(root.c_str(), "alpha", nullptr, nullptr, nullptr, err, sizeof err) && strstr(err, "WHDLoad is missing"));
    /* built-in WHDLoad (romfs on the 3DS): used when the SD card has none, never replaces a file that is there */
    static std::string builtin;                                                   /* whd_builtin_whdload points to it for the rest of the run */
    builtin = work + "/builtin_WHDLoad";
    write_file(builtin, "BUILTIN-WHDLOAD");
    whd_builtin_whdload = builtin.c_str();
    CHECK(!whd_prepare_launch(root.c_str(), "alpha", nullptr, nullptr, nullptr, err, sizeof err) && strstr(err, "No .slave"));  /* got past the check */
    CHECK(slurp(root + "/C/WHDLoad") == "BUILTIN-WHDLOAD");
    write_file(root + "/C/WHDLoad", "binary");                                    /* the user's own copy */
    write_file(builtin, "OTHER-BUILTIN");
    CHECK(!whd_prepare_launch(root.c_str(), "alpha", nullptr, nullptr, nullptr, err, sizeof err) && strstr(err, "No .slave"));
    CHECK(slurp(root + "/C/WHDLoad") == "binary");                                /* untouched */
    remove((root + "/C/WHDLoad").c_str());
    remove((root + "/C").c_str());                                                /* even the C folder is created */
    CHECK(!whd_prepare_launch(root.c_str(), "alpha", nullptr, nullptr, nullptr, err, sizeof err) && strstr(err, "No .slave"));
    CHECK(slurp(root + "/C/WHDLoad") == "OTHER-BUILTIN");
    write_file(root + "/C/WHDLoad", "binary");
    CHECK(!whd_prepare_launch(root.c_str(), "alpha", nullptr, nullptr, nullptr, err, sizeof err) && strstr(err, "No .slave"));

    mkdir((root + "/alpha/Game").c_str(), 0777);
    write_file(root + "/alpha/Game/Game.slave", "s");
    write_file(root + "/alpha/Game/GameCD.slave", "s");
    write_file(root + "/alpha/Other.slave", "s");
    std::string kdir = work + "/kicks";
    mkdir(kdir.c_str(), 0777);
    write_file(kdir + "/kick13.rom", "ROM13");
    write_file(kdir + "/Kick34005.A500.RTB", "rtb");
    write_file(kdir + "/kick31.rom", "ROM31");
    CHECK(whd_prepare_launch(root.c_str(), "alpha", kdir.c_str(), nullptr, nullptr, err, sizeof err));
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
    CHECK(whd_prepare_launch(root.c_str(), "alpha", kdir.c_str(), "NOVBRMOVE", nullptr, err, sizeof err));
    ss = slurp(root + "/S/Startup-Sequence");
    CHECK(ss == "C:SetPatch QUIET\nVersion\n; uae3DSng WHDLoad launcher (rewritten for every start)\nCD \"SYS:alpha\"\nC:WHDLoad \"Other.slave\" NOVBRMOVE\n  EndCLI >NIL:\n");
    CHECK(slurp(root + "/S/Startup-Sequence.orig") == "C:SetPatch QUIET\nVersion\n  EndCLI >NIL:\n");
    CHECK(whd_prepare_launch(root.c_str(), "alpha", kdir.c_str(), nullptr, nullptr, err, sizeof err));   /* idempotent */
    CHECK(slurp(root + "/S/Startup-Sequence") == "C:SetPatch QUIET\nVersion\n; uae3DSng WHDLoad launcher (rewritten for every start)\nCD \"SYS:alpha\"\nC:WHDLoad \"Other.slave\" PRELOAD\n  EndCLI >NIL:\n");

    /* the relocation tables that ship with the app (romfs on the 3DS): copied when missing, never replacing a file */
    static std::string bk;                                  /* whd_builtin_kickstarts points to it while this runs */
    bk = work + "/builtin_kicks";
    mkdir(bk.c_str(), 0777);
    write_file(bk + "/kick34005.A500.RTB", "builtin-rtb-13");
    write_file(bk + "/kick33180.A500.PAT", "builtin-pat-12");
    whd_builtin_kickstarts = bk.c_str();
    std::string root2 = work + "/fake2";
    mkdir(root2.c_str(), 0777);
    mkdir((root2 + "/g").c_str(), 0777);
    write_file(root2 + "/g/G.slave", "s");
    CHECK(whd_prepare_launch(root2.c_str(), "g", nullptr, nullptr, nullptr, err, sizeof err));       /* no kickstart folder at all */
    CHECK(slurp(root2 + "/Devs/Kickstarts/kick34005.A500.RTB") == "builtin-rtb-13");
    CHECK(slurp(root2 + "/Devs/Kickstarts/kick33180.A500.PAT") == "builtin-pat-12");
    write_file(root2 + "/Devs/Kickstarts/kick34005.A500.RTB", "mine");
    CHECK(whd_prepare_launch(root2.c_str(), "g", "/nonexistent-kick-dir", nullptr, nullptr, err, sizeof err));
    CHECK(slurp(root2 + "/Devs/Kickstarts/kick34005.A500.RTB") == "mine");                           /* never replaced */
    std::string root3 = work + "/fake3";
    mkdir(root3.c_str(), 0777);
    mkdir((root3 + "/g").c_str(), 0777);
    write_file(root3 + "/g/G.slave", "s");
    std::string kd3 = work + "/kicks3";
    mkdir(kd3.c_str(), 0777);
    write_file(kd3 + "/kick34005.A500.RTB", "the-users-own-table");
    CHECK(whd_prepare_launch(root3.c_str(), "g", kd3.c_str(), nullptr, nullptr, err, sizeof err));
    CHECK(slurp(root3 + "/Devs/Kickstarts/kick34005.A500.RTB") == "the-users-own-table");           /* the user's file wins */
    CHECK(slurp(root3 + "/Devs/Kickstarts/kick33180.A500.PAT") == "builtin-pat-12");
    CHECK(whd_prepare_launch(root3.c_str(), "g", kd3.c_str(), nullptr, nullptr, err, sizeof err));  /* idempotent */
    CHECK(slurp(root3 + "/Devs/Kickstarts/kick34005.A500.RTB") == "the-users-own-table");
    whd_builtin_kickstarts = nullptr;
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

/* ---------------------------------------------------------------------------------------------------------------- */
/* image installs                                                                                                   */
/* ---------------------------------------------------------------------------------------------------------------- */

/* A tiny LHA writer (header level 0, method -lh0-, i.e. stored) to build archives with exactly the content we want */
struct LhaEntry { std::string name, data; bool dir = false; int crc_error = 0; };

static uint16_t crc16_arc(const std::string &d)
{
    uint16_t crc = 0;
    for (unsigned char c : d) {
        crc ^= c;
        for (int i = 0; i < 8; i++) crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
    return crc;
}

static void make_lha(const std::string &path, const std::vector<LhaEntry> &entries)
{
    std::string out;
    for (auto &e : entries) {
        std::string h;
        h += e.dir ? "-lhd-" : "-lh0-";
        uint32_t sz = (uint32_t)e.data.size();
        for (int i = 0; i < 2; i++) for (int b = 0; b < 4; b++) h += (char)(sz >> (8 * b));   /* packed, original */
        h += std::string("\x00\x00\x21\x00", 4);          /* DOS time 0, date 1980-01-01 */
        h += (char)0x20;                                  /* attribute */
        h += (char)0;                                     /* level 0 */
        h += (char)e.name.size();
        h += e.name;
        uint16_t crc = e.dir ? 0 : crc16_arc(e.data);
        if (e.crc_error) crc ^= 0x55;
        h += (char)(crc & 0xff);
        h += (char)(crc >> 8);
        unsigned char sum = 0;
        for (unsigned char c : h) sum += c;
        out += (char)h.size();
        out += (char)sum;
        out += h;
        out += e.data;
    }
    out += '\0';
    FILE *f = fopen(path.c_str(), "wb"); fwrite(out.data(), 1, out.size(), f); fclose(f);
}

static std::string pseudo(size_t n, unsigned seed)
{
    std::string s(n, 0);
    unsigned x = seed * 2654435761u + 1;
    for (size_t i = 0; i < n; i++) { x = x * 1664525u + 1013904223u; s[i] = (char)(x >> 24); }
    return s;
}

static bool exists(const std::string &p) { return access(p.c_str(), 0) == 0; }

static bool launch_ok(const char *root, const char *game, const char *kdir, const char *args, whd_launch_info *li)
{
    char err[256];
    bool ok = whd_prepare_launch(root, game, kdir, args, li, err, sizeof err) != 0;
    if (!ok) { printf("  launch of '%s' failed: %s\n", game, err); g_fail++; }
    return ok;
}

static void collect_files(const std::string &dir, const std::string &rel, std::map<std::string, std::string> &out)
{
    DIR *d = opendir(dir.c_str());
    if (!d) return;
    while (dirent *e = readdir(d)) {
        if (e->d_name[0] == '.') continue;
        std::string p = dir + "/" + e->d_name, r = rel.empty() ? e->d_name : rel + "/" + e->d_name;
        if (is_dir(p)) { out[r + "/"] = ""; collect_files(p, r, out); }
        else out[r] = slurp(p);
    }
    closedir(d);
}

static void image_tests(const std::string &work)
{
    printf("== image installs (hand made archives)\n");
    std::string root = work + "/img";
    mkdir(root.c_str(), 0777);
    char folder[WHD_NAME_MAX], err[256];
    whd_install_stats st;
    char names[16][WHD_NAME_MAX];

    /* 1. a normal game */
    std::string big = pseudo(300000, 7);
    std::vector<LhaEntry> game = {
        { "TestGame/", "", true },
        { "TestGame/TestGame.slave", pseudo(5000, 1) },
        { "TestGame/TestGameCD.slave", pseudo(100, 2) },
        { "TestGame/data/level1.dat", big },
        { "TestGame/data/empty.bin", "" },
        { "TestGame/Empty/", "", true },
        { "TestGame/A_very_long_file_name_that_exceeds_thirty_chars.txt", "long" },
        { "TestGame/Caf\xe9:x", "latin1 and a colon" },
    };
    make_lha(work + "/Test Game.lha", game);
    Progress pr;
    whd_image_requires_slave = 1;
    CHECK(whd_install_lha(root.c_str(), (work + "/Test Game.lha").c_str(), WHD_INSTALL_IMAGE, progress_cb, &pr, folder,
                          sizeof folder, &st, err, sizeof err));
    printf("  installed '%s': %u files, %llu bytes, image %llu bytes, renamed %u, progress calls %u\n", folder, st.files,
           (unsigned long long)st.bytes, (unsigned long long)st.image_bytes, st.renamed, pr.calls);
    CHECK(!strcmp(folder, "Test Game"));
    CHECK(st.files == 6 && st.renamed == 1 && st.has_slave);
    CHECK(pr.monotonic && pr.calls > 5);
    std::string hdf = root + "/Test Game.hdf";
    CHECK(exists(hdf) && exists(root + "/Test Game.whd") && !exists(root + "/Test Game.hdf.tmp"));
    CHECK(!exists(root + "/Test Game"));                               /* no folder: everything is in the image */
    {
        struct stat sb; CHECK(stat(hdf.c_str(), &sb) == 0 && (uint64_t)sb.st_size == st.image_bytes);
    }
    OfsRead rd; OfsChecker chk; std::string cerr;
    bool valid = chk.run(hdf, rd, cerr);
    if (!valid) printf("  checker: %s\n", cerr.c_str());
    CHECK(valid);
    CHECK(rd.volname == "WHDGame");
    CHECK(rd.files.size() == 6);
    CHECK(rd.dirs.count("TestGame") && rd.dirs.count("TestGame/data") && rd.dirs.count("TestGame/Empty"));
    auto content = [&](const std::string &n) { auto it = rd.files.find(n); return it == rd.files.end() ? std::string("<missing>") : std::string(it->second.begin(), it->second.end()); };
    CHECK(content("TestGame/data/level1.dat") == big);
    CHECK(content("TestGame/TestGame.slave") == pseudo(5000, 1));
    CHECK(content("TestGame/data/empty.bin") == "");
    CHECK(content("TestGame/A_very_long_file_name_that.txt") == "long");        /* cut to 30 characters, ".txt" stays */
    CHECK(content("TestGame/Caf\xe9_x") == "latin1 and a colon");                  /* bytes kept, ':' replaced */
    std::string info = slurp(root + "/Test Game.whd");
    CHECK(info.find("slave=TestGame/TestGame.slave\n") != std::string::npos);
    int n = whd_list_games(root.c_str(), names, 16);
    CHECK(n == 1 && !strcmp(names[0], "Test Game"));

    /* launch */
    std::string kdir = work + "/kicks";
    whd_launch_info li;
    launch_ok(root.c_str(), "Test Game", kdir.c_str(), nullptr, &li);
    CHECK(std::string(li.hdf_path) == hdf);
    std::string ss = slurp(root + "/S/Startup-Sequence");
    printf("  Startup-Sequence:\n%s", ss.c_str());
    CHECK(ss == "; uae3DSng WHDLoad launcher (rewritten for every start)\nCD \"WHDGame:TestGame\"\nC:WHDLoad \"TestGame.slave\" PRELOAD\n");

    /* installing again replaces the image (no leftovers) */
    CHECK(whd_install_lha(root.c_str(), (work + "/Test Game.lha").c_str(), WHD_INSTALL_IMAGE, nullptr, nullptr, folder,
                          sizeof folder, &st, err, sizeof err));
    CHECK((rd = OfsRead(), chk.run(hdf, rd, cerr)) && rd.files.size() == 6);

    /* an old folder install of the same name does not show up twice, the image wins */
    mkdir((root + "/Test Game").c_str(), 0777);
    mkdir((root + "/Old Folder Game").c_str(), 0777);
    n = whd_list_games(root.c_str(), names, 16);
    CHECK(n == 2 && !strcmp(names[0], "Old Folder Game") && !strcmp(names[1], "Test Game"));
    CHECK(launch_ok(root.c_str(), "Test Game", kdir.c_str(), nullptr, &li) && li.hdf_path[0]);
    rmdir((root + "/Test Game").c_str());
    rmdir((root + "/Old Folder Game").c_str());

    /* 2. the choice of the slave: closest to the top, nothing deeper than 4 folders, then the shortest name */
    make_lha(work + "/Deep.lha", { { "g/a/b/c/d/Deep.slave", "x" }, { "g/a/Mid.slave", "x" }, { "g/Z.slave", "x" }, { "g/Zz.slave", "x" } });
    CHECK(whd_install_lha(root.c_str(), (work + "/Deep.lha").c_str(), WHD_INSTALL_IMAGE, nullptr, nullptr, folder, sizeof folder, &st, err, sizeof err));
    launch_ok(root.c_str(), "Deep", kdir.c_str(), "NOVBRMOVE", &li);
    CHECK(slurp(root + "/S/Startup-Sequence") == "; uae3DSng WHDLoad launcher (rewritten for every start)\nCD \"WHDGame:g\"\nC:WHDLoad \"Z.slave\" NOVBRMOVE\n");

    make_lha(work + "/Rootslave.lha", { { "Root.slave", "x" }, { "Dir/Sub.slave", "x" } });
    CHECK(whd_install_lha(root.c_str(), (work + "/Rootslave.lha").c_str(), WHD_INSTALL_IMAGE, nullptr, nullptr, folder, sizeof folder, &st, err, sizeof err));
    launch_ok(root.c_str(), "Rootslave", kdir.c_str(), nullptr, &li);
    CHECK(slurp(root + "/S/Startup-Sequence") == "; uae3DSng WHDLoad launcher (rewritten for every start)\nCD \"WHDGame:\"\nC:WHDLoad \"Root.slave\" PRELOAD\n");

    /* names with the characters that are special in an AmigaDOS command line */
    make_lha(work + "/Quotes.lha", { { "Q\"d*/We\"ird*.slave", "x" } });
    CHECK(whd_install_lha(root.c_str(), (work + "/Quotes.lha").c_str(), WHD_INSTALL_IMAGE, nullptr, nullptr, folder, sizeof folder, &st, err, sizeof err));
    launch_ok(root.c_str(), "Quotes", kdir.c_str(), nullptr, &li);
    CHECK(slurp(root + "/S/Startup-Sequence") == "; uae3DSng WHDLoad launcher (rewritten for every start)\nCD \"WHDGame:Q*\"d**\"\nC:WHDLoad \"We*\"ird**.slave\" PRELOAD\n");

    /* a slave whose name is 31 characters long: cut to 30, but it must stay a .slave file that WHDLoad is started with */
    make_lha(work + "/Fate.lha", { { "Fate/", "", true }, { "Fate/FateGatesOfDawnUncensored.slave", "x" }, { "Fate/FateGatesOfDawnUncensored.info", "i" } });
    CHECK(whd_install_lha(root.c_str(), (work + "/Fate.lha").c_str(), WHD_INSTALL_IMAGE, nullptr, nullptr, folder, sizeof folder, &st, err, sizeof err));
    CHECK(st.has_slave && st.renamed == 1);
    launch_ok(root.c_str(), "Fate", kdir.c_str(), nullptr, &li);
    CHECK(slurp(root + "/S/Startup-Sequence") == "; uae3DSng WHDLoad launcher (rewritten for every start)\nCD \"WHDGame:Fate\"\nC:WHDLoad \"FateGatesOfDawnUncensore.slave\" PRELOAD\n");
    CHECK(slurp(root + "/Fate.whd").find("slave=Fate/FateGatesOfDawnUncensore.slave\n") != std::string::npos);
    CHECK((rd = OfsRead(), chk.run(root + "/Fate.hdf", rd, cerr)) && rd.files.count("Fate/FateGatesOfDawnUncensore.slave") && rd.files.count("Fate/FateGatesOfDawnUncensored.info"));

    /* 3. failures leave nothing behind */
    auto nothing_left = [&](const char *base) {
        return !exists(root + "/" + base + ".hdf") && !exists(root + "/" + base + ".hdf.tmp") && !exists(root + "/" + base + ".whd");
    };
    make_lha(work + "/Noslave.lha", { { "G/readme.txt", "hello" } });
    CHECK(!whd_install_lha(root.c_str(), (work + "/Noslave.lha").c_str(), WHD_INSTALL_IMAGE, nullptr, nullptr, folder, sizeof folder, &st, err, sizeof err) && strstr(err, "not a WHDLoad game"));
    CHECK(nothing_left("Noslave"));
    make_lha(work + "/Badcrc.lha", { { "G/G.slave", "hello" }, { "G/data", pseudo(70000, 3), false, 1 } });
    CHECK(!whd_install_lha(root.c_str(), (work + "/Badcrc.lha").c_str(), WHD_INSTALL_IMAGE, nullptr, nullptr, folder, sizeof folder, &st, err, sizeof err) && strstr(err, "CRC error"));
    printf("  bad crc -> '%s'\n", err);
    CHECK(nothing_left("Badcrc"));
    CHECK(!whd_install_lha(root.c_str(), (work + "/Test Game.lha").c_str(), WHD_INSTALL_IMAGE, cancel_cb, nullptr, folder, sizeof folder, &st, err, sizeof err) && !strcmp(err, "Cancelled"));
    CHECK(chk.run(hdf, rd, cerr));                                      /* the cancelled install must not have touched the old image */
    make_lha(work + "/Dots.lha", { { "../evil.slave", "x" }, { "a/../../b.txt", "y" } });   /* Lhasa collapses "..", an image cannot be left anyway */
    CHECK(whd_install_lha(root.c_str(), (work + "/Dots.lha").c_str(), WHD_INSTALL_IMAGE, nullptr, nullptr, folder, sizeof folder, &st, err, sizeof err));
    CHECK((rd = OfsRead(), chk.run(root + "/Dots.hdf", rd, cerr)) && rd.files.size() == 2 && rd.files.count("evil.slave") && rd.files.count("b.txt"));
    /* the same name twice: the first one is kept, the install does not fail */
    make_lha(work + "/Twice.lha", { { "T/T.slave", "first" }, { "T/data", "one" }, { "T/data", "two" }, { "T/T.slave", "third" } });
    CHECK(whd_install_lha(root.c_str(), (work + "/Twice.lha").c_str(), WHD_INSTALL_IMAGE, nullptr, nullptr, folder, sizeof folder, &st, err, sizeof err));
    CHECK(st.files == 2 && st.skipped == 2);
    CHECK((rd = OfsRead(), chk.run(root + "/Twice.hdf", rd, cerr)) && rd.files.size() == 2 && content("T/data") == "one" && content("T/T.slave") == "first");
    CHECK(!whd_install_lha(root.c_str(), (work + "/garbage.lha").c_str(), WHD_INSTALL_IMAGE, nullptr, nullptr, folder, sizeof folder, &st, err, sizeof err));
    CHECK(nothing_left("garbage"));
    CHECK(!whd_install_lha(root.c_str(), (work + "/missing.lha").c_str(), WHD_INSTALL_IMAGE, nullptr, nullptr, folder, sizeof folder, &st, err, sizeof err));
    printf("  missing -> '%s'\n", err);

    /* a damaged info file */
    write_file(root + "/Test Game.whd", "; nothing useful\n");
    CHECK(!whd_prepare_launch(root.c_str(), "Test Game", kdir.c_str(), nullptr, &li, err, sizeof err) && strstr(err, "damaged"));
    /* image without info file: not a game (not listed) */
    remove((root + "/Test Game.whd").c_str());
    n = whd_list_games(root.c_str(), names, 16);
    bool listed = false; for (int i = 0; i < n; i++) if (!strcmp(names[i], "Test Game")) listed = true;
    CHECK(!listed);
    printf("  image tests done, %d failure(s) so far\n", g_fail);
}

/* Install an archive of the Lhasa test suite as image and as folder: the same files with the same contents? */
static void compare_image_and_folder(const std::string &rel, const std::string &folder_root, const std::string &folder,
                                     const whd_install_stats &fst, const std::string &image_root, const std::string &archive)
{
    char name[WHD_NAME_MAX], err[256];
    whd_install_stats ist;
    int r = whd_install_lha(image_root.c_str(), archive.c_str(), WHD_INSTALL_IMAGE, nullptr, nullptr, name, sizeof name, &ist, err, sizeof err);
    if (!r) {
        if (strstr(err, "too big")) {                  /* a file of more than 4 GB cannot be put into an image */
            printf("  IMAGE XFAIL %-43s %s\n", rel.c_str(), err);
            return;
        }
        printf("  IMAGE FAIL %-44s %s\n", rel.c_str(), err);
        g_fail++;
        return;
    }
    OfsRead rd; OfsChecker chk; std::string cerr;
    if (!chk.run(image_root + "/" + name + ".hdf", rd, cerr)) {
        printf("  IMAGE INVALID %-41s %s\n", rel.c_str(), cerr.c_str());
        g_fail++;
        return;
    }
    if (ist.skipped) {                                 /* names used twice: the folder install keeps the last, the image the first */
        if (ist.files + ist.skipped != fst.files || rd.files.size() != ist.files) {
            printf("  IMAGE MISMATCH %-40s files %u+%u skipped / %u\n", rel.c_str(), ist.files, ist.skipped, fst.files);
            g_fail++;
        }
        return;
    }
    if (ist.files != fst.files || ist.bytes != fst.bytes || rd.files.size() != fst.files) {
        printf("  IMAGE MISMATCH %-40s files %u/%u bytes %llu/%llu in image %zu\n", rel.c_str(), ist.files, fst.files,
               (unsigned long long)ist.bytes, (unsigned long long)fst.bytes, rd.files.size());
        g_fail++;
        return;
    }
    if (ist.renamed) return;                           /* names were cut: only the totals are comparable */
    std::map<std::string, std::string> disk;
    collect_files(folder_root + "/" + folder, "", disk);
    std::map<std::string, std::string> img;
    for (auto &kv : rd.files) {
        char fat[600];
        if (!whd_safe_relative_path(kv.first.c_str(), fat, sizeof fat, nullptr)) { printf("  IMAGE BAD NAME %s\n", rel.c_str()); g_fail++; return; }
        img[fat] = std::string(kv.second.begin(), kv.second.end());
    }
    for (auto &kv : disk) {
        if (!kv.first.empty() && kv.first.back() == '/') continue;     /* folders: compared below */
        auto it = img.find(kv.first);
        if (it == img.end() || it->second != kv.second) {
            printf("  IMAGE CONTENT DIFFERS %-34s %s\n", rel.c_str(), kv.first.c_str());
            g_fail++;
            return;
        }
    }
    for (auto &kv : disk)
        if (!kv.first.empty() && kv.first.back() == '/') {
            std::string d = kv.first.substr(0, kv.first.size() - 1);
            bool found = false;
            for (auto &x : rd.dirs) { char fat[600]; if (whd_safe_relative_path(x.c_str(), fat, sizeof fat, nullptr) && d == fat) found = true; }
            if (!found) { printf("  IMAGE MISSING FOLDER %-35s %s\n", rel.c_str(), d.c_str()); g_fail++; return; }
        }
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s <archive dir> <work dir>\n", argv[0]); return 2; }
    std::string work = argv[2];
    mkdir(work.c_str(), 0777);
    unit_tests(work);
    image_tests(work);

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
        int r = whd_install_lha(root.c_str(), a.c_str(), WHD_INSTALL_FOLDER, progress_cb, &pr, folder, sizeof folder, &st, err, sizeof err);
        std::string rel = a.substr(strlen(argv[1]));
        if (r) {
            ok++;
            printf("OK   %-52s files=%u bytes=%llu renamed=%u calls=%u%s\n", rel.c_str(), st.files,
                   (unsigned long long)st.bytes, st.renamed, pr.calls, pr.monotonic ? "" : " PROGRESS-NOT-MONOTONIC");
            if (!pr.monotonic) g_fail++;
            whd_image_requires_slave = 0;                    /* the Lhasa test archives are no WHDLoad games */
            compare_image_and_folder(rel, root, folder, st, work + "/image" + std::to_string(idx), a);
            whd_image_requires_slave = 1;
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
        int r = whd_install_lha(root.c_str(), archives[0].c_str(), WHD_INSTALL_FOLDER, cancel_cb, nullptr, folder, sizeof folder, nullptr, err, sizeof err);
        CHECK(!r && !strcmp(err, "Cancelled"));
    }
    /* not an archive / missing */
    {
        std::string root = work + "/bad"; mkdir(root.c_str(), 0777);
        char folder[WHD_NAME_MAX], err[256];
        write_file(work + "/garbage.lha", "this is not an lha archive, just text. ......................................");
        CHECK(!whd_install_lha(root.c_str(), (work + "/garbage.lha").c_str(), WHD_INSTALL_FOLDER, nullptr, nullptr, folder, sizeof folder, nullptr, err, sizeof err));
        printf("  garbage -> '%s'\n", err);
        CHECK(!whd_install_lha(root.c_str(), (work + "/missing.lha").c_str(), WHD_INSTALL_FOLDER, nullptr, nullptr, folder, sizeof folder, nullptr, err, sizeof err));
        printf("  missing -> '%s'\n", err);
    }
    printf("== result: %s (%d check failure(s))\n", g_fail ? "FAILED" : "all checks passed", g_fail);
    return g_fail ? 1 : 0;
}
