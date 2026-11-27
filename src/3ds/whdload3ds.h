/*
 * uae3DSng - WHDLoad installer / launcher support (no emulator or 3DS dependencies, so it can be tested on a PC).
 *
 * Layout on the SD card (WHD_ROOT is mounted as the boot "HD Dir" when a game is launched):
 *
 *   whdload/C/WHDLoad            the WHDLoad program: yours if you put one there, otherwise the copy from the romfs is
 *                                put there when a game is started (see WHD_BUILTIN_WHDLOAD)
 *   whdload/S/Startup-Sequence   written for every launch (CD into the game, run WHDLoad)
 *   whdload/Devs/Kickstarts/     ROM images / .RTB / .PAT files, copied from the kickstarts folder when missing
 *   whdload/<Game>/              one folder per installed .lha, e.g. whdload/Beast/Beast/Beast.slave
 *
 * Installing = extracting the .lha (with the vendored Lhasa library, every file is CRC-checked) straight into the
 * folder with plain, large, buffered writes. The emulator is not running at that time.
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

#define WHD_NAME_MAX 128      /* length of one game folder name */

/* Called while extracting: item = name of the current file inside the archive. Return non-zero to cancel. */
typedef int (*whd_progress_fn)(void *user, const char *item, unsigned files_done, unsigned files_total,
                               uint64_t bytes_done, uint64_t bytes_total);

typedef struct {
    unsigned files;        /* regular files written */
    unsigned renamed;      /* entries whose name had to be changed to be valid on the SD card (FAT) */
    uint64_t bytes;        /* bytes written */
    unsigned elapsed_ms;
} whd_install_stats;

#ifdef __cplusplus
extern "C" {
#endif

/* Where the fallback WHDLoad is read from (defaults to WHD_BUILTIN_WHDLOAD; the host test points it elsewhere) */
extern const char *whd_builtin_whdload;

/* Extract archive (.lha/.lzh) into <root>/<name of the archive>/ . Returns 1 on success (game_folder = folder name,
   below root), 0 on failure (err = message for the user). Files of an earlier install are overwritten. */
int whd_install_lha(const char *root, const char *archive, whd_progress_fn progress, void *user,
                    char *game_folder, size_t game_folder_size, whd_install_stats *stats,
                    char *err, size_t err_size);

/* Names of the installed games (= sub folders of root except the system folders), sorted. Returns the count. */
int whd_list_games(const char *root, char names[][WHD_NAME_MAX], int max_names);

/* Make the game start: makes sure C/WHDLoad exists (copies the built-in one if the SD card has none), copies Kickstart images (from kick_dir, may be NULL) into
   Devs/Kickstarts, finds the .slave and writes S/Startup-Sequence. extra_args may be NULL (default "PRELOAD").
   Returns 1 on success, 0 on failure (err = message for the user). */
int whd_prepare_launch(const char *root, const char *game, const char *kick_dir, const char *extra_args,
                       char *err, size_t err_size);

/* Pure helpers, exposed for the tests */
void whd_folder_name_from_archive(const char *archive_path, char *folder, size_t folder_size);
/* Turn a path from an archive into a safe relative path below the install folder (components separated by '/').
   Returns 1 ok, 0 not usable (empty, or ".." inside). *renamed is increased when characters had to be replaced. */
int whd_safe_relative_path(const char *in, char *out, size_t out_size, unsigned *renamed);

#ifdef __cplusplus
}
#endif

#endif
