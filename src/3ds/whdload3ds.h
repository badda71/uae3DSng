/*
 * uae3DSng - WHDLoad installer / launcher support (no emulator or 3DS dependencies, so it can be tested on a PC).
 *
 * Layout on the SD card (WHD_ROOT is mounted as the boot "HD Dir" when a game is launched):
 *
 *   whdload/C/WHDLoad            the WHDLoad program: yours if you put one there, otherwise the copy from the romfs is
 *                                put there when a game is started (see WHD_BUILTIN_WHDLOAD)
 *   whdload/S/Startup-Sequence   written for every launch (CD into the game, run WHDLoad)
 *   whdload/Devs/Kickstarts/     the .RTB / .PAT tables from the romfs, plus the ROM images (and your own .RTB / .PAT /
 *                                .key) from the kickstarts folder, copied when missing
 *   whdload/<Game>.hdf           one Amiga disk image (OFS, volume "WHDGame") per installed .lha, mounted as a hard file
 *   whdload/<Game>.whd           its info file: names the .slave inside the image
 *   whdload/<Game>/              (first version) a game extracted into a folder; still listed and started
 *
 * Installing = decoding the .lha (with the vendored Lhasa library, every file is CRC-checked) straight into the image
 * with large buffered writes (src/3ds/ofs_image.cpp). One big file, because hundreds of small files are very slow on the
 * 3DS SD card (about 0.3 s to create and 0.1 s to look at each one). The emulator is not running at that time.
 */
#ifndef WHDLOAD3DS_H
#define WHDLOAD3DS_H

#include <stddef.h>
#include <stdint.h>

#ifndef WHD_ROOT
#define WHD_ROOT "sdmc:/3ds/uae3DSng/whdload"
#endif
#ifndef WHD_KICKSTART_DIR
#define WHD_KICKSTART_DIR "sdmc:/3ds/uae3DSng/kickstarts"
#endif

/* The WHDLoad program that ships inside the app (romfs/whdload/WHDLoad). It is only a fallback: a C/WHDLoad that is
   already on the SD card is never replaced. */
#ifndef WHD_BUILTIN_WHDLOAD
#define WHD_BUILTIN_WHDLOAD "romfs:/whdload/WHDLoad"
#endif
/* The Kickstart relocation / patch tables (.RTB/.PAT) that ship with the app (romfs/whdload/Kickstarts). They are copied
   to <root>/Devs/Kickstarts when a game is started; a table that is already there is never replaced. */
#ifndef WHD_BUILTIN_KICKSTARTS
#define WHD_BUILTIN_KICKSTARTS "romfs:/whdload/Kickstarts"
#endif

#define WHD_NAME_MAX 128      /* length of one game folder name */

/* Called while extracting: item = name of the current file inside the archive. Return non-zero to cancel. */
typedef int (*whd_progress_fn)(void *user, const char *item, unsigned files_done, unsigned files_total,
                               uint64_t bytes_done, uint64_t bytes_total);

typedef struct {
    unsigned files;        /* regular files written */
    unsigned renamed;      /* entries whose name had to be changed (image: cut to 30 characters; folder: FAT) */
    uint64_t bytes;        /* bytes of file data */
    unsigned elapsed_ms;
    unsigned skipped;      /* image installs: entries not stored because an earlier entry has the same name */
    unsigned has_slave;    /* image installs: a .slave file was found (otherwise the archive is not a WHDLoad game) */
    uint64_t image_bytes;  /* image installs: size of the .hdf file */
} whd_install_stats;

typedef enum {
    WHD_INSTALL_IMAGE = 0, /* one Amiga disk image <root>/<name>.hdf (+ <name>.whd): fast, this is what the app uses */
    WHD_INSTALL_FOLDER = 1 /* files in <root>/<name>/: very slow on the 3DS with hundreds of files, kept for tests */
} whd_install_mode;

/* The volume name of a game image on the Amiga ("WHDGame:") */
#define WHD_VOLUME_NAME "WHDGame"

/* Filled by whd_prepare_launch */
typedef struct {
    char hdf_path[300];    /* image games: the file to mount as a hardfile (geometry 32/1/2/512); empty for folder games */
} whd_launch_info;

#ifdef __cplusplus
extern "C" {
#endif

/* Image installs fail when the archive has no .slave file (it is not a WHDLoad game). The host test clears this to
   exercise the image writer with any archive. */
extern int whd_image_requires_slave;

/* Where the fallback WHDLoad is read from (defaults to WHD_BUILTIN_WHDLOAD; the host test points it elsewhere) */
extern const char *whd_builtin_whdload;
extern const char *whd_builtin_kickstarts;

/* Install archive (.lha/.lzh): either into the image <root>/<name of the archive>.hdf or into the folder of that name
   (see whd_install_mode). Returns 1 on success (game_folder = the game's name, as listed by whd_list_games), 0 on
   failure (err = message for the user, nothing is left behind). An earlier install of the same name is replaced. */
int whd_install_lha(const char *root, const char *archive, whd_install_mode mode, whd_progress_fn progress, void *user,
                    char *game_folder, size_t game_folder_size, whd_install_stats *stats,
                    char *err, size_t err_size);

/* Names of the installed games (images <name>.hdf with their <name>.whd, and sub folders except the system folders),
   sorted, each name once. Returns the count. */
int whd_list_games(const char *root, char names[][WHD_NAME_MAX], int max_names);

/* Make the game start: makes sure C/WHDLoad exists (copies the built-in one if the SD card has none), copies Kickstart images (from kick_dir, may be NULL) into
   Devs/Kickstarts, finds the .slave and writes S/Startup-Sequence. extra_args may be NULL (default "PRELOAD").
   info may be NULL. Returns 1 on success, 0 on failure (err = message for the user). */
int whd_prepare_launch(const char *root, const char *game, const char *kick_dir, const char *extra_args,
                       whd_launch_info *info, char *err, size_t err_size);

/* Pure helpers, exposed for the tests */
void whd_folder_name_from_archive(const char *archive_path, char *folder, size_t folder_size);
/* Turn a path from an archive into a safe relative path below the install folder (components separated by '/').
   Returns 1 ok, 0 not usable (empty, or ".." inside). *renamed is increased when characters had to be replaced. */
int whd_safe_relative_path(const char *in, char *out, size_t out_size, unsigned *renamed);

#ifdef __cplusplus
}
#endif

#endif
