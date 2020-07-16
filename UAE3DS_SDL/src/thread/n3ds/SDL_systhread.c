/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
	  claim that you wrote the original software. If you use this software
	  in a product, an acknowledgment in the product documentation would be
	  appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
	  misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

#include "SDL_config.h"

#include "SDL_error.h"
#include "SDL_thread.h"
#include "../SDL_systhread.h"
#include "../SDL_thread_c.h"
#include <3ds.h>

#define CURRENT_KTHREAD 0xFFFF8000
#define STACKSIZE       (32 * 1024)
#define APPCORE_CPUID   0

void ThreadEntry(void *arg)
{
	SDL_RunThread(arg);
}

int SDL_SYS_CreateThread(SDL_Thread *thread, void *args)
{
//	s32 priority = 0x2F;
	s32 priority = 0x30;

	/* Set priority of new thread higher than the current thread */
	svcGetThreadPriority(&priority, CURRENT_KTHREAD);
	if(priority>0x19) priority--;
	else priority = 0x19; //priority 0x18 is for video thread that is activated by a signal and than must run at maximum priority to avoid flickering
	if(priority>0x2F) priority = 0x2F;

	thread->handle = threadCreate(ThreadEntry, args,
		STACKSIZE, priority, -2, false);

	thread->threadid = (int) thread->handle;
	if (!thread->threadid)
	{
	SDL_SetError("Create Thread failed");
	return(-1);
	}
 
	return 0;
}

void SDL_SYS_SetupThread(void)
{
	 //Nothing, probably
}

Uint32 SDL_ThreadID(void)
{
	// When called from main thread, return -1.
	// Thread ID 0 is reserved and should not be used for main thread
	// For some reason, threadGetHandle(threadGetCurrent) doesn't always work,
    // Use Thread pointer as thread ID as a workaround.
	Thread current = threadGetCurrent();
	if (current != NULL) {
		u32 threadID = (u32)current;	
		return threadID;
	}
	else {
		return -1; // Normally this means it is main thread
	}
}

void SDL_SYS_WaitThread(SDL_Thread *thread)
{
	threadJoin(thread->handle, 0x7FFFFFFFFFFFFFFF);
}

void SDL_SYS_KillThread(SDL_Thread *thread)
{
	threadFree(thread->handle);
}

