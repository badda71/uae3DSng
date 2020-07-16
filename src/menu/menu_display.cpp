#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "menu.h"
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include "uae.h"
#include "options.h"
#include "sound.h"
#include <SDL/SDL.h>
#include "gp2x.h"
#include <SDL/SDL_ttf.h>
#include "menu_config.h"
#include "keyboard.h"

const char *text_str_display_separator="--------------------------------------";
const char *text_str_display_title=    "Display and Sound Settings";
static const char *text_str_sound="Sound";
static const char *text_str_fast="Fast";
static const char *text_str_accurate="Accurate";
static const char *text_str_off="Off";
static const char *text_str_sndrate="Sound Rate";
static const char *text_str_48k="48k";
static const char *text_str_44k="44k";
static const char *text_str_32k="32k";
static const char *text_str_22k="22k";
static const char *text_str_11k="11k";
static const char *text_str_8k="8k";
static const char *text_str_status_line="Status Line";

int menuDisplay = 0;

extern int moveY;
extern int screenWidth;
extern int sound_rate;
extern int quit_pressed_in_submenu;
extern int emulating;

enum { 
	MENUDISPLAY_RETURNMAIN = 0,
	MENUDISPLAY_PRESETWIDTH,
	MENUDISPLAY_PRESETHEIGHT,
	MENUDISPLAY_DISPLINES,
#if !defined(__PSP2__) && !defined(__SWITCH__) //screenwidth has no meaning on Vita and is never used
	MENUDISPLAY_SCREENWIDTH,
#endif
	MENUDISPLAY_VERTPOS,
#if !defined(__PSP2__) && !defined(__SWITCH__)
	MENUDISPLAY_CUTLEFT,
	MENUDISPLAY_CUTRIGHT,
#endif
	MENUDISPLAY_REFRESHRATE,
	MENUDISPLAY_FRAMESKIP,
#if defined(__PSP2__) || defined(__SWITCH__)
	MENUDISPLAY_SHADER,
#endif
	MENUDISPLAY_STATUSLINE,
	MENUDISPLAY_BACKGROUND,
	MENUDISPLAY_FONT,
	MENUDISPLAY_SOUND,
	MENUDISPLAY_SNDRATE,
	MENUDISPLAY_STEREO,
	MENUDISPLAY_END
};

#if defined(__PSP2__)
enum {
	SHADER_NONE = 0,
	SHADER_LCD3X,
	SHADER_AAA,
	SHADER_SCALE2X,
	SHADER_SHARP_BILINEAR,
	SHADER_SHARP_BILINEAR_SIMPLE,
	SHADER_FXAA,
	NUM_SHADERS, //NUM_SHADERS - 1 is the max allowed number in mainMenu_shader
};
#endif

#if defined(__SWITCH__)
enum {
	SHADER_NONE = 0,
	SHADER_SHARP_BILINEAR_SIMPLE,
	SHADER_BILINEAR,
	SHADER_POINT,
	NUM_SHADERS, //NUM_SHADERS - 1 is the max allowed number in mainMenu_shader
};
#endif

