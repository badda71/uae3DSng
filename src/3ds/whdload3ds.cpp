/*
 * uae3DSng - WHDLoad installer / launcher support. See whdload3ds.h.
 *
 * Everything here is plain stdio/POSIX + the vendored Lhasa library, so the same file is compiled for the 3DS and for
 * the host test (tools/whdload_host_test.cpp).
 */
#include "whdload3ds.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

extern "C" {
#include "public/lha_decoder.h"
#include "public/lha_input_stream.h"
#include "public/lha_reader.h"
#include "lha_file_header.h"
#include "crc16.h"
}

#ifdef __3DS__
#include "log3ds.h"
#define WHD_LOG(...) log3ds(__VA_ARGS__)
#else
#define WHD_LOG(...) do { fprintf(stderr, "[whd] " __VA_ARGS__); fputc('\n', stderr); } while (0)
#endif

#define WHD_MARKER      "; uae3DSng WHDLoad launcher"
#define IO_BUFFER_SIZE  (64 * 1024)
#define MAX_PATH_LEN    600

/* ---------------------------------------------------------------------------------------------------------------- */
/* small helpers                                                                                                    */
/* ---------------------------------------------------------------------------------------------------------------- */

const char *whd_builtin_whdload = WHD_BUILTIN_WHDLOAD;

static void set_err(char *err, size_t err_size, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static void set_err(char *err, size_t err_size, const char *fmt, ...)
{
    if (!err || err_size == 0)
        return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err, err_size, fmt, ap);
    va_end(ap);
}

static unsigned now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned)(tv.tv_sec * 1000u + tv.tv_usec / 1000u);
}

static int is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static long file_size(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
        return -1;
    return (long)st.st_size;
}

/* mkdir -p. Errors of the intermediate steps are ignored (e.g. "sdmc:" itself cannot be created), the result is
   whether the final path is a directory. */
static int mkdir_p(const char *path)
{
    char tmp[MAX_PATH_LEN];
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof(tmp))
        return 0;
    memcpy(tmp, path, len + 1);
    while (len > 1 && tmp[len - 1] == '/')
        tmp[--len] = '\0';
    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            if (!is_dir(tmp))
                mkdir(tmp, 0777);
            tmp[i] = '/';
        }
    }
    if (!is_dir(tmp))
        mkdir(tmp, 0777);
    return is_dir(tmp);
}

static int mkdir_parent(const char *file_path)
{
    char tmp[MAX_PATH_LEN];
    snprintf(tmp, sizeof(tmp), "%s", file_path);
    char *slash = strrchr(tmp, '/');
    if (!slash)
        return 1;
    *slash = '\0';
    return mkdir_p(tmp);
}

static int is_system_folder(const char *name)
{
    static const char *const names[] = { "S", "C", "Libs", "Devs", "L", "Fonts", "Locale", NULL };
    for (int i = 0; names[i]; i++)
        if (strcasecmp(name, names[i]) == 0)
            return 1;
    return 0;
}

static int has_ext(const char *name, const char *ext)   /* ext with the dot */
{
    size_t n = strlen(name), e = strlen(ext);
    return n >= e && strcasecmp(name + n - e, ext) == 0;
}

static int copy_file(const char *src, const char *dst)
{
    FILE *in = fopen(src, "rb");
    if (!in)
        return 0;
    if (!mkdir_parent(dst)) {
        fclose(in);
        return 0;
    }
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return 0;
    }
    char *buf = (char *)malloc(IO_BUFFER_SIZE);
    int ok = buf != NULL;
    while (ok) {
        size_t n = fread(buf, 1, IO_BUFFER_SIZE, in);
        if (n == 0)
            break;
        if (fwrite(buf, 1, n, out) != n)
            ok = 0;
    }
    free(buf);
    fclose(in);
    if (fclose(out) != 0)
        ok = 0;
    if (!ok)
        remove(dst);
    return ok;
}

