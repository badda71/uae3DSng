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

#include "gp2x.h"
#include "xwin.h"

bool switch_autofire=false;

void read_joystick(int nr, unsigned int *dir, int *button)
{
    int left = 0, right = 0, top = 0, bot = 0;

    *dir = 0;
    *button = 0;
	static int af_state = 0, af_delay = 0;

	if (nr + 1 == mainMenu_joyPort) return;

	extern int emulated_left, emulated_right, emulated_top, emulated_bot, emulated_button1, emulated_button2;

	left|=emulated_left;
	right|=emulated_right;
	top|=emulated_top;
	bot|=emulated_bot;
	*button |= emulated_button1;

	// autofire stuff
	if (switch_autofire) {
		switch (af_state) {
		case 0: // AF button was just pressed, press button for one frame
			af_state = 1;
			*button |= 1;
			break;
		case 1: // button is pressed, release after one frame
			af_state = 2;
			af_delay = 0;
			break;
		case 2: // button is not pressed, wait mainMenu_autofireRate before pressing it again
			if (af_delay > mainMenu_autofireRate)
			{
				af_state = 1;
				*button |= 1;
			} else {
				++af_delay;
			}
			break;
		}
	} else {
		af_state=0;
	}

	// map b-btn to joy up
	top|=emulated_button2;
    
	if (left) top = !top;
	if (right) bot = !bot;
		*dir = bot | (right << 1) | (top << 8) | (left << 9);
}
