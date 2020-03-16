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

extern int mainMenu_autofire;

extern int nr_joysticks;

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
extern int mainMenu_leftStickMouse;
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

int dpadUp[MAX_NUM_CONTROLLERS]={};
int dpadDown[MAX_NUM_CONTROLLERS]={};
int dpadLeft[MAX_NUM_CONTROLLERS]={};
int dpadRight[MAX_NUM_CONTROLLERS]={};
int stickUp[MAX_NUM_CONTROLLERS]={};
int stickDown[MAX_NUM_CONTROLLERS]={};
int stickLeft[MAX_NUM_CONTROLLERS]={};
int stickRight[MAX_NUM_CONTROLLERS]={};
int buttonA[MAX_NUM_CONTROLLERS]={}; // Vita Square, GP2X_BUTTON_B
int buttonB[MAX_NUM_CONTROLLERS]={}; // Vita Circle, GP2X_BUTTON_A
int buttonX[MAX_NUM_CONTROLLERS]={}; // Vita Cross, GP2X_BUTTON_X
int buttonY[MAX_NUM_CONTROLLERS]={}; // Vita Triangle, GP2X_BUTTON_Y
int triggerL[MAX_NUM_CONTROLLERS]={};
int triggerR[MAX_NUM_CONTROLLERS]={};
#ifdef __SWITCH__
int triggerL2[MAX_NUM_CONTROLLERS]={};
int triggerR2[MAX_NUM_CONTROLLERS]={};
int triggerL3[MAX_NUM_CONTROLLERS]={};
int triggerR3[MAX_NUM_CONTROLLERS]={};
#endif
int buttonSelect[MAX_NUM_CONTROLLERS]={};
int buttonStart[MAX_NUM_CONTROLLERS]={};

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

#ifdef __SWITCH__
int singleJoycons = 0;  // are single Joycons being used at the moment?
void update_joycon_mode() {
	return;
/*
	int handheld = hidGetHandheldMode();
	int coalesceControllers = 0;
	int splitControllers = 0;
	if (!handheld) {
		if (mainMenu_singleJoycons) {
			if (!singleJoycons) {
				splitControllers = 1;
				singleJoycons = 1;
			}
		} else if (singleJoycons) {
			coalesceControllers = 1;
			singleJoycons = 0;
		}
	} else {
		if (singleJoycons) {
			coalesceControllers = 1;
			singleJoycons = 0;
		}
	}
	if (coalesceControllers) {
		// find all left/right single JoyCon pairs and join them together
		for (int id = 0; id < 8; id++) {
			hidSetNpadJoyAssignmentModeDual((HidControllerID) id);
		}
		int lastRightId = 8;		
		for (int id0 = 0; id0 < 8; id0++) {
			if (hidGetControllerType((HidControllerID) id0) & TYPE_JOYCON_LEFT) {
				for (int id1=lastRightId-1; id1>=0; id1--) {
					if (hidGetControllerType((HidControllerID) id1) & TYPE_JOYCON_RIGHT) {
						lastRightId=id1;
						// prevent missing player numbers
						if (id0 < id1) {
							hidMergeSingleJoyAsDualJoy((HidControllerID) id0, (HidControllerID) id1);
						} else if (id0 > id1) {
							hidMergeSingleJoyAsDualJoy((HidControllerID) id1, (HidControllerID) id0);
						}
						break;
					}
				}
			}	
		}
	}
	if (splitControllers) {
		for (int id=0; id<8; id++) {
			hidSetNpadJoyAssignmentModeSingleByDefault((HidControllerID) id);
		}
		hidSetNpadJoyHoldType(HidJoyHoldType_Horizontal);
		hidScanInput();
	}*/
}
#endif