static void draw_displayMenu(int c)
{

	int menuWidth = 38;
	int menuHeight = 23;
	
	int leftMargin = ((50 - menuWidth) / 2)*8;
	int menuLine = ((31 - menuHeight) / 2)*8;
	
	int tabstop1 = leftMargin + 14*8;
	int tabstop2 = leftMargin + 16*8;
	int tabstop3 = leftMargin + 18*8;
	int tabstop4 = leftMargin + 20*8;
	int tabstop5 = leftMargin + 22*8;
	int tabstop6 = leftMargin + 24*8;
	int tabstop7 = leftMargin + 26*8;
	int tabstop8 = leftMargin + 28*8;
	int tabstop9 = leftMargin + 30*8;
	int tabstop10 = leftMargin + 32*8;
	int tabstop11 = leftMargin + 34*8;

	static int b=0;
	int bb=(b%6)/3;
	char value[25]="";

	text_draw_background();
	text_draw_window(leftMargin/8,menuLine/8,menuWidth,menuHeight,text_str_display_title);

	// MENUDISPLAY_RETURNMAIN
	if (menuDisplay == MENUDISPLAY_RETURNMAIN && bb)
		write_text_inv_pos(leftMargin, menuLine, "Return to main menu");
	else
		write_text_pos(leftMargin, menuLine, "Return to main menu");

	menuLine+=8;
	write_text_pos(leftMargin,menuLine,text_str_display_separator);
	menuLine+=8;

	// MENUDISPLAY_PRESETWIDTH
	write_text_pos(leftMargin,menuLine,"Preset Width");
	snprintf(value, 20, "%d", visibleAreaWidth);
	if ((menuDisplay!=MENUDISPLAY_PRESETWIDTH)||(bb))
		write_text_pos(tabstop3,menuLine,value);
	else
		write_text_inv_pos(tabstop3,menuLine,value);

	// MENUDISPLAY_PRESETHEIGHT
	menuLine+=12;
	write_text_pos(leftMargin,menuLine,"Preset Height");
	if ((menuDisplay!=MENUDISPLAY_PRESETHEIGHT)||(bb))
		write_text_pos(tabstop3,menuLine,presetMode);
	else
		write_text_inv_pos(tabstop3,menuLine,presetMode);

	menuLine+=8;
	write_text_pos(leftMargin,menuLine,text_str_display_separator);
	menuLine+=8;
	write_text_pos(leftMargin,menuLine,"Custom Settings");
	menuLine+=8;
	write_text_pos(leftMargin,menuLine,"---------------");
	menuLine+=8;

	// MENUDISPLAY_DISPLINES
	write_text_pos(leftMargin,menuLine,"Displayed Lines");
	sprintf(value, "%d", mainMenu_displayedLines);
	if ((menuDisplay!=MENUDISPLAY_DISPLINES)||(bb))
		write_text_pos(tabstop3,menuLine,value);
	else
		write_text_inv_pos(tabstop3,menuLine,value);
#if !defined(__PSP2__) && !defined(__SWITCH__)
	// MENUDISPLAY_SCREENWIDTH
	menuLine+=12;
	write_text_pos(leftMargin,menuLine,"Screen Width");
	sprintf(value, "%d", screenWidth);
	if ((menuDisplay!=MENUDISPLAY_SCREENWIDTH)||(bb))
		write_text_pos(tabstop3,menuLine,value);
	else
		write_text_inv_pos(tabstop3,menuLine,value);
#endif
	// MENUDISPLAY_VERTPOS
	menuLine+=12;
	write_text_pos(leftMargin,menuLine,"Vertical Position");
	sprintf(value, "%d", moveY);
	if ((menuDisplay!=MENUDISPLAY_VERTPOS)||(bb))
		write_text_pos(tabstop3,menuLine,value);
	else
		write_text_inv_pos(tabstop3,menuLine,value);
#if !defined(__PSP2__) && !defined(__SWITCH__)
	// MENUDISPLAY_CUTLEFT
	menuLine+=12;
	write_text_pos(leftMargin,menuLine,"Cut Left");
	sprintf(value, "%d", mainMenu_cutLeft);
	if ((menuDisplay!=MENUDISPLAY_CUTLEFT)||(bb))
		write_text_pos(tabstop3,menuLine,value);
	else
		write_text_inv_pos(tabstop3,menuLine,value);

	// MENUDISPLAY_CUTRIGHT
	menuLine+=12;
	write_text_pos(leftMargin,menuLine,"Cut Right");
	sprintf(value, "%d", mainMenu_cutRight);
	if ((menuDisplay!=MENUDISPLAY_CUTRIGHT)||(bb))
		write_text_pos(tabstop3,menuLine,value);
	else
		write_text_inv_pos(tabstop3,menuLine,value);

#else // !defined(__PSP2__) && !defined(__SWITCH__)
	menuLine+=8;
	write_text_pos(leftMargin,menuLine,text_str_display_separator);
	menuLine+=8;
#endif
	// MENUDISPLAY_REFRESHRATE
	write_text_pos(leftMargin,menuLine,"Refresh Rate");
	if ((!mainMenu_ntsc)&&((menuDisplay!=MENUDISPLAY_REFRESHRATE)||(bb)))
		write_text_inv_pos(tabstop1,menuLine,"50Hz");
	else
		write_text_pos(tabstop1,menuLine,"50Hz");

	if ((mainMenu_ntsc)&&((menuDisplay!=MENUDISPLAY_REFRESHRATE)||(bb)))
		write_text_inv_pos(tabstop3+1*8,menuLine,"60Hz");
	else
		write_text_pos(tabstop3+1*8,menuLine,"60Hz");

	// MENUDISPLAY_FRAMESKIP
#ifdef PANDORA
	write_text_pos(tabstop3+7*8,menuLine,"Frameskip");
	if ((mainMenu_frameskip==0)&&((menuDisplay!=MENUDISPLAY_FRAMESKIP)||(bb)))
		write_text_inv_pos(tabstop3+17*8,menuLine,"0");
	else
		write_text_pos(tabstop3+17*8,menuLine,"0");
	if ((mainMenu_frameskip==1)&&((menuDisplay!=MENUDISPLAY_FRAMESKIP)||(bb)))
		write_text_inv_pos(tabstop3+19*8,menuLine,"1");
	else
		write_text_pos(tabstop3+19*8,menuLine,"1");
#else
	menuLine+=12;
	write_text_pos(tabstop3+7*8,menuLine,"Frameskip");
	if ((mainMenu_frameskip==1)&&((menuDisplay!=MENUDISPLAY_FRAMESKIP)||(bb)))
		write_text_inv_pos(tabstop2,menuLine,"1");
	else
		write_text_pos(tabstop2,menuLine,"1");
	if ((mainMenu_frameskip==2)&&((menuDisplay!=MENUDISPLAY_FRAMESKIP)||(bb)))
		write_text_inv_pos(tabstop3,menuLine,"2");
	else
		write_text_pos(tabstop3,menuLine,"2");
	if ((mainMenu_frameskip==3)&&((menuDisplay!=MENUDISPLAY_FRAMESKIP)||(bb)))
		write_text_inv_pos(tabstop4,menuLine,"3");
	else
		write_text_pos(tabstop4,menuLine,"3");
	if ((mainMenu_frameskip==4)&&((menuDisplay!=MENUDISPLAY_FRAMESKIP)||(bb)))
		write_text_inv_pos(tabstop5,menuLine,"4");
	else
		write_text_pos(tabstop5,menuLine,"4");
	if ((mainMenu_frameskip==5)&&((menuDisplay!=MENUDISPLAY_FRAMESKIP)||(bb)))
		write_text_inv_pos(tabstop6,menuLine,"5");
	else
		write_text_pos(tabstop6,menuLine,"5");
	if ((mainMenu_frameskip==6)&&((menuDisplay!=MENUDISPLAY_FRAMESKIP)||(bb)))
		write_text_inv_pos(tabstop7,menuLine,"6");
	else
		write_text_pos(tabstop7,menuLine,"6");
	if ((mainMenu_frameskip==7)&&((menuDisplay!=MENUDISPLAY_FRAMESKIP)||(bb)))
		write_text_inv_pos(tabstop8,menuLine,"7");
	else
		write_text_pos(tabstop8,menuLine,"7");
	if ((mainMenu_frameskip==8)&&((menuDisplay!=MENUDISPLAY_FRAMESKIP)||(bb)))
		write_text_inv_pos(tabstop9,menuLine,"8");
	else
		write_text_pos(tabstop9,menuLine,"8");

	menuLine+=12;
#endif

#if defined(__PSP2__) || defined(__SWITCH__)	
	//Shader settings on Vita
	//MENUDISPLAY_SHADER
	menuLine+=12;
	write_text_pos(leftMargin,menuLine,"Shader");
  
  	switch (mainMenu_shader)
  	{
#ifdef __SWITCH__
		case SHADER_NONE:
			snprintf((char*)value, 25, "NONE (auto 2x/3x/4x)");
			break;
		case SHADER_SHARP_BILINEAR_SIMPLE:
			snprintf((char*)value, 25, "SHARP_BILINEAR_SIMPLE");
			break;		
		case SHADER_BILINEAR:
			snprintf((char*)value, 25, "BILINEAR");
			break;
		case SHADER_POINT:
			snprintf((char*)value, 25, "POINT");
			break;
#else
		case SHADER_NONE:
			snprintf((char*)value, 25, "NONE (perfect 2x)");
			break;
		case SHADER_LCD3X:
			snprintf((char*)value, 25, "LCD3X");
			break;
		case SHADER_SCALE2X:
			snprintf((char*)value, 25, "SCALE2X");
			break;
		case SHADER_AAA:
			snprintf((char*)value, 25, "AAA");
			break;
		case SHADER_SHARP_BILINEAR:
			snprintf((char*)value, 25, "SHARP_BILINEAR");
			break;
		case SHADER_SHARP_BILINEAR_SIMPLE:
			snprintf((char*)value, 25, "SHARP_BILINEAR_SIMPLE");
			break;
		case SHADER_FXAA:
			snprintf((char*)value, 25, "FXAA");
			break;
#endif
		default:
			break;
	}
	if ((menuDisplay!=MENUDISPLAY_SHADER)||(bb))
		write_text_inv_pos(tabstop1,menuLine,value);
	else
		write_text_pos(tabstop1,menuLine,value);
#endif
	// MENUDISPLAY_STATUSLINE
	menuLine+=12;
	write_text_pos(leftMargin, menuLine,text_str_status_line);
	if ((!mainMenu_showStatus)&&((menuDisplay!=MENUDISPLAY_STATUSLINE)||(bb)))
		write_text_inv_pos(tabstop1,menuLine, "Off");
	else
		write_text_pos(tabstop1, menuLine, "Off");
	if ((mainMenu_showStatus)&&((menuDisplay!=MENUDISPLAY_STATUSLINE)||(bb)))
		write_text_inv_pos(tabstop3, menuLine,"On");
	else
		write_text_pos(tabstop3, menuLine,"On");

	// MENUDISPLAY_BACKGROUND
	menuLine+=12;
	write_text_pos(leftMargin, menuLine,"Menu Background");
	if ((mainMenu_background==0)&&((menuDisplay!=MENUDISPLAY_BACKGROUND)||(bb)))
		write_text_inv_pos(tabstop3,menuLine, "Static");
	else
		write_text_pos(tabstop3, menuLine, "Static");
	if ((mainMenu_background==1)&&((menuDisplay!=MENUDISPLAY_BACKGROUND)||(bb)))
		write_text_inv_pos(tabstop8-2*8, menuLine,"Moving");
	else
		write_text_pos(tabstop8-2*8, menuLine,"Moving");
		
	// MENUDISPLAY_FONT
	menuLine+=12;
	write_text_pos(leftMargin, menuLine,"Menu Font");
	if ((mainMenu_font==0)&&((menuDisplay!=MENUDISPLAY_FONT)||(bb)))
		write_text_inv_pos(tabstop3,menuLine, "Narrow");
	else
		write_text_pos(tabstop3, menuLine, "Narrow");

	if ((mainMenu_font==1)&&((menuDisplay!=MENUDISPLAY_FONT)||(bb)))
		write_text_inv_pos(tabstop3+7*8, menuLine,"Wide");
	else
		write_text_pos(tabstop3+7*8, menuLine,"Wide");

	if ((mainMenu_font==2)&&((menuDisplay!=MENUDISPLAY_FONT)||(bb)))
		write_text_inv_pos(tabstop3+12*8, menuLine,"Original");
	else
		write_text_pos(tabstop3+12*8, menuLine,"Original");

	menuLine+=8;
	write_text_pos(leftMargin,menuLine,text_str_display_separator);
	menuLine+=8;

	// MENUDISPLAY_SOUND
	write_text_pos(leftMargin,menuLine,text_str_sound);
	if ((mainMenu_sound==0)&&((menuDisplay!=MENUDISPLAY_SOUND)||(bb)))
		write_text_inv_pos(tabstop1,menuLine,text_str_off);
	else
		write_text_pos(tabstop1,menuLine,text_str_off);

	if ((mainMenu_sound==1)&&((menuDisplay!=MENUDISPLAY_SOUND)||(bb)))
		write_text_inv_pos(tabstop3,menuLine,text_str_fast);
	else
		write_text_pos(tabstop3,menuLine,text_str_fast);

	if ((mainMenu_sound==2)&&((menuDisplay!=MENUDISPLAY_SOUND)||(bb)))
		write_text_inv_pos(tabstop5+1*8,menuLine,text_str_accurate);
	else
		write_text_pos(tabstop5+1*8,menuLine,text_str_accurate);

	// MENUDISPLAY_SNDRATE
	menuLine+=12;
	write_text_pos(leftMargin,menuLine,text_str_sndrate);

	if ((sound_rate==8000)&&((menuDisplay!=MENUDISPLAY_SNDRATE)||(bb)))
		write_text_inv_pos(tabstop1,menuLine,text_str_8k);
	else
		write_text_pos(tabstop1,menuLine,text_str_8k);

	if ((sound_rate==11025)&&((menuDisplay!=MENUDISPLAY_SNDRATE)||(bb)))
		write_text_inv_pos(tabstop3,menuLine,text_str_11k);
	else
		write_text_pos(tabstop3,menuLine,text_str_11k);

	if ((sound_rate==22050)&&((menuDisplay!=MENUDISPLAY_SNDRATE)||(bb)))
		write_text_inv_pos(tabstop5,menuLine,text_str_22k);
	else
		write_text_pos(tabstop5,menuLine,text_str_22k);

	if ((sound_rate==32000)&&((menuDisplay!=MENUDISPLAY_SNDRATE)||(bb)))
		write_text_inv_pos(tabstop7,menuLine,text_str_32k);
	else
		write_text_pos(tabstop7,menuLine,text_str_32k);

	if ((sound_rate==44100)&&((menuDisplay!=MENUDISPLAY_SNDRATE)||(bb)))
		write_text_inv_pos(tabstop9,menuLine,text_str_44k);
	else
		write_text_pos(tabstop9,menuLine,text_str_44k);

	if ((sound_rate==48000)&&((menuDisplay!=MENUDISPLAY_SNDRATE)||(bb)))
		write_text_inv_pos(tabstop11,menuLine,text_str_48k);
	else
		write_text_pos(tabstop11,menuLine,text_str_48k);

	// MENUDISPLAY_STEREO
	menuLine+=12;
	write_text_pos(leftMargin,menuLine,"Stereo Sep.");
	if ((mainMenu_soundStereo==0)&&((menuDisplay!=MENUDISPLAY_STEREO)||(bb)))
		write_text_inv_pos(tabstop1,menuLine,"0");
	else
		write_text_pos(tabstop1,menuLine,"0");
		
	if ((mainMenu_soundStereo==1)&&(mainMenu_soundStereoSep==0)&&((menuDisplay!=MENUDISPLAY_STEREO)||(bb)))
		write_text_inv_pos(tabstop3,menuLine,"25");
	else
		write_text_pos(tabstop3,menuLine,"25");
		
	if ((mainMenu_soundStereo==1)&&(mainMenu_soundStereoSep==1)&&((menuDisplay!=MENUDISPLAY_STEREO)||(bb)))
		write_text_inv_pos(tabstop5,menuLine,"50");
	else
		write_text_pos(tabstop5,menuLine,"50");

	if ((mainMenu_soundStereo==1)&&(mainMenu_soundStereoSep==2)&&((menuDisplay!=MENUDISPLAY_STEREO)||(bb)))
		write_text_inv_pos(tabstop7,menuLine,"75");
	else
		write_text_pos(tabstop7,menuLine,"75");
	
		if ((mainMenu_soundStereo==1)&&(mainMenu_soundStereoSep==3)&&((menuDisplay!=MENUDISPLAY_STEREO)||(bb)))
		write_text_inv_pos(tabstop9,menuLine,"100");
	else
		write_text_pos(tabstop9,menuLine,"100");

	text_flip();
	b++;
}

