 /*
  * UAE - The Un*x Amiga Emulator
  *
  * SDL interface
  *
  * Copyright 2001 Bernd Lachner (EMail: dev@lachner-net.de)
  *
  * Partialy based on the UAE X interface (xwin.c)
  *
  * Copyright 1995, 1996 Bernd Schmidt
  * Copyright 1996 Ed Hanway, Andre Beck, Samuel Devulder, Bruno Coste
  * Copyright 1998 Marcus Sundberg
  * DGA support by Kai Kollmorgen
  * X11/DGA merge, hotkeys and grabmouse by Marcus Sundberg
  */

#include "sysconfig.h"
#include "sysdeps.h"

#include <unistd.h>
#include <signal.h>

#include <SDL/SDL.h>
#include <SDL/SDL_endian.h>

#include "config.h"
#include "uae.h"
#include "options.h"
#include "memory-uae.h"
#include "xwin.h"
#include "custom.h"
#include "drawing.h"
#include "m68k/m68k_intrf.h"
#include "keyboard.h"
#include "keybuf.h"
#include "gui.h"
#include "debug.h"
#include "gp2x.h"
#include "gp2xutil.h"
#include "inputmode.h"
#include "menu_config.h"
#include "menu.h"
#include "savestate.h"
#include "rpt.h"
#include "events.h"
#include "uibottom.h"
#include "uae3ds.h"

extern int moved_x;
extern int moved_y;
extern int stylusClickOverride;
extern int stylusAdjustX;
extern int stylusAdjustY;
int doStylusRightClick = 0;

extern int gp2xMouseEmuOn;
extern int gp2xButtonRemappingOn;

int show_inputmode = 0;
int show_volumecontrol;
extern int soundVolume;
extern int drawfinished;
int prefs_gfx_framerate, changed_gfx_framerate;

uae_u16 *prSDLScreenPixels;

char *gfx_mem=NULL;
unsigned gfx_rowbytes=0;

Uint32 uae4all_numframes=0;

#define VIDEO_FLAGS SDL_SWSURFACE | SDL_FULLSCREEN | SDL_DOUBLEBUF

/* Uncomment for debugging output */
/* #define DEBUG */

/* SDL variable for output surface */
SDL_Surface *prSDLScreen = NULL;

static int red_bits, green_bits, blue_bits;
static int red_shift, green_shift, blue_shift;
static SDL_Color arSDLColors[256];
static int ncolors = 0;

/* Keyboard and mouse */
int uae4all_keystate[256];

static unsigned long previous_synctime = 0;
static unsigned long next_synctime = 0;

void flush_block ()
{
	if (show_inputmode)
		inputmode_redraw();	
	if (drawfinished)
	{
		drawfinished=0;
		if (savestate_state == STATE_DOSAVE)
		{
			CreateScreenshot(SCREENSHOT);
		}
		unsigned long start = read_processor_time();
		if(start < next_synctime && next_synctime - start > time_per_frame - 1000)
			usleep((next_synctime - start) - 1000);
		SDL_Flip(prSDLScreen);
		last_synctime = read_processor_time();

		if(last_synctime - next_synctime > time_per_frame - 1000)
			adjust_idletime(0);
		else
			adjust_idletime(next_synctime - start);

		if(last_synctime - next_synctime > time_per_frame - 5000)
			next_synctime = last_synctime + time_per_frame * (1 + prefs_gfx_framerate);
		else
			next_synctime = next_synctime + time_per_frame * (1 + prefs_gfx_framerate);
	}
	init_row_map();
}

void black_screen_now(void)
{
	SDL_FillRect(prSDLScreen,NULL,0);
	SDL_Flip(prSDLScreen);
}

static __inline__ int bitsInMask (unsigned long mask)
{
	/* count bits in mask */
	int n = 0;
	while (mask)
	{
		n += mask & 1;
		mask >>= 1;
	}
	return n;
}

static __inline__ int maskShift (unsigned long mask)
{
	/* determine how far mask is shifted */
	int n = 0;
	while (!(mask & 1))
	{
		n++;
		mask >>= 1;
	}
	return n;
}

static int get_color (int r, int g, int b, xcolnr *cnp)
{
	*cnp = SDL_MapRGB(prSDLScreen->format, r, g, b);
	arSDLColors[ncolors].r = r;
	arSDLColors[ncolors].g = g;
	arSDLColors[ncolors].b = b;

	ncolors++;
	return 1;
}