#if defined(__PSP2__) || defined(__SWITCH__)
void remap_custom_controls() // assign custom 1-3 to currently used custom set
{
	for (int i=0; i<MAX_NUM_CONTROLLERS; i++)
	{
		int j=mainMenu_custom_controlSet;
		mainMenu_custom_up[i] = mainMenu_customPreset_up[j][i];
		mainMenu_custom_down[i] = mainMenu_customPreset_down[j][i];
		mainMenu_custom_left[i] = mainMenu_customPreset_left[j][i];
		mainMenu_custom_right[i] = mainMenu_customPreset_right[j][i];
		mainMenu_custom_stickup[i] = mainMenu_customPreset_stickup[j][i];
		mainMenu_custom_stickdown[i] = mainMenu_customPreset_stickdown[j][i];
		mainMenu_custom_stickleft[i] = mainMenu_customPreset_stickleft[j][i];
		mainMenu_custom_stickright[i] = mainMenu_customPreset_stickright[j][i];
		mainMenu_custom_A[i] = mainMenu_customPreset_A[j][i];
		mainMenu_custom_B[i] = mainMenu_customPreset_B[j][i];
		mainMenu_custom_X[i] = mainMenu_customPreset_X[j][i];
		mainMenu_custom_Y[i] = mainMenu_customPreset_Y[j][i];
		mainMenu_custom_L[i] = mainMenu_customPreset_L[j][i];
		mainMenu_custom_R[i] = mainMenu_customPreset_R[j][i];
#ifdef __SWITCH__
		mainMenu_custom_L2[i] = mainMenu_customPreset_L2[j][i];
		mainMenu_custom_R2[i] = mainMenu_customPreset_R2[j][i];
		mainMenu_custom_L3[i] = mainMenu_customPreset_L3[j][i];
		mainMenu_custom_R3[i] = mainMenu_customPreset_R3[j][i];
#endif
	}
}		
#endif

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
#if !(defined(AROS))
	SDL_JoystickEventState(SDL_ENABLE);
	SDL_JoystickOpen(0);
#endif
	if (prSDLScreen!=NULL)
	{
		emulating=0;
		uae4all_init_sound();
		init_kickstart();

#if defined(__PSP2__) // NOT __SWITCH__
		//Lock PS Button to prevent file corruption
		sceShellUtilLock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN);
#endif


		init_text(1);
#if !defined(__SWITCH__) && !defined(__PSP2__)
		if (!uae4all_image_file0[0])
#endif
			run_mainMenu();
		quit_text();
		inputmode_init();
		uae4all_pause_music();
		emulating=1;
		getChanges();
		check_all_prefs();
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

