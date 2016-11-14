/*
  * UAE - The Un*x Amiga Emulator
  *
  * Definitions for accessing cycle counters on a given machine, if possible.
  *
  * Copyright 1998 Bernd Schmidt
  */

#ifndef _RPT_H_
#define _RPT_H_

#ifdef __PSP2__
#include <psp2/kernel/processmgr.h>
#endif

typedef unsigned long frame_time_t;

extern int64_t g_uae_epoch;

static __inline__ frame_time_t read_processor_time (void)
{
  int64_t time;
#ifdef __PSP2__
  time = sceKernelGetProcessTimeWide();
#else
  struct timespec ts;
  
  clock_gettime (CLOCK_MONOTONIC, &ts);

  time = (((int64_t) ts.tv_sec) * 1000000) + (ts.tv_nsec / 1000);
#endif
  return time - g_uae_epoch;
}

#endif /* _RPT_H_ */

