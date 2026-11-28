/*
 * Host (PC) test for src/3ds/whdload3ds.cpp - see run.sh.
 *
 *   whdload_host_test <dir with .lzh/.lha files, searched recursively> <empty work dir>
 *
 * 1. unit tests of the name helper, game list, launch preparation, built-in Kickstart tables
 * 2. hand made archives installed as disk images (and started / uninstalled)
 * 3. installs every archive found as a disk image into its own root and prints OK/FAIL. Every successful install has
 *    been verified against the CRC-16 and the length stored in the archive, and the image against an independent OFS
 *    checker. All files named "gpl-2" (the same text, packed by ~40 different LHA programs and methods) must
 *    additionally be byte-identical, and "gpl-2.gz" (stored) must gunzip to the very same bytes.
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
    char f[128];
    whd_game_name_from_archive("sdmc:/roms/ShadowOfTheBeast_v1.2.lha", f, sizeof f); CHECK(!strcmp(f, "ShadowOfTheBeast_v1.2"));
    whd_game_name_from_archive("C:\\x\\Game One.LZH", f, sizeof f);                   CHECK(!strcmp(f, "Game One"));
    whd_game_name_from_archive("/a/C.lha", f, sizeof f);                              CHECK(!strcmp(f, "C_game"));
    whd_game_name_from_archive("/a/..lha", f, sizeof f);                              CHECK(!strcmp(f, "game"));
    whd_game_name_from_archive("/a/we?ird:name.lha", f, sizeof f);                    CHECK(!strcmp(f, "we_ird_name") || !strcmp(f, "name"));

    /* game list + launch preparation on a fake tree: images need the .hdf and the .whd, nothing else is a game */
    std::string root = work + "/fake";
    mkdir(root.c_str(), 0777);
    for (const char *d : { "/Zebra", "/S", "/C", "/Devs", "/.hidden", "/Beta", "/Oldstyle" }) mkdir((root + d).c_str(), 0777);   /* folders are no games */
    write_file(root + "/notadir", "x");
    write_file(root + "/Zebra.hdf", "img");
    write_file(root + "/Zebra.whd", "slave=Zebra/Zebra.slave\n");
    write_file(root + "/alpha.hdf", "img");
    write_file(root + "/alpha.whd", "; uae3DSng WHDLoad game image\nslave=alpha/Game/Other.slave\n");
    write_file(root + "/Beta.hdf", "img");
    write_file(root + "/Beta.whd", "slave=Beta.slave\n");
    write_file(root + "/NoInfo.hdf", "img");                      /* image without info file */
    write_file(root + "/OnlyInfo.whd", "slave=x.slave\n");       /* info without image */
    write_file(root + "/Empty.hdf", "");
    write_file(root + "/Empty.whd", "slave=x.slave\n");          /* empty image */
    write_file(root + "/.hid.hdf", "img");
    write_file(root + "/.hid.whd", "slave=x.slave\n");
    char names[16][WHD_NAME_MAX];
    int n = whd_list_games(root.c_str(), names, 16, nullptr);
    CHECK(n == 3 && !strcmp(names[0], "alpha") && !strcmp(names[1], "Beta") && !strcmp(names[2], "Zebra"));

    char err[200];
    CHECK(!whd_prepare_launch(root.c_str(), "nogame", nullptr, nullptr, nullptr, err, sizeof err) && strstr(err, "not found"));
    CHECK(!whd_prepare_launch(root.c_str(), "Oldstyle", nullptr, nullptr, nullptr, err, sizeof err) && strstr(err, "not found"));   /* a folder is no game */
    CHECK(!whd_prepare_launch(root.c_str(), "NoInfo", nullptr, nullptr, nullptr, err, sizeof err) && strstr(err, "not found"));
    CHECK(!whd_prepare_launch(root.c_str(), "../x", nullptr, nullptr, nullptr, err, sizeof err));
    CHECK(!whd_prepare_launch(root.c_str(), "alpha", nullptr, nullptr, nullptr, err, sizeof err) && strstr(err, "WHDLoad is missing"));
    /* built-in WHDLoad (romfs on the 3DS): used when the SD card has none, never replaces a file that is there */
    write_file(root + "/Damaged.hdf", "img");
    write_file(root + "/Damaged.whd", "; nothing useful\n");                  /* a game whose launch fails after the WHDLoad check */
    static std::string builtin;                                                   /* whd_builtin_whdload points to it for the rest of the run */
    builtin = work + "/builtin_WHDLoad";
    write_file(builtin, "BUILTIN-WHDLOAD");
    whd_builtin_whdload = builtin.c_str();
    CHECK(!whd_prepare_launch(root.c_str(), "Damaged", nullptr, nullptr, nullptr, err, sizeof err) && strstr(err, "damaged"));  /* got past the check */
    CHECK(slurp(root + "/C/WHDLoad") == "BUILTIN-WHDLOAD");
    write_file(root + "/C/WHDLoad", "binary");                                    /* the user's own copy */
    write_file(builtin, "OTHER-BUILTIN");
    CHECK(!whd_prepare_launch(root.c_str(), "Damaged", nullptr, nullptr, nullptr, err, sizeof err) && strstr(err, "damaged"));
    CHECK(slurp(root + "/C/WHDLoad") == "binary");                                /* untouched */
    remove((root + "/C/WHDLoad").c_str());
    remove((root + "/C").c_str());                                                /* even the C folder is created */
    CHECK(!whd_prepare_launch(root.c_str(), "Damaged", nullptr, nullptr, nullptr, err, sizeof err) && strstr(err, "damaged"));
    CHECK(slurp(root + "/C/WHDLoad") == "OTHER-BUILTIN");
    write_file(root + "/C/WHDLoad", "binary");

    std::string kdir = work + "/kicks";
    mkdir(kdir.c_str(), 0777);
    write_file(kdir + "/kick13.rom", "ROM13");
    write_file(kdir + "/Kick34005.A500.RTB", "rtb");
    write_file(kdir + "/kick31.rom", "ROM31");
    CHECK(whd_prepare_launch(root.c_str(), "alpha", kdir.c_str(), nullptr, nullptr, err, sizeof err));
    std::string ss = slurp(root + "/S/Startup-Sequence");
    printf("  Startup-Sequence:\n%s", ss.c_str());
    CHECK(ss.find("CD \"WHDGame:alpha/Game\"\n") != std::string::npos);
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
    CHECK(ss == "C:SetPatch QUIET\nVersion\n; uae3DSng WHDLoad launcher (rewritten for every start)\nCD \"WHDGame:alpha/Game\"\nC:WHDLoad \"Other.slave\" NOVBRMOVE\n  EndCLI >NIL:\n");
    CHECK(slurp(root + "/S/Startup-Sequence.orig") == "C:SetPatch QUIET\nVersion\n  EndCLI >NIL:\n");
    CHECK(whd_prepare_launch(root.c_str(), "alpha", kdir.c_str(), nullptr, nullptr, err, sizeof err));   /* idempotent */
    CHECK(slurp(root + "/S/Startup-Sequence") == "C:SetPatch QUIET\nVersion\n; uae3DSng WHDLoad launcher (rewritten for every start)\nCD \"WHDGame:alpha/Game\"\nC:WHDLoad \"Other.slave\" PRELOAD\n  EndCLI >NIL:\n");

    /* the relocation tables that ship with the app (romfs on the 3DS): copied when missing, never replacing a file */
    static std::string bk;                                  /* whd_builtin_kickstarts points to it while this runs */
    bk = work + "/builtin_kicks";
    mkdir(bk.c_str(), 0777);
    write_file(bk + "/kick34005.A500.RTB", "builtin-rtb-13");
    write_file(bk + "/kick33180.A500.PAT", "builtin-pat-12");
    whd_builtin_kickstarts = bk.c_str();
    std::string root2 = work + "/fake2";
    mkdir(root2.c_str(), 0777);
    write_file(root2 + "/g.hdf", "img");
    write_file(root2 + "/g.whd", "slave=G.slave\n");
    CHECK(whd_prepare_launch(root2.c_str(), "g", nullptr, nullptr, nullptr, err, sizeof err));       /* no kickstart folder at all */
    CHECK(slurp(root2 + "/Devs/Kickstarts/kick34005.A500.RTB") == "builtin-rtb-13");
    CHECK(slurp(root2 + "/Devs/Kickstarts/kick33180.A500.PAT") == "builtin-pat-12");
    write_file(root2 + "/Devs/Kickstarts/kick34005.A500.RTB", "mine");
    CHECK(whd_prepare_launch(root2.c_str(), "g", "/nonexistent-kick-dir", nullptr, nullptr, err, sizeof err));
    CHECK(slurp(root2 + "/Devs/Kickstarts/kick34005.A500.RTB") == "mine");                           /* never replaced */
    std::string root3 = work + "/fake3";
    mkdir(root3.c_str(), 0777);
    write_file(root3 + "/g.hdf", "img");
    write_file(root3 + "/g.whd", "slave=G.slave\n");
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
    CHECK(whd_install_lha(root.c_str(), (work + "/Test Game.lha").c_str(), progress_cb, &pr, folder,
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
    int n = whd_list_games(root.c_str(), names, 16, nullptr);
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
    CHECK(whd_install_lha(root.c_str(), (work + "/Test Game.lha").c_str(), nullptr, nullptr, folder,
                          sizeof folder, &st, err, sizeof err));
    CHECK((rd = OfsRead(), chk.run(hdf, rd, cerr)) && rd.files.size() == 6);

    /* folders in the root (e.g. the first version's folder installs) are no games */
    mkdir((root + "/Test Game").c_str(), 0777);
    mkdir((root + "/Old Folder Game").c_str(), 0777);
    n = whd_list_games(root.c_str(), names, 16, nullptr);
    CHECK(n == 1 && !strcmp(names[0], "Test Game"));
    CHECK(!whd_prepare_launch(root.c_str(), "Old Folder Game", kdir.c_str(), nullptr, &li, err, sizeof err) && strstr(err, "not found"));
    CHECK(launch_ok(root.c_str(), "Test Game", kdir.c_str(), nullptr, &li) && li.hdf_path[0]);
    rmdir((root + "/Test Game").c_str());
    rmdir((root + "/Old Folder Game").c_str());

    /* 2. the choice of the slave: closest to the top, nothing deeper than 4 folders, then the shortest name */
    make_lha(work + "/Deep.lha", { { "g/a/b/c/d/Deep.slave", "x" }, { "g/a/Mid.slave", "x" }, { "g/Z.slave", "x" }, { "g/Zz.slave", "x" } });
    CHECK(whd_install_lha(root.c_str(), (work + "/Deep.lha").c_str(), nullptr, nullptr, folder, sizeof folder, &st, err, sizeof err));
    launch_ok(root.c_str(), "Deep", kdir.c_str(), "NOVBRMOVE", &li);
    CHECK(slurp(root + "/S/Startup-Sequence") == "; uae3DSng WHDLoad launcher (rewritten for every start)\nCD \"WHDGame:g\"\nC:WHDLoad \"Z.slave\" NOVBRMOVE\n");

    make_lha(work + "/Rootslave.lha", { { "Root.slave", "x" }, { "Dir/Sub.slave", "x" } });
    CHECK(whd_install_lha(root.c_str(), (work + "/Rootslave.lha").c_str(), nullptr, nullptr, folder, sizeof folder, &st, err, sizeof err));
    launch_ok(root.c_str(), "Rootslave", kdir.c_str(), nullptr, &li);
    CHECK(slurp(root + "/S/Startup-Sequence") == "; uae3DSng WHDLoad launcher (rewritten for every start)\nCD \"WHDGame:\"\nC:WHDLoad \"Root.slave\" PRELOAD\n");

    /* names with the characters that are special in an AmigaDOS command line */
    make_lha(work + "/Quotes.lha", { { "Q\"d*/We\"ird*.slave", "x" } });
    CHECK(whd_install_lha(root.c_str(), (work + "/Quotes.lha").c_str(), nullptr, nullptr, folder, sizeof folder, &st, err, sizeof err));
    launch_ok(root.c_str(), "Quotes", kdir.c_str(), nullptr, &li);
    CHECK(slurp(root + "/S/Startup-Sequence") == "; uae3DSng WHDLoad launcher (rewritten for every start)\nCD \"WHDGame:Q*\"d**\"\nC:WHDLoad \"We*\"ird**.slave\" PRELOAD\n");

    /* a slave whose name is 31 characters long: cut to 30, but it must stay a .slave file that WHDLoad is started with */
    make_lha(work + "/Fate.lha", { { "Fate/", "", true }, { "Fate/FateGatesOfDawnUncensored.slave", "x" }, { "Fate/FateGatesOfDawnUncensored.info", "i" } });
    CHECK(whd_install_lha(root.c_str(), (work + "/Fate.lha").c_str(), nullptr, nullptr, folder, sizeof folder, &st, err, sizeof err));
    CHECK(st.has_slave && st.renamed == 1);
    launch_ok(root.c_str(), "Fate", kdir.c_str(), nullptr, &li);
    CHECK(slurp(root + "/S/Startup-Sequence") == "; uae3DSng WHDLoad launcher (rewritten for every start)\nCD \"WHDGame:Fate\"\nC:WHDLoad \"FateGatesOfDawnUncensore.slave\" PRELOAD\n");
    CHECK(slurp(root + "/Fate.whd").find("slave=Fate/FateGatesOfDawnUncensore.slave\n") != std::string::npos);
    CHECK((rd = OfsRead(), chk.run(root + "/Fate.hdf", rd, cerr)) && rd.files.count("Fate/FateGatesOfDawnUncensore.slave") && rd.files.count("Fate/FateGatesOfDawnUncensored.info"));

    /* uninstall: an image game (with a left-over .tmp) */
    {
        CHECK(exists(root + "/Fate.hdf") && exists(root + "/Fate.whd"));
        write_file(root + "/Fate.hdf.tmp", "left over");
        CHECK(whd_uninstall(root.c_str(), "Fate", err, sizeof err));
        CHECK(!exists(root + "/Fate.hdf") && !exists(root + "/Fate.whd") && !exists(root + "/Fate.hdf.tmp"));
        n = whd_list_games(root.c_str(), names, 16, nullptr);
        bool fate_listed = false; for (int i = 0; i < n; i++) if (!strcmp(names[i], "Fate")) fate_listed = true;
        CHECK(!fate_listed);
        CHECK(!whd_uninstall(root.c_str(), "Fate", err, sizeof err) && strstr(err, "not found"));
        CHECK(exists(root + "/Test Game.hdf"));                                           /* other games are untouched */

        /* a game that cannot be started or deleted any more because the info file is gone: the image is still removed */
        make_lha(work + "/Half.lha", { { "Half/Half.slave", "x" } });
        CHECK(whd_install_lha(root.c_str(), (work + "/Half.lha").c_str(), nullptr, nullptr, folder, sizeof folder, &st, err, sizeof err));
        remove((root + "/Half.whd").c_str());
        CHECK(whd_uninstall(root.c_str(), "Half", err, sizeof err) && !exists(root + "/Half.hdf"));

        /* a folder of that name is never touched (no game of that name exists) */
        mkdir((root + "/Plain Folder").c_str(), 0777);
        write_file(root + "/Plain Folder/keep.txt", "keep");
        CHECK(!whd_uninstall(root.c_str(), "Plain Folder", err, sizeof err) && exists(root + "/Plain Folder/keep.txt"));

        for (const char *bad : { "", "..", ".", ".hidden", "a/b", "a\\b", "C", "devs", "x:y" })
            CHECK(!whd_uninstall(root.c_str(), bad, err, sizeof err) && strstr(err, "Invalid"));
        CHECK(!whd_uninstall(root.c_str(), nullptr, err, sizeof err));
        CHECK(exists(root + "/C/WHDLoad") && exists(root + "/S/Startup-Sequence"));        /* system folders are never touched */
    }

    /* 3. failures leave nothing behind */
    auto nothing_left = [&](const char *base) {
        return !exists(root + "/" + base + ".hdf") && !exists(root + "/" + base + ".hdf.tmp") && !exists(root + "/" + base + ".whd");
    };
    make_lha(work + "/Noslave.lha", { { "G/readme.txt", "hello" } });
    CHECK(!whd_install_lha(root.c_str(), (work + "/Noslave.lha").c_str(), nullptr, nullptr, folder, sizeof folder, &st, err, sizeof err) && strstr(err, "not a WHDLoad game"));
    CHECK(nothing_left("Noslave"));
    make_lha(work + "/Badcrc.lha", { { "G/G.slave", "hello" }, { "G/data", pseudo(70000, 3), false, 1 } });
    CHECK(!whd_install_lha(root.c_str(), (work + "/Badcrc.lha").c_str(), nullptr, nullptr, folder, sizeof folder, &st, err, sizeof err) && strstr(err, "CRC error"));
    printf("  bad crc -> '%s'\n", err);
    CHECK(nothing_left("Badcrc"));
    CHECK(!whd_install_lha(root.c_str(), (work + "/Test Game.lha").c_str(), cancel_cb, nullptr, folder, sizeof folder, &st, err, sizeof err) && !strcmp(err, "Cancelled"));
    CHECK(chk.run(hdf, rd, cerr));                                      /* the cancelled install must not have touched the old image */
    make_lha(work + "/Dots.lha", { { "../evil.slave", "x" }, { "a/../../b.txt", "y" } });   /* Lhasa collapses "..", an image cannot be left anyway */
    CHECK(whd_install_lha(root.c_str(), (work + "/Dots.lha").c_str(), nullptr, nullptr, folder, sizeof folder, &st, err, sizeof err));
    CHECK((rd = OfsRead(), chk.run(root + "/Dots.hdf", rd, cerr)) && rd.files.size() == 2 && rd.files.count("evil.slave") && rd.files.count("b.txt"));
    /* the same name twice: the first one is kept, the install does not fail */
    make_lha(work + "/Twice.lha", { { "T/T.slave", "first" }, { "T/data", "one" }, { "T/data", "two" }, { "T/T.slave", "third" } });
    CHECK(whd_install_lha(root.c_str(), (work + "/Twice.lha").c_str(), nullptr, nullptr, folder, sizeof folder, &st, err, sizeof err));
    CHECK(st.files == 2 && st.skipped == 2);
    CHECK((rd = OfsRead(), chk.run(root + "/Twice.hdf", rd, cerr)) && rd.files.size() == 2 && content("T/data") == "one" && content("T/T.slave") == "first");
    CHECK(!whd_install_lha(root.c_str(), (work + "/garbage.lha").c_str(), nullptr, nullptr, folder, sizeof folder, &st, err, sizeof err));
    CHECK(nothing_left("garbage"));
    CHECK(!whd_install_lha(root.c_str(), (work + "/missing.lha").c_str(), nullptr, nullptr, folder, sizeof folder, &st, err, sizeof err));
    printf("  missing -> '%s'\n", err);

    /* a damaged info file */
    write_file(root + "/Test Game.whd", "; nothing useful\n");
    CHECK(!whd_prepare_launch(root.c_str(), "Test Game", kdir.c_str(), nullptr, &li, err, sizeof err) && strstr(err, "damaged"));
    /* image without info file: not a game (not listed) */
    remove((root + "/Test Game.whd").c_str());
    n = whd_list_games(root.c_str(), names, 16, nullptr);
    bool listed = false; for (int i = 0; i < n; i++) if (!strcmp(names[i], "Test Game")) listed = true;
    CHECK(!listed);
    printf("  image tests done, %d failure(s) so far\n", g_fail);
}

/* Install an archive of the Lhasa test suite as an image: the image must be a valid OFS volume with the files of the
   archive (every file has been CRC-checked while decoding). Returns the files of the image in rd; false = failed. */
static bool install_suite_archive(const std::string &rel, const std::string &image_root, const std::string &archive,
                                  OfsRead &rd, whd_install_stats &ist, std::string &name_out, std::string &error)
{
    char name[WHD_NAME_MAX], err[256];
    Progress pr;
    int r = whd_install_lha(image_root.c_str(), archive.c_str(), progress_cb, &pr, name, sizeof name, &ist, err, sizeof err);
    if (!r) { error = err; return false; }
    name_out = name;
    if (!pr.monotonic) { printf("  PROGRESS-NOT-MONOTONIC %s\n", rel.c_str()); g_fail++; }
    OfsChecker chk; std::string cerr;
    rd = OfsRead();
    if (!chk.run(image_root + "/" + name + ".hdf", rd, cerr)) {
        printf("  IMAGE INVALID %-41s %s\n", rel.c_str(), cerr.c_str());
        g_fail++;
        return true;
    }
    if (rd.files.size() != ist.files) {
        printf("  IMAGE MISMATCH %-40s files %u in the image %zu\n", rel.c_str(), ist.files, rd.files.size());
        g_fail++;
    }
    return true;
}

static void options_favorites_tests(const std::string &work)
{
    printf("== game options and favourites\n");
    std::string root = work + "/optfav";
    mkdir(root.c_str(), 0777);
    for (const char *g : { "alpha", "Beta", "gamma", "Zebra" }) {
        write_file(root + "/" + g + ".hdf", "img");
        write_file(root + "/" + g + ".whd", (std::string("; uae3DSng WHDLoad game image\nslave=") + g + "/Game.slave\n").c_str());
    }
    char names[16][WHD_NAME_MAX], err[256];
    unsigned char fav[16];
    auto order = [&](int max = 16) {
        int n = whd_list_games(root.c_str(), names, max, fav);
        std::string o;
        for (int i = 0; i < n; i++) o += std::string(i ? "," : "") + (fav[i] ? "*" : "") + names[i];
        return o;
    };

    /* favourites: first in the list, each group sorted by name */
    CHECK(order() == "alpha,Beta,gamma,Zebra");
    CHECK(whd_set_favorite(root.c_str(), "Zebra", 1, err, sizeof err));
    CHECK(order() == "*Zebra,alpha,Beta,gamma");
    CHECK(whd_set_favorite(root.c_str(), "gamma", 1, err, sizeof err));
    CHECK(order() == "*gamma,*Zebra,alpha,Beta");
    CHECK(whd_set_favorite(root.c_str(), "gamma", 1, err, sizeof err));                 /* twice: still once */
    CHECK(slurp(root + "/favorites.txt") == "Zebra\ngamma\n");
    CHECK(whd_set_favorite(root.c_str(), "ZEBRA", 0, err, sizeof err));                 /* FAT: the case does not matter */
    CHECK(order() == "*gamma,alpha,Beta,Zebra");
    CHECK(whd_set_favorite(root.c_str(), "Zebra", 0, err, sizeof err));                 /* not a favourite: nothing to do */
    CHECK(whd_list_games(root.c_str(), names, 2, fav) == 2);                             /* a short list is a short list */
    CHECK(!exists(root + "/favorites.txt.tmp"));
    for (const char *bad : { "", ".hidden", "../x", "a/b", "a:b", "S", "C" }) {
        CHECK(!whd_set_favorite(root.c_str(), bad, 1, err, sizeof err) && strstr(err, "Invalid"));
    }
    CHECK(slurp(root + "/favorites.txt") == "gamma\n");
    write_file(root + "/favorites.txt", "ghost\r\nBeta\r\n\r\nalpha");                  /* hand-edited: CRLF, blank line, unknown game, no final newline */
    CHECK(order() == "*alpha,*Beta,gamma,Zebra");
    CHECK(whd_set_favorite(root.c_str(), "Zebra", 1, err, sizeof err));
    CHECK(slurp(root + "/favorites.txt") == "ghost\nBeta\nalpha\nZebra\n");
    CHECK(whd_list_games(root.c_str(), names, 16, nullptr) == 4);                        /* favorite may be NULL */
    remove((root + "/favorites.txt").c_str());
    CHECK(order() == "alpha,Beta,gamma,Zebra");

    /* uninstall removes the favourite */
    CHECK(whd_set_favorite(root.c_str(), "Beta", 1, err, sizeof err));
    CHECK(whd_uninstall(root.c_str(), "Beta", err, sizeof err));
    CHECK(slurp(root + "/favorites.txt") == "");
    CHECK(order() == "alpha,gamma,Zebra");

    /* options */
    char opt[WHD_OPTIONS_MAX];
    std::string orig = slurp(root + "/alpha.whd");
    CHECK(whd_get_options(root.c_str(), "alpha", opt, sizeof opt) && opt[0] == 0);
    CHECK(whd_set_options(root.c_str(), "alpha", "CUSTOM1=1 QUITKEY=$5D", err, sizeof err));
    CHECK(whd_get_options(root.c_str(), "alpha", opt, sizeof opt) && !strcmp(opt, "CUSTOM1=1 QUITKEY=$5D"));
    CHECK(slurp(root + "/alpha.whd") == orig + "options=CUSTOM1=1 QUITKEY=$5D\n");     /* slave line and comment untouched */
    CHECK(whd_set_options(root.c_str(), "alpha", "  A\tB\n C  ", err, sizeof err));
    CHECK(whd_get_options(root.c_str(), "alpha", opt, sizeof opt) && !strcmp(opt, "A?B? C"));
    CHECK(slurp(root + "/alpha.whd") == orig + "options=A?B? C\n");                    /* replaced, not added */
    CHECK(whd_set_options(root.c_str(), "alpha", std::string(300, 'x').c_str(), err, sizeof err));
    CHECK(whd_get_options(root.c_str(), "alpha", opt, sizeof opt) && strlen(opt) == WHD_OPTIONS_MAX - 1);
    CHECK(whd_set_options(root.c_str(), "alpha", "   ", err, sizeof err));            /* blank = none */
    CHECK(slurp(root + "/alpha.whd") == orig);
    CHECK(whd_set_options(root.c_str(), "alpha", "", err, sizeof err));
    CHECK(slurp(root + "/alpha.whd") == orig && !exists(root + "/alpha.whd.tmp"));
    write_file(root + "/gamma.whd", "slave=gamma/Game.slave");                         /* no final newline */
    CHECK(whd_set_options(root.c_str(), "gamma", "NOCACHE", err, sizeof err));
    CHECK(slurp(root + "/gamma.whd") == "slave=gamma/Game.slave\noptions=NOCACHE\n");
    CHECK(!whd_set_options(root.c_str(), "nogame", "X", err, sizeof err) && strstr(err, "not found"));
    CHECK(!whd_set_options(root.c_str(), "../alpha", "X", err, sizeof err) && strstr(err, "Invalid"));
    CHECK(!whd_get_options(root.c_str(), "nogame", opt, sizeof opt) && opt[0] == 0);
    CHECK(!whd_get_options(root.c_str(), "a/b", opt, sizeof opt));
    char small[8];                                                                         /* a short buffer is cut, not overrun */
    CHECK(whd_get_options(root.c_str(), "gamma", small, sizeof small) && !strcmp(small, "NOCACHE"));
    CHECK(whd_get_options(root.c_str(), "gamma", small, 4) && !strcmp(small, "NOC"));

    /* the options end up in the startup sequence, after PRELOAD; "slave=" inside them is no slave */
    mkdir((root + "/C").c_str(), 0777);
    write_file(root + "/C/WHDLoad", "binary");
    std::string kdir = work + "/kicks";
    const char *head = "; uae3DSng WHDLoad launcher (rewritten for every start)\nCD \"WHDGame:alpha\"\nC:WHDLoad \"Game.slave\" ";
    CHECK(launch_ok(root.c_str(), "alpha", kdir.c_str(), nullptr, nullptr));
    CHECK(slurp(root + "/S/Startup-Sequence") == std::string(head) + "PRELOAD\n");
    CHECK(whd_set_options(root.c_str(), "alpha", "slave=evil.slave CUSTOM1=1", err, sizeof err));
    CHECK(launch_ok(root.c_str(), "alpha", kdir.c_str(), nullptr, nullptr));
    CHECK(slurp(root + "/S/Startup-Sequence") == std::string(head) + "PRELOAD slave=evil.slave CUSTOM1=1\n");
    CHECK(launch_ok(root.c_str(), "alpha", kdir.c_str(), "NOVBRMOVE", nullptr));       /* an explicit command line replaces it all */
    CHECK(slurp(root + "/S/Startup-Sequence") == std::string(head) + "NOVBRMOVE\n");
    write_file(root + "/alpha.whd", "; hand edited\r\nslave=alpha/Game.slave\r\noptions=CUSTOM1=2\r\n");   /* CRLF */
    CHECK(whd_get_options(root.c_str(), "alpha", opt, sizeof opt) && !strcmp(opt, "CUSTOM1=2"));
    CHECK(launch_ok(root.c_str(), "alpha", kdir.c_str(), nullptr, nullptr));
    CHECK(slurp(root + "/S/Startup-Sequence") == std::string(head) + "PRELOAD CUSTOM1=2\n");

    /* a reinstall keeps the options, an uninstall + install does not */
    std::string iroot = work + "/optfav_img";
    mkdir(iroot.c_str(), 0777);
    std::vector<LhaEntry> game = { { "Mygame/", "", true }, { "Mygame/G.slave", pseudo(1000, 3) }, { "Mygame/d.dat", pseudo(5000, 4) } };
    make_lha(work + "/G.lha", game);
    char folder[WHD_NAME_MAX];
    whd_install_stats st;
    whd_image_requires_slave = 1;
    CHECK(whd_install_lha(iroot.c_str(), (work + "/G.lha").c_str(), nullptr, nullptr, folder, sizeof folder, &st, err, sizeof err));
    CHECK(slurp(iroot + "/G.whd") == "; uae3DSng WHDLoad game image\nslave=Mygame/G.slave\n");
    CHECK(whd_set_options(iroot.c_str(), "G", "CUSTOM1=1", err, sizeof err));
    CHECK(whd_install_lha(iroot.c_str(), (work + "/G.lha").c_str(), nullptr, nullptr, folder, sizeof folder, &st, err, sizeof err));
    CHECK(slurp(iroot + "/G.whd") == "; uae3DSng WHDLoad game image\nslave=Mygame/G.slave\noptions=CUSTOM1=1\n");
    CHECK(whd_uninstall(iroot.c_str(), "G", err, sizeof err));
    CHECK(whd_install_lha(iroot.c_str(), (work + "/G.lha").c_str(), nullptr, nullptr, folder, sizeof folder, &st, err, sizeof err));
    CHECK(slurp(iroot + "/G.whd") == "; uae3DSng WHDLoad game image\nslave=Mygame/G.slave\n");
    remove((iroot + "/G.hdf").c_str());
    remove((iroot + "/G.whd").c_str());
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s <archive dir> <work dir>\n", argv[0]); return 2; }
    std::string work = argv[2];
    mkdir(work.c_str(), 0777);
    unit_tests(work);
    image_tests(work);
    options_favorites_tests(work);

    std::vector<std::string> archives;
    walk(argv[1], archives);
    printf("== %zu archives\n", archives.size());
    int ok = 0, fail = 0, xfail = 0, idx = 0;
    std::map<unsigned long long, std::vector<std::string>> gpl;    /* hash of gpl-2 -> archives */
    std::vector<std::string> gz_mismatch;
    long gpl_size = 0;
    mkdir((work + "/gz").c_str(), 0777);
    whd_image_requires_slave = 0;                            /* the Lhasa test archives are no WHDLoad games */
    for (auto &a : archives) {
        std::string root = work + "/r" + std::to_string(idx++);
        mkdir(root.c_str(), 0777);
        OfsRead rd; whd_install_stats st; std::string name, error;
        bool r = install_suite_archive(a.substr(strlen(argv[1])), root, a, rd, st, name, error);
        std::string rel = a.substr(strlen(argv[1]));
        if (r) {
            ok++;
            printf("OK   %-52s files=%u bytes=%llu renamed=%u skipped=%u\n", rel.c_str(), st.files,
                   (unsigned long long)st.bytes, st.renamed, st.skipped);
            auto g = rd.files.find("gpl-2");
            if (!expected_failure(rel) && g != rd.files.end()) {     /* (Mac archives wrap the data in MacBinary: other bytes) */
                unsigned long long h = 1469598103934665603ULL;
                for (unsigned char c : g->second) h = (h ^ c) * 1099511628211ULL;
                gpl[h].push_back(rel);
                gpl_size = (long)g->second.size();
            }
            auto z = rd.files.find("gpl-2.gz");
            if (!expected_failure(rel) && z != rd.files.end()) {
                std::string gp = work + "/gz/" + std::to_string(idx) + ".gz";
                FILE *f = fopen(gp.c_str(), "wb");
                if (f) { fwrite(z->second.data(), 1, z->second.size(), f); fclose(f); }
                gz_mismatch.push_back(gp);                    /* compared below, after the reference hash is known */
            }
            remove((root + "/" + name + ".hdf").c_str());      /* the images are big: not needed any more */
            remove((root + "/" + name + ".whd").c_str());
        } else {
            fail++;
            const char *why = expected_failure(rel);
            if (!why && error.find("too big") != std::string::npos)
                why = "a file of more than 4 GB cannot go into an image";
            if (why) {
                xfail++;
                printf("XFAIL %-51s %s  [%s]\n", rel.c_str(), error.c_str(), why);
            } else {
                printf("FAIL %-52s %s\n", rel.c_str(), error.c_str());
                g_fail++;
            }
        }
    }
    whd_image_requires_slave = 1;
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
        whd_image_requires_slave = 0;
        int r = whd_install_lha(root.c_str(), archives[0].c_str(), cancel_cb, nullptr, folder, sizeof folder, nullptr, err, sizeof err);
        whd_image_requires_slave = 1;
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
