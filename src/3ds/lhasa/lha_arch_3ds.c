/*
 * Platform glue for the vendored Lhasa library on the Nintendo 3DS (devkitARM / newlib / libctru).
 * Replaces lha_arch_unix.c: the 3DS has no ownership, permission, time-stamp or symlink calls, and whdload3ds.cpp
 * extracts files itself (lha_reader_read + own fwrite), so everything below that touches metadata is a no-op.
 */
#include "lha_arch.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

int lha_arch_vasprintf(char **result, const char *fmt, va_list args)
{
	va_list copy;
	int len;
	char *buf;

	va_copy(copy, args);
	len = vsnprintf(NULL, 0, fmt, copy);
	va_end(copy);
	if (len < 0) {
		return -1;
	}
	buf = (char *) malloc((size_t) len + 1);
	if (buf == NULL) {
		return -1;
	}
	vsnprintf(buf, (size_t) len + 1, fmt, args);
	*result = buf;
	return len;
}

void lha_arch_set_binary(FILE *handle)
{
	(void) handle;
}

int lha_arch_mkdir(char *path, unsigned int unix_perms)
{
	return mkdir(path, unix_perms) == 0;
}

int lha_arch_chown(char *filename, int unix_uid, int unix_gid)
{
	(void) filename; (void) unix_uid; (void) unix_gid;
	return 1;
}

int lha_arch_chmod(char *filename, int unix_perms)
{
	(void) filename; (void) unix_perms;
	return 1;
}

int lha_arch_utime(char *filename, unsigned int timestamp)
{
	(void) filename; (void) timestamp;
	return 1;
}

int lha_arch_set_windows_timestamps(char *filename, uint64_t creation_time, uint64_t modification_time,
                                    uint64_t access_time)
{
	(void) filename; (void) creation_time; (void) modification_time; (void) access_time;
	return 1;
}

FILE *lha_arch_fopen(char *filename, int unix_uid, int unix_gid, int unix_perms)
{
	(void) unix_uid; (void) unix_gid; (void) unix_perms;
	return fopen(filename, "wb");
}

LHAFileType lha_arch_exists(char *filename)
{
	struct stat statbuf;

	if (stat(filename, &statbuf) != 0) {
		return errno == ENOENT ? LHA_FILE_NONE : LHA_FILE_ERROR;
	}
	return S_ISDIR(statbuf.st_mode) ? LHA_FILE_DIRECTORY : LHA_FILE_FILE;
}

int lha_arch_symlink(char *path, char *target)
{
	(void) path; (void) target;
	return 0;
}
