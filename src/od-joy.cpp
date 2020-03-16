 /*
	* UAE - The Un*x Amiga Emulator
	*
	* Joystick emulation for Linux and BSD. They share too much code to
	* split this file.
	*
	* Copyright 1997 Bernd Schmidt
	* Copyright 1998 Krister Walfridsson
	*/

#include "sysconfig.h"
#include "sysdeps.h"

#include "config.h"
#include "uae.h"
#include "options.h"
#include "memory-uae.h"
#include "custom.h"
#include "joystick.h"
#include "SDL.h"
#include "menu.h"
#include "menu_config.h"
#include "math.h"

#ifdef GP2X
#include "gp2x.h"
#include "xwin.h"
extern int gp2xMouseEmuOn;
extern int gp2xButtonRemappingOn;
#if defined(PANDORA)
extern int dpadUp[MAX_NUM_CONTROLLERS];
extern int dpadDown[MAX_NUM_CONTROLLERS];
extern int dpadLeft[MAX_NUM_CONTROLLERS];
extern int dpadRight[MAX_NUM_CONTROLLERS];
extern int stickUp[MAX_NUM_CONTROLLERS];
extern int stickDown[MAX_NUM_CONTROLLERS];
extern int stickLeft[MAX_NUM_CONTROLLERS];
extern int stickRight[MAX_NUM_CONTROLLERS];
extern int buttonA[MAX_NUM_CONTROLLERS];
extern int buttonB[MAX_NUM_CONTROLLERS];
extern int buttonX[MAX_NUM_CONTROLLERS];
extern int buttonY[MAX_NUM_CONTROLLERS];
extern int triggerL[MAX_NUM_CONTROLLERS];
extern int triggerR[MAX_NUM_CONTROLLERS];
#ifdef __SWITCH__
extern int triggerL2[MAX_NUM_CONTROLLERS];
extern int triggerR2[MAX_NUM_CONTROLLERS];
extern int triggerL3[MAX_NUM_CONTROLLERS];
extern int triggerR3[MAX_NUM_CONTROLLERS];
#endif
extern int buttonSelect[MAX_NUM_CONTROLLERS];
extern int buttonStart[MAX_NUM_CONTROLLERS];
extern int singleJoycons;
#endif
#if defined(__PSP2__) || defined(__SWITCH__)
extern int rAnalogX;
extern int rAnalogY;
extern int lAnalogX;
extern int lAnalogY;
extern int mainMenu_leftStickMouse;
extern int mainMenu_touchControls;
extern int mainMenu_deadZone;
int delay2[] = {0, 0, 0}; // for 2nd, 3rd and 4th player non-custom autofire
#endif
bool slow_mouse = false;
bool fast_mouse = false;

extern char launchDir[300];
bool switch_autofire=false;
int delay=0;
#endif

int nr_joysticks;
bool parport_joystick_enabled = true;
//needed for parallel port joystick handling done in this file
extern int joy2_dir, joy3_dir;
extern int joy2_button, joy3_button;

SDL_Joystick *uae4all_joy0, *uae4all_joy1, *uae4all_joy2, *uae4all_joy3, *uae4all_joy4, *uae4all_joy5, *uae4all_joy6, *uae4all_joy7;
extern SDL_Surface *prSDLScreen;

void read_joystick(int nr, unsigned int *dir, int *button)
{
    int left = 0, right = 0, top = 0, bot = 0;

    *dir = 0;
    *button = 0;

	if (nr + 1 == mainMenu_joyPort) return;

	extern int emulated_left, emulated_right, emulated_top, emulated_bot, emulated_button1, emulated_button2;

	left|=emulated_left;
	right|=emulated_right;
	top|=emulated_top;
	bot|=emulated_bot;
	*button |= emulated_button1;

	// map b-btn to joy up
	top|=emulated_button2;
    
	if (left) top = !top;
	if (right) bot = !bot;
		*dir = bot | (right << 1) | (top << 8) | (left << 9);
}

void init_joystick(void)
{
	nr_joysticks = 0;
}

void close_joystick(void)
{
}
