 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Interface to the Tcl/Tk GUI
  *
  * Copyright 1996 Bernd Schmidt
  */

#define _GUI_CPP

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "uae.h"
#include "options.h"
#include "gui.h"
#include "menu.h"
#include "menu_config.h"
#include "debug_uae4all.h"
#include "custom.h"
#include "memory-uae.h"
#include "xwin.h"
#include "drawing.h"
#include "sound.h"
#include "audio.h"
#include "keybuf.h"
#include "keyboard.h"
#include "disk.h"
#include "savestate.h"
#include "gensound.h"
#include <SDL/SDL.h>

#define VIDEO_FLAGS SDL_SWSURFACE | SDL_FULLSCREEN | SDL_DOUBLEBUF

#include "gp2x.h"
#include "gp2xutil.h"

#if defined(__PSP2__) // NOT __SWITCH__
#include <psp2/shellutil.h>
#endif

#ifdef _3DS
#include <3ds.h>
#endif

extern int gp2xMouseEmuOn, gp2xButtonRemappingOn;
extern bool switch_autofire;
int justMovedUp[MAX_NUM_CONTROLLERS]={};
int justMovedDown[MAX_NUM_CONTROLLERS]={};
int justMovedLeft[MAX_NUM_CONTROLLERS]={}; 
int justMovedRight[MAX_NUM_CONTROLLERS]={};
int justLComma=0, justLPeriod=0;
int justMovedStickUp[MAX_NUM_CONTROLLERS]={};
int justMovedStickDown[MAX_NUM_CONTROLLERS]={};
int justMovedStickLeft[MAX_NUM_CONTROLLERS]={};
int justMovedStickRight[MAX_NUM_CONTROLLERS]={};
int justPressedA[MAX_NUM_CONTROLLERS]={};
int justPressedB[MAX_NUM_CONTROLLERS]={};
int justPressedX[MAX_NUM_CONTROLLERS]={};
int justPressedY[MAX_NUM_CONTROLLERS]={};
int justPressedL[MAX_NUM_CONTROLLERS]={};
int justPressedR[MAX_NUM_CONTROLLERS]={};
#ifdef __SWITCH__
int justPressedL2[MAX_NUM_CONTROLLERS]={};
int justPressedR2[MAX_NUM_CONTROLLERS]={};
int justPressedL3[MAX_NUM_CONTROLLERS]={};
int justPressedR3[MAX_NUM_CONTROLLERS]={};
#endif
int justPressedQ=0;
int stylusClickOverride=0;
int stylusAdjustX=0, stylusAdjustY=0;
int screenWidth = 640;

static int customAutofireDelay[MAX_NUM_CONTROLLERS]={};

extern struct gui_info gui_data;

static char _show_message_str[40]={
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
};

int show_message=0;
char *show_message_str=(char *)&_show_message_str[0];

extern SDL_Surface *prSDLScreen;

extern SDL_Joystick *uae4all_joy0, *uae4all_joy1, *uae4all_joy2, *uae4all_joy3, *uae4all_joy4, *uae4all_joy5, *uae4all_joy6, *uae4all_joy7;

#if defined(__PSP2__) || defined(__SWITCH__)
//Predefined quick switch resolutions to select via TRIGGER R+START+DPAD LEFT/RIGHT
static int can_change_quickSwitchModeID = 1;
static int can_change_custom_controlSet = 1;
static int quickSwitchModeID=1;
struct myRes
{
		int num_lines;
		int top_pos;
};
static myRes quickSwitchModes[] = {
	{192, 18},
	{200, 18},
	{216, 18},
	{224, 18},
	{240, 18},
	{256, 18},
	{270, 18},
	{286, 0},
	{192, 30},
	{200, 30},
	{216, 30},
	{224, 30},
	{240, 30},
	{256, 30},
	{270, 30},
};
extern int moveY;

//analog stick values for mouse emulation on Vita
int lAnalogX=0;
int lAnalogY=0;
int rAnalogX=0;
int rAnalogY=0;
int lAnalogXCenter[MAX_NUM_CONTROLLERS]={};
int lAnalogYCenter[MAX_NUM_CONTROLLERS]={};
int rAnalogXCenter[MAX_NUM_CONTROLLERS]={};
int rAnalogYCenter[MAX_NUM_CONTROLLERS]={};
int haveJoysticksBeenCentered=0;
#endif // __PSP2__

extern int uae4all_keystate[];

int emulating=0;
char uae4all_image_file0[256]  = { 0, };
char uae4all_image_file1[256] = { 0, };
char uae4all_image_file2[256]  = { 0, };
char uae4all_image_file3[256] = { 0, };

