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

#ifdef __SWITCH__
#include "switch/switch_touch.h"
#endif

bool mouse_state = true;
extern bool slow_mouse;
extern bool fast_mouse;

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

/* Decode KeySyms. This function knows about all keys that are common
 * between different keyboard languages. */
static int kc_decode (SDL_keysym *prKeySym)
{
    switch (prKeySym->sym)
    {
    case SDLK_b: return AK_B;
    case SDLK_c: return AK_C;
    case SDLK_d: return AK_D;
    case SDLK_e: return AK_E;
    case SDLK_f: return AK_F;
    case SDLK_g: return AK_G;
    case SDLK_h: return AK_H;
    case SDLK_i: return AK_I;
    case SDLK_j: return AK_J;
    case SDLK_k: return AK_K;
    case SDLK_l: return AK_L;
    case SDLK_n: return AK_N;
    case SDLK_o: return AK_O;
    case SDLK_p: return AK_P;
    case SDLK_r: return AK_R;
    case SDLK_s: return AK_S;
    case SDLK_t: return AK_T;
    case SDLK_u: return AK_U;
    case SDLK_v: return AK_V;
    case SDLK_x: return AK_X;

    case SDLK_0: return AK_0;
    case SDLK_1: return AK_1;
    case SDLK_2: return AK_2;
    case SDLK_3: return AK_3;
    case SDLK_4: return AK_4;
    case SDLK_5: return AK_5;
    case SDLK_6: return AK_6;
    case SDLK_7: return AK_7;
    case SDLK_8: return AK_8;
    case SDLK_9: return AK_9;

    case SDLK_KP0: return AK_NP0;
    case SDLK_KP1: return AK_NP1;
    case SDLK_KP2: return AK_NP2;
    case SDLK_KP3: return AK_NP3;
    case SDLK_KP4: return AK_NP4;
    case SDLK_KP5: return AK_NP5;
    case SDLK_KP6: return AK_NP6;
    case SDLK_KP7: return AK_NP7;
    case SDLK_KP8: return AK_NP8;
    case SDLK_KP9: return AK_NP9;
    case SDLK_KP_DIVIDE: return AK_NPDIV;
    case SDLK_KP_MULTIPLY: return AK_NPMUL;
    case SDLK_KP_MINUS: return AK_NPSUB;
    case SDLK_KP_PLUS: return AK_NPADD;
    case SDLK_KP_PERIOD: return AK_NPDEL;
    case SDLK_KP_ENTER: return AK_ENT;

    case SDLK_F1: return AK_F1;
    case SDLK_F2: return AK_F2;
    case SDLK_F3: return AK_F3;
    case SDLK_F4: return AK_F4;
    case SDLK_F5: return AK_F5;
    case SDLK_F6: return AK_F6;
    case SDLK_F7: return AK_F7;
    case SDLK_F8: return AK_F8;
    case SDLK_F9: return AK_F9;
    case SDLK_F10: return AK_F10;

    case SDLK_BACKSPACE: return AK_BS;
    case SDLK_DELETE: return AK_DEL;
    case SDLK_LCTRL: return AK_CTRL;
    case SDLK_RCTRL: return AK_RCTRL;
    case SDLK_TAB: return AK_TAB;
    case SDLK_LALT: return AK_LALT;
    case SDLK_RALT: return AK_RALT;
    case SDLK_LMETA: return AK_LAMI;
    case SDLK_RMETA: return AK_RAMI;
    case SDLK_RETURN: return AK_RET;
    case SDLK_SPACE: return AK_SPC;
    case SDLK_LSHIFT: return AK_LSH;
    case SDLK_RSHIFT: return AK_RSH;
    case SDLK_ESCAPE: return AK_ESC;

    case SDLK_INSERT: return AK_HELP;
    case SDLK_HOME: return AK_NPLPAREN;
    case SDLK_END: return AK_NPRPAREN;
    case SDLK_CAPSLOCK: return AK_CAPSLOCK;

    case SDLK_UP: return AK_UP;
    case SDLK_DOWN: return AK_DN;
    case SDLK_LEFT: return AK_LF;
    case SDLK_RIGHT: return AK_RT;

    case SDLK_PAGEDOWN: return AK_LAMI;        /* PgDn mapped to left amiga */
    case SDLK_PAGEUP: return AK_RAMI;          /* PgUp mapped to right amiga */

    default: return -1;
    }
}

static int decode_us (SDL_keysym *prKeySym)
{
    switch(prKeySym->sym)
    {
	/* US specific */
    case SDLK_a: return AK_A;
    case SDLK_m: return AK_M;
    case SDLK_q: return AK_Q;
    case SDLK_y: return AK_Y;
    case SDLK_w: return AK_W;
    case SDLK_z: return AK_Z;
    case SDLK_LEFTBRACKET: return AK_LBRACKET;
    case SDLK_RIGHTBRACKET: return AK_RBRACKET;
    case SDLK_COMMA: return AK_COMMA;
    case SDLK_PERIOD: return AK_PERIOD;
    case SDLK_SLASH: return AK_SLASH;
    case SDLK_SEMICOLON: return AK_SEMICOLON;
    case SDLK_MINUS: return AK_MINUS;
    case SDLK_EQUALS: return AK_EQUAL;
	/* this doesn't work: */
    case SDLK_BACKQUOTE: return AK_QUOTE;
    case SDLK_QUOTE: return AK_BACKQUOTE;
    case SDLK_BACKSLASH: return AK_BACKSLASH;
    default: return -1;
    }
}

int keycode2amiga(SDL_keysym *prKeySym)
{
    int iAmigaKeycode = kc_decode(prKeySym);
    if (iAmigaKeycode == -1)
            return decode_us(prKeySym);
    return iAmigaKeycode;
}

static int refresh_necessary = 0;

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
				iAmigaKeyCode = keycode2amiga(&(rEvent.key.keysym));
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
				iAmigaKeyCode = keycode2amiga(&(rEvent.key.keysym));
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
			mouse_state = true;
			int mouseScale = mainMenu_mouseMultiplier * 16;
			mouseScale /= 100;

			if (fast_mouse) mouseScale *= 3;
			if (slow_mouse) mouseScale /= 8;

			lastmx += rEvent.motion.xrel * mouseScale;
			lastmy += rEvent.motion.yrel * mouseScale;
			// move mouse if touching mousepad edges
			if(rEvent.motion.x < 5)
				lastmx -= mouseScale * 4;
			if(rEvent.motion.y < 5)
				lastmy -= mouseScale * 4;
			if(rEvent.motion.x >= 395)
				lastmx += mouseScale * 4;
			if(rEvent.motion.y >= 235)
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