static int key_displayMenu(int *c)
{
	int end=0;
	int left=0, right=0, up=0, down=0, hit0=0, hit1=0, hit2=0;
	SDL_Event event;

	while (SDL_PollEvent(&event) > 0)
	{
		left=right=up=down=hit0=hit1=hit2=0;
		if (event.type == SDL_KEYDOWN)
		{
			uae4all_play_click();
			switch(event.key.keysym.sym)
			{
			case DS_RIGHT1:
			case DS_RIGHT2:
			case DS_RIGHT3:
			case AK_RT: right=1; break;
			case DS_LEFT1:
			case DS_LEFT2:
			case DS_LEFT3:
			case AK_LF: left=1; break;
			case DS_UP1:
			case DS_UP2:
			case DS_UP3:
			case AK_UP: up=1; break;
			case DS_DOWN1:
			case DS_DOWN2:
			case DS_DOWN3:
			case AK_DN: down=1; break;
			case AK_RET:
			case AK_SPC:
			case DS_START:
			case DS_A: hit0=1; break;
			case AK_ESC:
			case DS_B: hit1=1; break;
			}
		}

		if (hit2) //Does the user want to cancel the menu completely?
		{
			if (emulating)
			{
				end = -1; 
				quit_pressed_in_submenu = 1; //Tell the mainMenu to cancel, too
			}
		}	
#if !defined(__PSP2__) && !defined(__SWITCH__)
		else if (hit0)
		{
			end = -1;
		}
#endif
		else if (hit1)
		{
			end = -1;
		}
		else if (up)
		{
			if (menuDisplay==0) menuDisplay = MENUDISPLAY_END - 1;
			else menuDisplay--;
		}
		else if (down)
		{
			if (menuDisplay == MENUDISPLAY_END - 1) menuDisplay=0;
			else menuDisplay++;
		}
		switch (menuDisplay)
		{
			case MENUDISPLAY_RETURNMAIN:
				if (hit0)
					end = -1;
				break;
			case MENUDISPLAY_PRESETWIDTH:
				if (left)
				{
					if(presetModeId < 10)
						SetPresetMode(presetModeId + 50);
					else
						SetPresetMode(presetModeId - 10);
				}
				if(right)
				{
					if(presetModeId > 50)
						SetPresetMode(presetModeId - 50);
					else
						SetPresetMode(presetModeId + 10);
				}
				break;
			case MENUDISPLAY_PRESETHEIGHT:
				if (left)
				{
					switch(presetModeId)
					{
						case 0:
						case 10:
						case 20:
						case 30:
						case 40:
						case 50:
							SetPresetMode(presetModeId + 7);
							break;
						default:
							SetPresetMode(presetModeId - 1);
					}
				}
				else if (right)
				{
					switch(presetModeId)
					{
						case 7:
						case 17:
						case 27:
						case 37:
						case 47:
						case 57:
							SetPresetMode(presetModeId - 7);
							break;
						default:
							SetPresetMode(presetModeId + 1);
					}
				}
				break;
			case MENUDISPLAY_DISPLINES:
				if (left)
				{
					if (mainMenu_displayedLines>100)
						mainMenu_displayedLines--;
				}
				else if (right)
				{
					if (mainMenu_displayedLines<286)
						mainMenu_displayedLines++;
				}
				break;
#if !defined(__PSP2__) && !defined(__SWITCH__)
			case MENUDISPLAY_SCREENWIDTH:
				if (left)
				{
					screenWidth-=10;
					if (screenWidth<200)
						screenWidth=200;
				}
				else if (right)
				{
					screenWidth+=10;
					if (screenWidth>800)
						screenWidth=800;
				}
				break;
#endif
			case MENUDISPLAY_VERTPOS:
				if (left)
				{
					if (moveY>-26)
						moveY--;
				}
				else if (right)
				{
					if (moveY<66)
						moveY++;
				}
				break;
#if !defined(__PSP2__) && !defined(__SWITCH__)
			case MENUDISPLAY_CUTLEFT:
				if (left)
				{
					if (mainMenu_cutLeft>0)
						mainMenu_cutLeft--;
				}
				else if (right)
				{
					if (mainMenu_cutLeft<100)
						mainMenu_cutLeft++;
				}
				break;
			case MENUDISPLAY_CUTRIGHT:
				if (left)
				{
					if (mainMenu_cutRight>0)
						mainMenu_cutRight--;
				}
				else if (right)
				{
					if (mainMenu_cutRight<100)
						mainMenu_cutRight++;
				}
				break;
#endif
			case MENUDISPLAY_FRAMESKIP:
#ifdef PANDORA
				if ((left)||(right))
						mainMenu_frameskip = !mainMenu_frameskip;
#else
				if (left)
				{
					if (mainMenu_frameskip>0)
						mainMenu_frameskip--;
					else
						mainMenu_frameskip=8;
				}
				else if (right)
				{
					if (mainMenu_frameskip<8)
						mainMenu_frameskip++;
					else
						mainMenu_frameskip=0;
				}
#endif
				break;
			case MENUDISPLAY_REFRESHRATE:
				if ((left)||(right))
						mainMenu_ntsc = !mainMenu_ntsc;
				break;
#if defined(__PSP2__) || defined(__SWITCH__) //shader choice on VITA
			case MENUDISPLAY_SHADER:
				if (left)
				{
					if (mainMenu_shader <= 0)
						mainMenu_shader = 0;
					else 
						mainMenu_shader -= 1;
				}
				else if (right)
				{
					if (mainMenu_shader >= NUM_SHADERS-1)
						mainMenu_shader = NUM_SHADERS-1;
					else
						mainMenu_shader +=1;
				}
				break;
#endif
			case MENUDISPLAY_STATUSLINE:
				if ((left)||(right))
					mainMenu_showStatus=!mainMenu_showStatus;
				break;

			case MENUDISPLAY_BACKGROUND:
				if ((left)||(right))
					mainMenu_background=!mainMenu_background;
				break;
			case MENUDISPLAY_FONT:
				if (left) {
					if ((mainMenu_font > 0) && (mainMenu_font <= 2)) {
						mainMenu_font--;
					} else {
						mainMenu_font = 2;
					}
				} else if (right) {
					if ((mainMenu_font < 2) && (mainMenu_font >= 0)) {
						mainMenu_font++;
					} else {
						mainMenu_font = 0;
					}
				}
				break;
			case MENUDISPLAY_SOUND:
					if (left)
					{
						if (mainMenu_sound == 1)
							mainMenu_sound = 0;
						else if (mainMenu_sound == 2)
							mainMenu_sound = 1;
						else if (mainMenu_sound == 0)
							mainMenu_sound = 2;
					}
					else if (right)
					{
						if (mainMenu_sound == 2)
							mainMenu_sound = 0;
						else if (mainMenu_sound == 0)
							mainMenu_sound = 1;
						else if (mainMenu_sound == 1)
							mainMenu_sound = 2;
					}
					break;
				case MENUDISPLAY_SNDRATE:
					if ((left)||(right))
					{
#ifndef __SWITCH__
						static int rates[] = { 8000, 11025, 22050, 32000, 44100, 48000 };
						int sel;
						for (sel = 0; sel < sizeof(rates) / sizeof(rates[0]); sel++)
							if (rates[sel] == sound_rate) break;
						sel += left ? -1 : 1;
						if (sel < 0) sel = 5;
						if (sel > 5) sel = 0;
						sound_rate = rates[sel];
#endif
					}
					break;
		
				case MENUDISPLAY_STEREO:
					if (left)
					{
						if(mainMenu_soundStereo == 0)
						{
							mainMenu_soundStereo=1;
							mainMenu_soundStereoSep=3;
						}
						else if (mainMenu_soundStereoSep > 0)
							mainMenu_soundStereoSep--;
						else 
						{	
							mainMenu_soundStereo=0;
							mainMenu_soundStereoSep=3;
						}
					}		
					if (right)
					{
						if(mainMenu_soundStereo == 0)
						{
							mainMenu_soundStereo=1;
							mainMenu_soundStereoSep=0;
						}
						else if (mainMenu_soundStereoSep < 3)
						{
							mainMenu_soundStereoSep++;
						}
						else 
						{	
							mainMenu_soundStereo=0;
							mainMenu_soundStereoSep=3;
						}
					}		
				
					break;
		}
	}
	return end;
}

static void raise_displayMenu()
{
	int i;

	text_draw_background();
	text_flip();
#if !defined(__PSP2__) && !defined(__SWITCH__)
	for(i=0;i<10;i++)
	{
		text_draw_background();
		text_draw_window(80-64,(10-i)*24,160+64+64,220,text_str_display_title);
		text_flip();
	}
#endif
}

static void unraise_displayMenu()
{
	int i;
#if !defined(__PSP2__) && !defined(__SWITCH__)
	for(i=9;i>=0;i--)
	{
		text_draw_background();
		text_draw_window(80-64,(10-i)*24,160+64+64,220,text_str_display_title);
		text_flip();
	}
#endif
	text_draw_background();
	text_flip();
}

int run_menuDisplay()
{
	SDL_Event event;
	SDL_Delay(150);
	while(SDL_PollEvent(&event))
		SDL_Delay(10);
	int end=0, c=0;
	raise_displayMenu();
	while(!end)
	{
		draw_displayMenu(c);
		end=key_displayMenu(&c);
	}
	set_joyConf();
	unraise_displayMenu();
	return end;
}
