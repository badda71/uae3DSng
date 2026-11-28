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
 *   whdload/<Game>.whd           its info file: names the .slave inside the image (slave=) and holds the options the game is
 *                                started with (options=, optional, edited in the menu)
 *   whdload/favorites.txt        the names of the favourite games, one per line; they are listed first
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

#define WHD_NAME_MAX 128      /* length of a game name (the archive's name without .lha) */
#define WHD_OPTIONS_MAX 200   /* size of the options string of a game, including the 0 */

/* Called while extracting: item = name of the current file inside the archive. Return non-zero to cancel. */
typedef int (*whd_progress_fn)(void *user, const char *item, unsigned files_done, unsigned files_total,
                               uint64_t bytes_done, uint64_t bytes_total);

typedef struct {
    unsigned files;        /* regular files written */
    unsigned renamed;      /* entries whose name had to be cut to 30 characters */
    uint64_t bytes;        /* bytes of file data */
    unsigned elapsed_ms;
    unsigned skipped;      /* entries not stored because an earlier entry has the same name */
    unsigned has_slave;    /* a .slave file was found (otherwise the archive is not a WHDLoad game) */
    uint64_t image_bytes;  /* size of the .hdf file */
} whd_install_stats;

/* The volume name of a game image on the Amiga ("WHDGame:") */
#define WHD_VOLUME_NAME "WHDGame"

/* Filled by whd_prepare_launch */
typedef struct {
    char hdf_path[300];    /* the game's image: the file to mount as a hardfile (geometry 32/1/2/512) */
} whd_launch_info;

#ifdef __cplusplus
extern "C" {
#endif

/* An install fails when the archive has no .slave file (it is not a WHDLoad game). The host test clears this to
   exercise the image writer with any archive. */
extern int whd_image_requires_slave;

/* Where the fallback WHDLoad is read from (defaults to WHD_BUILTIN_WHDLOAD; the host test points it elsewhere) */
extern const char *whd_builtin_whdload;
extern const char *whd_builtin_kickstarts;

/* Install archive (.lha/.lzh) into the image <root>/<name of the archive>.hdf with its info file <name>.whd. Returns 1 on
   success (game_name = the game's name, as listed by whd_list_games), 0 on failure (err = message for the user, nothing is
   left behind). An earlier install of the same name is replaced. */
int whd_install_lha(const char *root, const char *archive, whd_progress_fn progress, void *user, char *game_name,
                    size_t game_name_size, whd_install_stats *stats, char *err, size_t err_size);

/* Names of the installed games (images <name>.hdf together with <name>.whd), each name once: the favourites first, then
   the others, both sorted by name. Returns the count. favorite (may be NULL, else max_names entries) gets 1 for every
   listed favourite. */
int whd_list_games(const char *root, char names[][WHD_NAME_MAX], int max_names, unsigned char *favorite);

/* Make a game a favourite (favorite != 0) or not. Returns 1 on success, 0 on failure (err = message for the user). */
int whd_set_favorite(const char *root, const char *game, int favorite, char *err, size_t err_size);

/* The options a game is started with (added to the WHDLoad command line after PRELOAD, e.g. "CUSTOM1=1 QUITKEY=$5D").
   whd_get_options returns 1 when the game's info file could be read (options = "" when it has none), else 0.
   whd_set_options stores them (an empty string removes them): characters outside ' '..'~' are replaced, blanks at both
   ends removed, longer than WHD_OPTIONS_MAX - 1 characters is cut. Returns 1 on success, 0 on failure (err = message). A
   reinstall of the game keeps its options. */
int whd_get_options(const char *root, const char *game, char *options, size_t options_size);
int whd_set_options(const char *root, const char *game, const char *options, char *err, size_t err_size);

/* Uninstall a game as listed by whd_list_games: deletes <game>.whd, <game>.hdf and a left-over <game>.hdf.tmp (and removes it from
   the favourites). Returns 1
   when nothing of the game is left, 0 on failure (err = message for the user; a game whose image could not be deleted is
   no longer listed as soon as its .whd is gone). */
int whd_uninstall(const char *root, const char *game, char *err, size_t err_size);

/* Make the game start: makes sure C/WHDLoad exists (copies the built-in one if the SD card has none), copies Kickstart images (from kick_dir, may be NULL) into
   Devs/Kickstarts, reads the .slave's name from <game>.whd and writes S/Startup-Sequence. The command line is "PRELOAD" and the game's
   options (see whd_set_options); a non-NULL extra_args replaces all of it. info may be NULL. Returns 1 on success, 0 on failure (err = message for the user). */
int whd_prepare_launch(const char *root, const char *game, const char *kick_dir, const char *extra_args,
                       whd_launch_info *info, char *err, size_t err_size);

/* Pure helper, exposed for the tests: the game name for an archive path ("sdmc:/x/Game_v1.2.lha" -> "Game_v1.2") */
void whd_game_name_from_archive(const char *archive_path, char *name, size_t name_size);

#ifdef __cplusplus
}
#endif

#endif
