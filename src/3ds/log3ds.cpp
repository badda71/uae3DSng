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
static int g_lazy = 0, g_dirty = 0;
static unsigned g_last_sync = 0;      /* ms */

static unsigned log_ms(void) { return (unsigned)((svcGetSystemTick() - g_t0) / (SYSCLOCK_ARM11 / 1000)); }

static void log_sync(void)
{
	if (g_log) fsync(fileno(g_log));
	g_dirty = 0;
	g_last_sync = log_ms();
}

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
	if (g_lazy) g_dirty = 1;
	else log_sync();
}

extern "C" void log3ds_lazy(int on)
{
	if (!on && g_lazy && g_dirty) log_sync();
	g_lazy = on;
	if (on) g_last_sync = g_log ? log_ms() : 0;
}

/* Called once per emulated frame (before the limiter sleeps for slack_us). The first call after the menu/start-up turns lazy
 * mode on. A pending sync is done when 2 s old and there is idle time to hide it in (its cost is made up for by the limiter's
 * catch-up allowance), at the latest after 10 s. */
extern "C" void log3ds_tick(unsigned slack_us)
{
	if (!g_lazy) { log3ds_lazy(1); return; }
	if (!g_dirty || !g_log) return;
	unsigned age = log_ms() - g_last_sync;
	if ((age >= 2000 && slack_us >= 6000) || age >= 10000) log_sync();
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
