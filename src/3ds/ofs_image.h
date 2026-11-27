/*
 * uae3DSng - writer for Amiga OFS ("DOS\0") hard disk images (no emulator or 3DS dependencies, tested on a PC).
 *
 * Why: creating hundreds of small files on the 3DS SD card is extremely slow (about 0.3 s per file, and every later
 * access to a folder with many files costs about 0.1 s per file), while one big file is written at full speed. A WHDLoad
 * game is therefore installed into ONE image file that the emulator mounts as a hardfile (the emulator tells the Amiga
 * that its hardfiles are DOS\0, so OFS is what must be written).
 *
 * Use (two phases, the whole layout is known before any data is written):
 *
 *   OfsImage *img = ofs_new("WHDGame");
 *   ofs_add_dir(img, "Game", ...);  id = ofs_add_file(img, "Game/Game.slave", size, ...);   ... for every entry
 *   ofs_layout(img, spare_bytes, ...);                  -> ofs_image_bytes(img) = size of the file that will be written
 *   ofs_begin_write(img, "x.hdf", ...);
 *   ofs_write_file(img, id, data, len, ...);            ... the contents of the files, in the order they were added
 *   ofs_finish(img, ...);  ofs_free(img);
 *
 * The image is written strictly front to back (no seeking): metadata is kept in memory and emitted when its position
 * is reached. Geometry for the emulator: 32 sectors, 1 surface, 2 reserved blocks, 512 bytes per block (the size is a
 * multiple of 32 blocks); the root block is in the middle, as AmigaDOS expects.
 */
#ifndef OFS_IMAGE_H
#define OFS_IMAGE_H

#include <stddef.h>
#include <stdint.h>

#define OFS_NAME_MAX      30        /* longest name of one file or folder */
#define OFS_MAX_ENTRIES   60000
#define OFS_ERR_EXISTS    (-2)      /* ofs_add_*: the name is already used (the caller may skip the entry); other errors: -1 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OfsImage OfsImage;

/* volume_name: shown on the Amiga as "<name>:" (at most 30 characters, no ':' or '/') */
OfsImage *ofs_new(const char *volume_name);
void ofs_free(OfsImage *img);

/* Describe the contents. path = components separated by '/', Amiga names as they are in the archive: ':' becomes '_',
   components longer than OFS_NAME_MAX are cut (counted in *renamed), "." and empty components are ignored, ".." is an
   error. Missing parent folders are created. unix_time 0 = now. Both return the entry id (>= 0) or a negative value (err = message).
   ofs_add_dir returns the existing id when the folder is already there; adding a file twice (or a name that is used by a
   file/folder of the other kind) returns OFS_ERR_EXISTS. */
int ofs_add_dir(OfsImage *img, const char *path, long unix_time, unsigned *renamed, char *err, size_t err_size);
int ofs_add_file(OfsImage *img, const char *path, uint32_t size, long unix_time, unsigned *renamed,
                 char *err, size_t err_size);

/* The path of an entry as it will be stored (after the name changes above), "a/b/c". Returns 1, or 0 when it does not fit. */
int ofs_entry_path(const OfsImage *img, int id, char *out, size_t out_size);

/* Compute the layout. spare_bytes = free space to leave on the volume (savegames etc.). Returns 1 / 0. */
int ofs_layout(OfsImage *img, uint64_t spare_bytes, char *err, size_t err_size);
uint64_t ofs_image_bytes(const OfsImage *img);   /* after ofs_layout */
uint32_t ofs_image_blocks(const OfsImage *img);

/* Create the image file and write the boot blocks. */
int ofs_begin_write(OfsImage *img, const char *file_path, char *err, size_t err_size);
/* Append data of file `id` (ids with size 0 need no call). Files must be written in the order they were added, each in
   order, exactly `size` bytes in total. */
int ofs_write_file(OfsImage *img, int id, const void *data, size_t len, char *err, size_t err_size);
/* Check that everything was written, write the rest of the image, close the file. On failure the file is closed too
   (the caller removes it). */
int ofs_finish(OfsImage *img, char *err, size_t err_size);
/* Close the file without finishing it (error path). */
void ofs_abort(OfsImage *img);

/* Exposed for the tests */
unsigned ofs_name_hash(const char *name, size_t len);   /* 0..71 */
uint32_t ofs_unix_to_days(long unix_time, uint32_t *mins, uint32_t *ticks);

#ifdef __cplusplus
}
#endif

#endif