static char *read_text_file(const char *path, size_t max)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    char *buf = (char *)malloc(max + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t n = fread(buf, 1, max, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

static int write_text_file(const char *path, const char *text)
{
    if (!mkdir_parent(path))
        return 0;
    FILE *f = fopen(path, "wb");
    if (!f)
        return 0;
    size_t len = strlen(text);
    int ok = fwrite(text, 1, len, f) == len;
    if (fclose(f) != 0)
        ok = 0;
    return ok;
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* names                                                                                                            */
/* ---------------------------------------------------------------------------------------------------------------- */

void whd_folder_name_from_archive(const char *archive_path, char *folder, size_t folder_size)
{
    if (folder_size == 0)
        return;
    const char *base = archive_path ? archive_path : "";
    for (const char *p = base; *p; p++)
        if (*p == '/' || *p == '\\' || *p == ':')
            base = p + 1;

    char tmp[96];
    snprintf(tmp, sizeof(tmp), "%s", base);
    size_t len = strlen(tmp);
    if (has_ext(tmp, ".lha") || has_ext(tmp, ".lzh"))
        tmp[len -= 4] = '\0';

    for (size_t i = 0; tmp[i]; i++) {
        unsigned char c = (unsigned char)tmp[i];
        if (!(isalnum(c) && c < 0x80) && c != ' ' && c != '_' && c != '-' && c != '.' && c != '(' && c != ')' &&
            c != '+' && c != ',')
            tmp[i] = '_';
    }
    /* no leading/trailing dots or spaces (not valid on FAT) */
    char *start = tmp;
    while (*start == ' ' || *start == '.')
        start++;
    len = strlen(start);
    while (len > 0 && (start[len - 1] == ' ' || start[len - 1] == '.'))
        start[--len] = '\0';

    if (*start == '\0')
        snprintf(folder, folder_size, "game");
    else if (is_system_folder(start))
        snprintf(folder, folder_size, "%s_game", start);
    else
        snprintf(folder, folder_size, "%s", start);
}

int whd_safe_relative_path(const char *in, char *out, size_t out_size, unsigned *renamed)
{
    if (!in || out_size < 2)
        return 0;
    size_t used = 0;
    out[0] = '\0';
    const char *p = in;
    while (*p) {
        while (*p == '/' || *p == '\\')
            p++;
        if (!*p)
            break;
        char comp[260];
        size_t n = 0;
        while (*p && *p != '/' && *p != '\\') {
            unsigned char c = (unsigned char)*p++;
            if (n >= sizeof(comp) - 1)
                continue;                                   /* absurdly long name: cut it */
            if (c < 0x20 || c >= 0x7f || strchr(":*?\"<>|", c)) {   /* not storable on FAT (also Latin-1 umlauts) */
                c = '_';
                if (renamed)
                    (*renamed)++;
            }
            comp[n++] = (char)c;
        }
        comp[n] = '\0';
        if (strcmp(comp, ".") == 0)
            continue;
        if (strcmp(comp, "..") == 0)
            return 0;                                       /* would leave the install folder */
        if (n > 0 && (comp[n - 1] == '.' || comp[n - 1] == ' ')) {      /* FAT drops a trailing dot/space */
            comp[n - 1] = '_';
            if (renamed)
                (*renamed)++;
        }
        if (used + n + 2 > out_size)
            return 0;
        if (used > 0)
            out[used++] = '/';
        memcpy(out + used, comp, n + 1);
        used += n;
    }
    return used > 0;
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* install = extract                                                                                                */
/* ---------------------------------------------------------------------------------------------------------------- */

static int is_dir_entry(const LHAFileHeader *h)
{
    return strcmp(h->compress_method, LHA_COMPRESS_TYPE_DIR) == 0;
}

int whd_install_lha(const char *root, const char *archive, whd_progress_fn progress, void *user,
                    char *game_folder, size_t game_folder_size, whd_install_stats *stats,
                    char *err, size_t err_size)
{
    whd_install_stats st;
    memset(&st, 0, sizeof(st));
    if (game_folder && game_folder_size)
        game_folder[0] = '\0';
    if (err && err_size)
        err[0] = '\0';
    unsigned t0 = now_ms();

    char folder[WHD_NAME_MAX];
    whd_folder_name_from_archive(archive, folder, sizeof(folder));
    char base[MAX_PATH_LEN];
    snprintf(base, sizeof(base), "%s/%s", root, folder);
    if (!mkdir_p(base)) {
        set_err(err, err_size, "Cannot create %s", base);
        return 0;
    }

    /* pass 1: how much is there? (headers only, the packed data is skipped) */
    unsigned files_total = 0, entries = 0;
    uint64_t bytes_total = 0;
    {
        LHAInputStream *stream = lha_input_stream_from((char *)archive);
        if (!stream) {
            set_err(err, err_size, "Cannot open the archive");
            return 0;
        }
        LHAReader *reader = lha_reader_new(stream);
        if (!reader) {
            lha_input_stream_free(stream);
            set_err(err, err_size, "Out of memory");
            return 0;
        }
        lha_reader_set_dir_policy(reader, LHA_READER_DIR_PLAIN);
        LHAFileHeader *h;
        while ((h = lha_reader_next_file(reader)) != NULL) {
            entries++;
            if (!is_dir_entry(h)) {
                files_total++;
                bytes_total += h->length;
            }
        }
        lha_reader_free(reader);
        lha_input_stream_free(stream);
    }
    if (entries == 0) {
        set_err(err, err_size, "Not an LHA archive (or empty)");
        return 0;
    }
    WHD_LOG("whdload: installing '%s' -> %s : %u files, %u bytes", archive, base, files_total, (unsigned)bytes_total);

    /* pass 2: extract */
    LHAInputStream *stream = lha_input_stream_from((char *)archive);
    LHAReader *reader = stream ? lha_reader_new(stream) : NULL;
    uint8_t *buf = (uint8_t *)malloc(IO_BUFFER_SIZE);
    if (!reader || !buf) {
        if (reader)
            lha_reader_free(reader);
        if (stream)
            lha_input_stream_free(stream);
        free(buf);
        set_err(err, err_size, "Out of memory");
        return 0;
    }
    lha_reader_set_dir_policy(reader, LHA_READER_DIR_PLAIN);

    int ok = 1;
    uint64_t bytes_done = 0;
    LHAFileHeader *h;
    while (ok && (h = lha_reader_next_file(reader)) != NULL) {
        if (is_dir_entry(h) && h->symlink_target)
            continue;                                       /* symbolic links do not exist on the Amiga */

        char *full = lha_file_header_full_path(h);
        char rel[MAX_PATH_LEN];
        unsigned renamed_before = st.renamed;
        if (!full || !whd_safe_relative_path(full, rel, sizeof(rel), &st.renamed)) {
            WHD_LOG("whdload: skipped an entry with an unusable path (%u bytes)", full ? (unsigned)strlen(full) : 0u);
            free(full);
            continue;
        }
        if (st.renamed != renamed_before) {
            WHD_LOG("whdload: entry stored as '%s' (the name in the archive has characters that FAT cannot hold)", rel);
            st.renamed = renamed_before + 1;               /* count entries, not characters */
        }
        free(full);

        char out_path[MAX_PATH_LEN];
        snprintf(out_path, sizeof(out_path), "%s/%s", base, rel);

        if (is_dir_entry(h)) {
            if (!mkdir_p(out_path)) {
                set_err(err, err_size, "Cannot create folder %s", rel);
                ok = 0;
            }
            continue;
        }

        if (!lha_decoder_for_name(h->compress_method)) {
            set_err(err, err_size, "%s: unsupported compression method %s", rel, h->compress_method);
            ok = 0;
            break;
        }
        if (!mkdir_parent(out_path)) {
            set_err(err, err_size, "Cannot create folder for %s", rel);
            ok = 0;
            break;
        }
        FILE *out = fopen(out_path, "wb");
        if (!out) {
            set_err(err, err_size, "Cannot create %s (%s)", rel, strerror(errno));
            ok = 0;
            break;
        }

        uint16_t crc = 0;
        uint64_t written = 0;
        int write_ok = 1, cancelled = 0;
        for (;;) {
            size_t n = lha_reader_read(reader, buf, IO_BUFFER_SIZE);
            if (n == 0)
                break;
            lha_crc16_buf(&crc, buf, n);
            if (fwrite(buf, 1, n, out) != n) {
                write_ok = 0;
                break;
            }
            written += n;
            bytes_done += n;
            if (progress && progress(user, rel, st.files, files_total, bytes_done, bytes_total)) {
                cancelled = 1;
                break;
            }
        }
        if (fclose(out) != 0)
            write_ok = 0;

        if (cancelled) {
            remove(out_path);
            set_err(err, err_size, "Cancelled");
            ok = 0;
        } else if (!write_ok) {
            remove(out_path);
            set_err(err, err_size, "Write error on %s (SD card full?)", rel);
            ok = 0;
        } else if (written != h->length || crc != h->crc) {
            remove(out_path);
            set_err(err, err_size, "%s is damaged (%s)", rel, written != h->length ? "wrong length" : "CRC error");
            ok = 0;
        } else {
            st.files++;
            st.bytes += written;
            if (progress && progress(user, rel, st.files, files_total, bytes_done, bytes_total)) {
                set_err(err, err_size, "Cancelled");
                ok = 0;
            }
        }
    }

    free(buf);
    lha_reader_free(reader);
    lha_input_stream_free(stream);

    st.elapsed_ms = now_ms() - t0;
    if (stats)
        *stats = st;
    if (!ok) {
        WHD_LOG("whdload: install failed after %u files: %s", st.files, err ? err : "");
        return 0;
    }
    WHD_LOG("whdload: installed %u files, %u bytes, %u renamed in %u ms", st.files, (unsigned)st.bytes, st.renamed,
            st.elapsed_ms);
    if (game_folder && game_folder_size)
        snprintf(game_folder, game_folder_size, "%s", folder);
    return 1;
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* list                                                                                                             */
/* ---------------------------------------------------------------------------------------------------------------- */

static int name_cmp(const void *a, const void *b)
{
    return strcasecmp((const char *)a, (const char *)b);
}

int whd_list_games(const char *root, char names[][WHD_NAME_MAX], int max_names)
{
    DIR *d = opendir(root);
    if (!d)
        return 0;
    int count = 0;
    struct dirent *e;
    while (count < max_names && (e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.' || is_system_folder(e->d_name) || strlen(e->d_name) >= WHD_NAME_MAX)
            continue;
        char path[MAX_PATH_LEN];
        snprintf(path, sizeof(path), "%s/%s", root, e->d_name);
        if (!is_dir(path))
            continue;
        snprintf(names[count++], WHD_NAME_MAX, "%s", e->d_name);
    }
    closedir(d);
    if (count > 1)
        qsort(names, (size_t)count, WHD_NAME_MAX, name_cmp);
    return count;
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* launch                                                                                                           */
/* ---------------------------------------------------------------------------------------------------------------- */

typedef struct {
    char rel[MAX_PATH_LEN];   /* relative to the game folder */
    int depth;
} Slave;

#define MAX_SLAVES 32

static void find_slaves(const char *dir, const char *rel, int depth, Slave *list, int *count)
{
    if (depth > 4 || *count >= MAX_SLAVES)
        return;
    DIR *d = opendir(dir);
    if (!d)
        return;
    struct dirent *e;
    while (*count < MAX_SLAVES && (e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.')
            continue;
        char path[MAX_PATH_LEN], sub[MAX_PATH_LEN];
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        if (rel[0])
            snprintf(sub, sizeof(sub), "%s/%s", rel, e->d_name);
        else
            snprintf(sub, sizeof(sub), "%s", e->d_name);
        if (is_dir(path)) {
            find_slaves(path, sub, depth + 1, list, count);
        } else if (has_ext(e->d_name, ".slave")) {
            snprintf(list[*count].rel, sizeof(list[*count].rel), "%s", sub);
            list[*count].depth = depth;
            (*count)++;
        }
    }
    closedir(d);
}

/* the one closest to the top, then the shortest name, then alphabetical (Game.slave wins over GameCD.slave) */
static int better_slave(const Slave *a, const Slave *b)
{
    if (a->depth != b->depth)
        return a->depth < b->depth;
    if (strlen(a->rel) != strlen(b->rel))
        return strlen(a->rel) < strlen(b->rel);
    return strcasecmp(a->rel, b->rel) < 0;
}

/* WHDLoad looks for its Kickstart images under these names; the candidates are what people call the files. */
typedef struct {
    const char *dest;
    const char *candidates[8];
} KickAlias;

static const KickAlias kick_aliases[] = {
    { "kick33180.A500",     { "kick12.rom", "kick1.2.rom", "kick33180.A500", "amiga-os-120.rom", "kick120.rom", NULL } },
    { "kick34005.A500",     { "kick13.rom", "kick1.3.rom", "kick34005.A500", "amiga-os-130.rom", "kick130.rom", NULL } },
    { "kick37175.A500",     { "kick20.rom", "kick2.04.rom", "kick37175.A500", "amiga-os-204.rom", NULL } },
    { "kick37350.A600",     { "kick37350.A600", "kick205.rom", "kick2.05.rom", "amiga-os-205-a600.rom", NULL } },
    { "kick39106.A1200",    { "kick39106.A1200", "kick30.rom", "kick3.0.rom", "amiga-os-300-a1200.rom", NULL } },
    { "kick40068.A1200",    { "kick31.rom", "kick3.1.rom", "kick40068.A1200", "amiga-os-310-a1200.rom", "kick310.rom",
                              NULL } },
    { "kick40063.A600",     { "kick40063.A600", "amiga-os-310-a600.rom", NULL } },
    { "kick40068.A4000",    { "kick40068.A4000", "amiga-os-310-a4000.rom", NULL } },
    { "kick40060.CD32",     { "kick40060.CD32", "amiga-os-310-cd32.rom", "cd32.rom", NULL } },
    { "kick40060.CD32.ext", { "kick40060.CD32.ext", "cd32.ext", NULL } },
    { "rom.key",            { "rom.key", NULL } },
    { NULL, { NULL } }
};

static void deploy_if_missing(const char *src, const char *dst)
{
    long s = file_size(src);
    if (s <= 0 || file_size(dst) == s)
        return;
    if (copy_file(src, dst))
        WHD_LOG("whdload: copied %s -> %s", src, dst);
    else
        WHD_LOG("whdload: could not copy %s -> %s", src, dst);
}

static void deploy_kickstarts(const char *root, const char *kick_dir)
{
    if (!kick_dir || !is_dir(kick_dir))
        return;
    char dst_dir[MAX_PATH_LEN];
    snprintf(dst_dir, sizeof(dst_dir), "%s/Devs/Kickstarts", root);
    mkdir_p(dst_dir);

    for (int i = 0; kick_aliases[i].dest; i++) {
        char dst[MAX_PATH_LEN];
        snprintf(dst, sizeof(dst), "%s/%s", dst_dir, kick_aliases[i].dest);
        for (int c = 0; kick_aliases[i].candidates[c]; c++) {
            char src[MAX_PATH_LEN];
            snprintf(src, sizeof(src), "%s/%s", kick_dir, kick_aliases[i].candidates[c]);
            if (file_size(src) > 0) {
                deploy_if_missing(src, dst);
                break;
            }
        }
    }
    /* relocation / patch tables and keys that the user keeps next to the ROMs */
    DIR *d = opendir(kick_dir);
    if (!d)
        return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (has_ext(e->d_name, ".rtb") || has_ext(e->d_name, ".pat") || has_ext(e->d_name, ".key")) {
            char src[MAX_PATH_LEN], dst[MAX_PATH_LEN];
            snprintf(src, sizeof(src), "%s/%s", kick_dir, e->d_name);
            snprintf(dst, sizeof(dst), "%s/%s", dst_dir, e->d_name);
            deploy_if_missing(src, dst);
        }
    }
    closedir(d);
}

/* line that starts with "EndCLI" (the end of a Workbench startup sequence) */
static char *find_endcli(char *text)
{
    for (char *line = text; line && *line;) {
        char *p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        if (strncasecmp(p, "endcli", 6) == 0)
            return line;
        line = strchr(line, '\n');
        if (line)
            line++;
    }
    return NULL;
}

int whd_prepare_launch(const char *root, const char *game, const char *kick_dir, const char *extra_args,
                       char *err, size_t err_size)
{
    if (err && err_size)
        err[0] = '\0';
    if (!game || !game[0] || strchr(game, '/') || strchr(game, '\\') || strcmp(game, "..") == 0) {
        set_err(err, err_size, "Invalid game name");
        return 0;
    }
    char game_dir[MAX_PATH_LEN];
    snprintf(game_dir, sizeof(game_dir), "%s/%s", root, game);
    if (!is_dir(game_dir)) {
        set_err(err, err_size, "Game folder not found: %s", game);
        return 0;
    }

    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/C/WHDLoad", root);
    if (file_size(path) <= 0) {
        /* none on the SD card: use the copy that ships with the app (a file the user put there is never touched) */
        if (!whd_builtin_whdload || file_size(whd_builtin_whdload) <= 0) {
            set_err(err, err_size, "WHDLoad is missing: copy it to %s", path);
            return 0;
        }
        if (!copy_file(whd_builtin_whdload, path) || file_size(path) != file_size(whd_builtin_whdload)) {
            set_err(err, err_size, "Cannot write %s (SD card full or locked?)", path);
            return 0;
        }
        WHD_LOG("whdload: no C/WHDLoad on the SD card, installed the built-in copy (%ld bytes)", file_size(path));
    }

    deploy_kickstarts(root, kick_dir);

    Slave *slaves = (Slave *)calloc(MAX_SLAVES, sizeof(Slave));
    if (!slaves) {
        set_err(err, err_size, "Out of memory");
        return 0;
    }
    int n_slaves = 0;
    find_slaves(game_dir, "", 0, slaves, &n_slaves);
    if (n_slaves == 0) {
        free(slaves);
        set_err(err, err_size, "No .slave file found in %s", game);
        return 0;
    }
    int best = 0;
    for (int i = 0; i < n_slaves; i++) {
        WHD_LOG("whdload: slave candidate %s", slaves[i].rel);
        if (better_slave(&slaves[i], &slaves[best]))
            best = i;
    }
    char slave_rel[MAX_PATH_LEN];
    snprintf(slave_rel, sizeof(slave_rel), "%s", slaves[best].rel);
    free(slaves);

    /* split into the directory (relative to SYS:) and the slave file name */
    char amiga_dir[MAX_PATH_LEN], slave_file[256];
    const char *slash = strrchr(slave_rel, '/');
    if (slash) {
        snprintf(amiga_dir, sizeof(amiga_dir), "SYS:%s/%.*s", game, (int)(slash - slave_rel), slave_rel);
        snprintf(slave_file, sizeof(slave_file), "%s", slash + 1);
    } else {
        snprintf(amiga_dir, sizeof(amiga_dir), "SYS:%s", game);
        snprintf(slave_file, sizeof(slave_file), "%s", slave_rel);
    }

    char args[256];
    snprintf(args, sizeof(args), "%s", (extra_args && extra_args[0]) ? extra_args : "PRELOAD");
    for (char *p = args; *p; p++)
        if (*p == '\n' || *p == '\r')
            *p = ' ';

    char launch[MAX_PATH_LEN * 2];
    snprintf(launch, sizeof(launch), WHD_MARKER " (rewritten for every start)\nCD \"%s\"\nC:WHDLoad \"%s\" %s\n",
             amiga_dir, slave_file, args);

    /* S/Startup-Sequence. A real Workbench startup sequence (it runs SetPatch) found there is kept as
       Startup-Sequence.orig and gets the launch lines before its EndCLI; otherwise a minimal one is written. */
    char startup[MAX_PATH_LEN], backup[MAX_PATH_LEN];
    snprintf(startup, sizeof(startup), "%s/S/Startup-Sequence", root);
    snprintf(backup, sizeof(backup), "%s/S/Startup-Sequence.orig", root);

    char *current = read_text_file(startup, 60000);
    if (current && !strstr(current, WHD_MARKER))
        write_text_file(backup, current);                   /* the user's own file (ours always has the marker): keep it */
    free(current);
    char *original = read_text_file(backup, 60000);

    size_t cap = (original ? strlen(original) : 0) + strlen(launch) + 256;
    char *text = (char *)calloc(1, cap);
    if (!text) {
        free(original);
        set_err(err, err_size, "Out of memory");
        return 0;
    }
    if (original && strstr(original, "SetPatch")) {
        char *endcli = find_endcli(original);
        size_t prefix = endcli ? (size_t)(endcli - original) : strlen(original);
        memcpy(text, original, prefix);
        strcat(text, launch);
        if (endcli)
            strcat(text, endcli);
    } else {
        snprintf(text, cap, "%s", launch);
    }
    free(original);

    int ok = write_text_file(startup, text);
    free(text);
    if (!ok) {
        set_err(err, err_size, "Cannot write %s", startup);
        return 0;
    }
    WHD_LOG("whdload: launch %s: CD \"%s\", slave %s, args '%s'", game, amiga_dir, slave_file, args);
    return 1;
}
