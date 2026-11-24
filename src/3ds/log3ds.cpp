/*
 * uae3DSng - crash-safe debug log (see log3ds.h)
 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <malloc.h>
#include <3ds.h>
#include "log3ds.h"

#define LOGDIR   "sdmc:/3ds/uae3DSng"
#define LOGFILE  LOGDIR "/uae3DSng.log"
#define OUTFILE  LOGDIR "/stdout.log"

static FILE *g_log = NULL;
static int g_tried = 0;
static u64 g_t0 = 0;

static void log_open(void)
{
	g_tried = 1;
	mkdir("sdmc:/3ds", 0777);
	mkdir(LOGDIR, 0777);
	g_log = fopen(LOGFILE, "w");
	g_t0 = svcGetSystemTick();
	/* line-buffered stdout in a file next to the log: printf() from the core and the SDL fork */
	if (freopen(OUTFILE, "w", stdout))
		setvbuf(stdout, NULL, _IOLBF, 0);
}

extern "C" void log3ds(const char *fmt, ...)
{
	if (!g_tried) log_open();
	if (!g_log) return;

	char buf[512];
	unsigned ms = (unsigned)((svcGetSystemTick() - g_t0) / (SYSCLOCK_ARM11 / 1000));
	int n = snprintf(buf, sizeof(buf), "[%7u] ", ms);
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);
	va_end(ap);

	fputs(buf, g_log);
	size_t l = strlen(buf);
	if (l == 0 || buf[l - 1] != '\n') fputc('\n', g_log);
	fflush(g_log);
	fsync(fileno(g_log));
}

extern "C" unsigned log3ds_linear_free_kb(void) { return (unsigned)(linearSpaceFree() / 1024); }

extern "C" void log3ds_mem(const char *tag)
{
	struct mallinfo mi = mallinfo();
	log3ds("mem[%s]: app free %u KB, linear free %u KB, malloc in use %u KB",
		tag,
		(unsigned)(osGetMemRegionFree(MEMREGION_APPLICATION) / 1024),
		(unsigned)(linearSpaceFree() / 1024),
		(unsigned)(mi.uordblks / 1024));
}