static int init_colors (void)
{
	int i;

	/* Truecolor: */
	red_bits = bitsInMask(prSDLScreen->format->Rmask);
	green_bits = bitsInMask(prSDLScreen->format->Gmask);
	blue_bits = bitsInMask(prSDLScreen->format->Bmask);
	red_shift = maskShift(prSDLScreen->format->Rmask);
	green_shift = maskShift(prSDLScreen->format->Gmask);
	blue_shift = maskShift(prSDLScreen->format->Bmask);
	alloc_colors64k (red_bits, green_bits, blue_bits, red_shift, green_shift, blue_shift);
	for (i = 0; i < 4096; i++)
		xcolors[i] = xcolors[i] * 0x00010001;

	return 1;
}

int graphics_setup (void)
{
    /* Initialize the SDL library */
    if ( SDL_Init(SDL_INIT_VIDEO) < 0 )
    {
        fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
        return 0;
    }
    return 1;
}

static void graphics_subinit (void)
{
	if (prSDLScreen == NULL)
	{
		fprintf(stderr, "Unable to set video mode: %s\n", SDL_GetError());
		return;
	}
	else
	{
		prSDLScreenPixels=(uae_u16 *)prSDLScreen->pixels;
		SDL_Flip(prSDLScreen);
		SDL_ShowCursor(SDL_DISABLE);
		/* Initialize structure for Amiga video modes */
		gfx_mem = (char *)prSDLScreen->pixels;
		gfx_rowbytes = prSDLScreen->pitch;
	}
	lastmx = lastmy = 0;
	newmousecounters = 0;
}

int graphics_init (void)
{
	int i,j;

	// this will hold the state of the mouse emulation toggle.  The start button
	// will enable mouse emulation, which will allow the joystick to move the 
	// mouse point (probably badly, but there you go).
	gp2xButtonRemappingOn = 0;
	show_volumecontrol = 0;

	graphics_subinit ();

	check_all_prefs();	

    if (!init_colors ())
		return 0;

    buttonstate[0] = buttonstate[1] = buttonstate[2] = 0;
    for (i = 256; i--;)
		uae4all_keystate[i] = 0;
  
    return 1;
}

static void graphics_subshutdown (void)
{
    SDL_FreeSurface(prSDLScreen);
}

void graphics_leave (void)
{
    graphics_subshutdown ();
	SDL_VideoQuit();
    dumpcustom ();
}

void handle_events (void)
{
	SDL_Event rEvent;
	int iAmigaKeyCode;
	int i, j;
	int iIsHotKey = 0;

	// must be called once per frame to expire mouse button presses
	uib_handle_tap_processing(NULL);

    while (SDL_PollEvent(&rEvent))
	{
		if (uib_handle_event(&rEvent)) continue;
		uae3ds_mapping_apply(&rEvent);

		gui_handle_events (&rEvent);

		switch (rEvent.type)
		{
		case SDL_QUIT:
			uae_quit();
			break;
		case SDL_KEYDOWN:
			if ((rEvent.key.keysym.sym & 0x100) == 0)
			{
				iAmigaKeyCode = rEvent.key.keysym.sym;
				if (iAmigaKeyCode >= 0)
				{
					if (!uae4all_keystate[iAmigaKeyCode])
					{
						uae4all_keystate[iAmigaKeyCode] = 1;
						record_key(iAmigaKeyCode << 1);
					}
				}
			}
			break;
		case SDL_KEYUP:
			if ((rEvent.key.keysym.sym & 0x100) == 0)
			{
				iAmigaKeyCode = rEvent.key.keysym.sym;
				if (iAmigaKeyCode >= 0)
				{
					uae4all_keystate[iAmigaKeyCode] = 0;
					record_key((iAmigaKeyCode << 1) | 1);
				}
			}
			break;
		case SDL_MOUSEBUTTONDOWN:
			buttonstate[(rEvent.button.button-1)%3] = 1;
			break;
		case SDL_MOUSEBUTTONUP:
			buttonstate[(rEvent.button.button-1)%3] = 0;
			break;
		case SDL_MOUSEMOTION:
			int mouseScale = mainMenu_mouseMultiplier * 32;
			mouseScale /= 100;
	
			lastmx += rEvent.motion.xrel * mouseScale;
			lastmy += rEvent.motion.yrel * mouseScale;
			// move mouse if touching mousepad edges
			if(rEvent.motion.x < 5)
				lastmx -= mouseScale * 4;
			if(rEvent.motion.y < 5)
				lastmy -= mouseScale * 4;
			if(rEvent.motion.x >= visibleAreaWidth-5)
				lastmx += mouseScale * 4;
			if(rEvent.motion.y >= mainMenu_displayedLines-5)
				lastmy += mouseScale * 4;			
			newmousecounters = 1;
			break;
		}
	}
	uib_update();
}

int needmousehack (void)
{
	return 1;
}

int lockscr (void)
{
    return 1;
}

void unlockscr (void)
{
}

void gui_purge_events(void)
{
	SDL_Event event;
	SDL_Delay(150);
	while(SDL_PollEvent(&event))
		SDL_Delay(10);
	keybuf_init();
}