char uae4all_hard_dir[256] = { 0, };
char uae4all_hard_file0[256] = { 0, };
char uae4all_hard_file1[256] = { 0, };
char uae4all_hard_file2[256] = { 0, };
char uae4all_hard_file3[256] = { 0, };

int drawfinished=0;

int moved_x = 0;
int moved_y = 0;

extern int mainMenu_case;
#ifdef WITH_TESTMODE
int no_limiter = 0;
#endif

int emulated_left=0;
int emulated_right=0;
int emulated_top=0;
int emulated_bot=0;
int emulated_button1=0;
int emulated_button2=0;

static void getChanges(void)
{
	if (mainMenu_sound)
	{
		if (mainMenu_sound == 1)
			changed_produce_sound=2;
		else
			changed_produce_sound=3;
		sound_default_evtime();
	}
	else
		changed_produce_sound=0;
	changed_gfx_framerate=mainMenu_frameskip;
}
	
int gui_init (void)
{
	SDL_ShowCursor(SDL_DISABLE);
	if (prSDLScreen!=NULL)
	{
		emulating=0;
		uae4all_init_sound();
		init_kickstart();

		//init_text(1);

		// start menu as soon as possible
		//run_mainMenu();
		SDL_Event e;
		e.type = SDL_KEYDOWN;
		e.key.keysym.sym = (SDLKey)DS_SELECT;
		e.key.keysym.mod = KMOD_MODE; // not mappable
		SDL_PushEvent(&e);
		e.type = SDL_KEYUP;
		SDL_PushEvent(&e);

		extern void reset_mainMenu();
		reset_mainMenu();
		init_sound();
		update_display();
		
		quit_text();
		inputmode_init();
		uae4all_pause_music();
		emulating=1;
		getChanges();
		check_all_prefs();
		reset_frameskip();
		return 0;
	}
	return -1;
}

int gui_update (void)
{
	extern char *savestate_filename;
	extern char *screenshot_filename;
	strcpy(changed_df[0],uae4all_image_file0);
	strcpy(changed_df[1],uae4all_image_file1);
	strcpy(changed_df[2],uae4all_image_file2);
	strcpy(changed_df[3],uae4all_image_file3);
	make_savestate_filenames(savestate_filename, screenshot_filename);
	real_changed_df[0]=1;
	real_changed_df[1]=1;
	real_changed_df[2]=1;
	real_changed_df[3]=1;
	return 0;
}

static void goMenu(void)
{
	int exitmode=0;
	int autosave=mainMenu_autosave;
	if (quit_program != 0)
		return;
	emulating=1;
	pause_sound();
	init_text(0);
	menu_raise();
	
	exitmode=run_mainMenu();

	/* Clear menu garbage at the bottom of the screen */
	black_screen_now();
	notice_screen_contents_lost();
	resume_sound();
	if ((!(strcmp(prefs_df[0],uae4all_image_file0))) || ((!(strcmp(prefs_df[1],uae4all_image_file1)))))
		menu_unraise();
	quit_text();
	getChanges();
	if (exitmode==1 || exitmode==2)
	{
		extern char *savestate_filename;
		extern char *screenshot_filename;
		extern int saveMenu_n_savestate;
		for(int i=0;i<mainMenu_drives;i++)
		{
			if (i==0 && strcmp(changed_df[0],uae4all_image_file0)) {
				strcpy(changed_df[0],uae4all_image_file0);
				real_changed_df[0]=1;
			}
			else if (i==1 && strcmp(changed_df[1],uae4all_image_file1)) {
				strcpy(changed_df[1],uae4all_image_file1);
				real_changed_df[1]=1;
			}
			else if (i==2 && strcmp(changed_df[2],uae4all_image_file2)) {
				strcpy(changed_df[2],uae4all_image_file2);
				real_changed_df[2]=1;
			}
			else if (i==3 && strcmp(changed_df[3],uae4all_image_file3)) {
				strcpy(changed_df[3],uae4all_image_file3);
				real_changed_df[3]=1;
			}
		}
		make_savestate_filenames(savestate_filename, screenshot_filename);
	}
	if (exitmode==3)
	{
		extern char *savestate_filename;
		extern char *screenshot_filename;
		for(int i=0;i<mainMenu_drives;i++)
		{
			changed_df[i][0]=0;
			if (i==0) {
				uae4all_image_file0[0]=0;
				if (strcmp(changed_df[0],uae4all_image_file0))
				{ 
				strcpy(changed_df[0],uae4all_image_file0);
				real_changed_df[0]=1;
				}
			}
			else if (i==1) {
				uae4all_image_file1[0]=0;
				if (strcmp(changed_df[1],uae4all_image_file1))
				{ 
				strcpy(changed_df[1],uae4all_image_file1);
				real_changed_df[1]=1;
				}
			}
			else if (i==2) {
				uae4all_image_file2[0]=0;
				if (strcmp(changed_df[2],uae4all_image_file2))
				{ 
				strcpy(changed_df[2],uae4all_image_file2);
				real_changed_df[2]=1;
				}
			}
			else if (i==3) {
				uae4all_image_file3[0]=0;
				if (strcmp(changed_df[3],uae4all_image_file3))
				{ 
				strcpy(changed_df[3],uae4all_image_file3);
				real_changed_df[3]=1;
				}
			}
			disk_eject(i);
		}
		make_savestate_filenames(savestate_filename, screenshot_filename);
	}
	if (exitmode==2)
	{
		if (autosave!=mainMenu_autosave)
		{
			prefs_df[0][0]=0;
		prefs_df[1][0]=0;
			prefs_df[2][0]=0;
		prefs_df[3][0]=0;
		}
		if(gp2xButtonRemappingOn)
			togglemouse();
			uae_reset ();
	}
	check_all_prefs();
	gui_purge_events();
	fpscounter_reset();
	notice_screen_contents_lost();
	
	//remove gfx garbage in pixel array
	lockscr();
	memset((char *) prSDLScreen->pixels, 0, prSDLScreen->h*prSDLScreen->pitch);
	unlockscr();
}