int customKey;
void getMapping(int customId)
{
	switch(customId)
	{
		case 1: customKey=AK_UP; break;
		case 2: customKey=AK_DN; break;
		case 3: customKey=AK_LF; break;
		case 4: customKey=AK_RT; break;
		case 5: customKey=AK_NP0; break;
		case 6: customKey=AK_NP1; break;
		case 7: customKey=AK_NP2; break;
		case 8: customKey=AK_NP3; break;
		case 9: customKey=AK_NP4; break;
		case 10: customKey=AK_NP5; break;
		case 11: customKey=AK_NP6; break;
		case 12: customKey=AK_NP7; break;
		case 13: customKey=AK_NP8; break;
		case 14: customKey=AK_NP9; break;
		case 15: customKey=AK_ENT; break;
		case 16: customKey=AK_NPDIV; break;
		case 17: customKey=AK_NPMUL; break;
		case 18: customKey=AK_NPSUB; break;
		case 19: customKey=AK_NPADD; break;
		case 20: customKey=AK_NPDEL; break;
		case 21: customKey=AK_NPLPAREN; break;
		case 22: customKey=AK_NPRPAREN; break;
		case 23: customKey=AK_SPC; break;
		case 24: customKey=AK_BS; break;
		case 25: customKey=AK_TAB; break;
		case 26: customKey=AK_RET; break;
		case 27: customKey=AK_ESC; break;
		case 28: customKey=AK_DEL; break;
		case 29: customKey=AK_LSH; break;
		case 30: customKey=AK_RSH; break;
		case 31: customKey=AK_CAPSLOCK; break;
		case 32: customKey=AK_CTRL; break;
		case 33: customKey=AK_LALT; break;
		case 34: customKey=AK_RALT; break;
		case 35: customKey=AK_LAMI; break;
		case 36: customKey=AK_RAMI; break;
		case 37: customKey=AK_HELP; break;
		case 38: customKey=AK_LBRACKET; break;
		case 39: customKey=AK_RBRACKET; break;
		case 40: customKey=AK_SEMICOLON; break;
		case 41: customKey=AK_COMMA; break;
		case 42: customKey=AK_PERIOD; break;
		case 43: customKey=AK_SLASH; break;
		case 44: customKey=AK_BACKSLASH; break;
		case 45: customKey=AK_QUOTE; break;
		case 46: customKey=AK_NUMBERSIGN; break;
		case 47: customKey=AK_LTGT; break;
		case 48: customKey=AK_BACKQUOTE; break;
		case 49: customKey=AK_MINUS; break;
		case 50: customKey=AK_EQUAL; break;
		case 51: customKey=AK_A; break;
		case 52: customKey=AK_B; break;
		case 53: customKey=AK_C; break;
		case 54: customKey=AK_D; break;
		case 55: customKey=AK_E; break;
		case 56: customKey=AK_F; break;
		case 57: customKey=AK_G; break;
		case 58: customKey=AK_H; break;
		case 59: customKey=AK_I; break;
		case 60: customKey=AK_J; break;
		case 61: customKey=AK_K; break;
		case 62: customKey=AK_L; break;
		case 63: customKey=AK_M; break;
		case 64: customKey=AK_N; break;
		case 65: customKey=AK_O; break;
		case 66: customKey=AK_P; break;
		case 67: customKey=AK_Q; break;
		case 68: customKey=AK_R; break;
		case 69: customKey=AK_S; break;
		case 70: customKey=AK_T; break;
		case 71: customKey=AK_U; break;
		case 72: customKey=AK_V; break;
		case 73: customKey=AK_W; break;
		case 74: customKey=AK_X; break;
		case 75: customKey=AK_Y; break;
		case 76: customKey=AK_Z; break;
		case 77: customKey=AK_1; break;
		case 78: customKey=AK_2; break;
		case 79: customKey=AK_3; break;
		case 80: customKey=AK_4; break;
		case 81: customKey=AK_5; break;
		case 82: customKey=AK_6; break;
		case 83: customKey=AK_7; break;
		case 84: customKey=AK_8; break;
		case 85: customKey=AK_9; break;
		case 86: customKey=AK_0; break;
		case 87: customKey=AK_F1; break;
		case 88: customKey=AK_F2; break;
		case 89: customKey=AK_F3; break;
		case 90: customKey=AK_F4; break;
		case 91: customKey=AK_F5; break;
		case 92: customKey=AK_F6; break;
		case 93: customKey=AK_F7; break;
		case 94: customKey=AK_F8; break;
		case 95: customKey=AK_F9; break;
		case 96: customKey=AK_F10; break;
		default: customKey=0;
	}
}

void gui_handle_events (SDL_Event *e)
{
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
//			if (mainMenu_cpad) break;
			// fallthrough
		case DS_UP1:
			emulated_top=v; break;
		case DS_DOWN2:
//			if (mainMenu_cpad) break;
			// fallthrough
		case DS_DOWN1:
			emulated_bot=v; break;
		case DS_LEFT2:
//			if (mainMenu_cpad) break;
			// fallthrough
		case DS_LEFT1:
			emulated_left=v; break;
		case DS_RIGHT2:
//			if (mainMenu_cpad) break;
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
/*		case DS_START:
			if (v) {
				if (nowSuperThrottle)
					leftSuperThrottle();
				else
					goSuperThrottle();
			}
			break;
		case DS_R:
			if (v) autofire_start();
			else autofire_stop();*/
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
