/*
  * UAE - The Un*x Amiga Emulator
  *
  * Definitions for accessing cycle counters on a given machine, if possible.
  *
  * Copyright 1998 Bernd Schmidt
  */

#ifndef _RPT_H_
#define _RPT_H_

#include <3ds.h>

typedef unsigned long frame_time_t;

extern int64_t g_uae_epoch;

static __inline__ frame_time_t read_processor_time (void)
{
  int64_t time;
  time = (int64_t) ((svcGetSystemTick() * 1000000) / 19200000);
  return time - g_uae_epoch;
}

#endif /* _RPT_H_ */