int nowSuperThrottle=0;

static void setSuperThrottle(int i)
{
	if (i && !nowSuperThrottle)
	{
		nowSuperThrottle=1;
		//m68k_speed=1; //6;
		changed_produce_sound=0;
		changed_gfx_framerate=20;
//		check_prefs_changed_cpu();
		check_prefs_changed_audio();	// sound
		check_prefs_changed_custom();	// framerate
		gui_set_message("SuperThrottle On",1000);
	} else if (!i && nowSuperThrottle) {
		nowSuperThrottle=0;
		getChanges();
//		check_prefs_changed_cpu();
		check_prefs_changed_audio();
		check_prefs_changed_custom();
		gui_set_message("SuperThrottle Off",1000);
	}
}

void gui_handle_events (SDL_Event *e)
{
	extern bool switch_autofire;
	int v,t = e->type;
	if ((v=(t == SDL_KEYDOWN)) || t == SDL_KEYUP) {
		switch (e->key.keysym.sym) {
		case DS_A:
			emulated_button1=v; break;
		case DS_B:
			emulated_button2=v; break;
		case DS_Y:
		case DS_ZL:
			buttonstate[0] = v; break;
		case DS_X:
		case DS_L:
			buttonstate[2] = v; break;
		case DS_UP2:
			if (mainMenu_mouseEmulation) break;
			// fallthrough
		case DS_UP1:
			emulated_top=v; break;
		case DS_DOWN2:
			if (mainMenu_mouseEmulation) break;
			// fallthrough
		case DS_DOWN1:
			emulated_bot=v; break;
		case DS_LEFT2:
			if (mainMenu_mouseEmulation) break;
			// fallthrough
		case DS_LEFT1:
			emulated_left=v; break;
		case DS_RIGHT2:
			if (mainMenu_mouseEmulation) break;
			// fallthrough
		case DS_RIGHT1:
			emulated_right=v; break;
/*		case DS_UP3:
		case DS_DOWN3:
			if (v) {
				mainMenu_adjustVposScale(e->key.keysym.sym == DS_UP3 ? 1 : -1, 0, 1);
				getChanges();
				check_all_prefs();
			    notice_screen_contents_lost();
			}
			break;
		case DS_LEFT3:
			if (v)
				mainMenu_adjustVposScale(0, -5, 1);
			break;
		case DS_RIGHT3:
			if (v)
				mainMenu_adjustVposScale(0, 5, 1);
			break;*/
		case DS_SELECT:
			if (v) goMenu();
			break;
		case DS_START:
			if (v) setSuperThrottle(!nowSuperThrottle);
			break;
		case DS_R:
			switch_autofire = v;
			break;
		default:
			break;
		}
	}
}

void gui_set_message(const char *msg, int t)
{
	return;

	show_message=t;
	strncpy(show_message_str, msg, 36);
}

void gui_show_window_bar(int per, int max, int case_title)
{
	return;

	char *title;
	if (case_title)
		title=(char*)"  Restore State";
	else
		title=(char*)"  Save State";
	_text_draw_window_bar(prSDLScreen,80,64,172,48,per,max,title);
	SDL_Flip(prSDLScreen);
}
