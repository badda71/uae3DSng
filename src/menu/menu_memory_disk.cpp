#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "menu.h"
#include "menu_config.h"

#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "autoconf.h"
#include "uae.h"
#include "options.h"
#include "sound.h"
#include "gui.h"
#include <SDL/SDL.h>
#include "gp2x.h"

/* PocketUAE config file. Used for parsing PocketUAE-like options. */
#include "cfgfile.h" 
#include "keyboard.h"
#include "uibottom.h"

const char *text_str_memdisk_separator="--------------------------------------";
static const char *text_str_memdisk_title=    "Harddisk and Memory Options";
const char *text_str_off="off";
const char *text_str_on="on";
const char *text_str_512K="512K";
const char *text_str_1M="1M";
const char *text_str_1_5M="1.5M";
const char *text_str_2M="2M";
const char *text_str_4M="4M";
const char *text_str_8M="8M";

int menuMemDisk = 0;
int current_hdf = 0;
extern int quit_pressed_in_submenu;
extern int emulating;

enum { 
	MENUDISK_RETURNMAIN = 0,
	MENUDISK_CHIPMEM,
	MENUDISK_SLOWMEM,
	MENUDISK_FASTMEM,
	MENUDISK_BOOTHD,
	MENUDISK_HDDIR,
	MENUDISK_HDFILE,
	MENUDISK_SAVEHDCONF,
	MENUDISK_FLOPPYSPEED,
	MENUDISK_END
};

extern char currentDir[300];

static void draw_memDiskMenu(int c)
{
	static int b=0;
	int bb=(b%6)/3;		/* Inverted/normal selection drawing */
	int menuWidth = 38;
	int menuHeight = 21;

	int leftMargin = ((50 - menuWidth) / 2)*8;
	int tabstop1 = leftMargin + 14*8;
	int tabstop2 = leftMargin + 16*8;
	int tabstop3 = leftMargin + 18*8;
	int tabstop4 = leftMargin + 20*8;
	int tabstop5 = leftMargin + 22*8;
	int tabstop6 = leftMargin + 24*8;
	int tabstop7 = leftMargin + 26*8;
	int tabstop8 = leftMargin + 28*8;
	int tabstop9 = leftMargin + 30*8;
	
	int menuLine = ((31 - menuHeight) / 2)*8;
	extern SDL_Surface *text_screen;

	text_draw_background();
	text_draw_window(leftMargin/8,menuLine/8,menuWidth,menuHeight,text_str_memdisk_title);
	
	if ((menuMemDisk == 0)&&(c==MENUDISK_RETURNMAIN)&&(bb))
		write_text_inv_pos(leftMargin, menuLine, "Return to main menu");
	else
		write_text_pos(leftMargin, menuLine, "Return to main menu");

	menuLine+=8;
	write_text_pos(leftMargin, menuLine, text_str_memdisk_separator);
	menuLine+=8;
	
	write_text_pos(leftMargin,menuLine,"Chip Memory");
	if ((mainMenu_chipMemory==0)&&((c!=MENUDISK_CHIPMEM)||(!bb)))
		write_text_inv_pos(tabstop3 + 4*8,menuLine,text_str_512K);
	else
		write_text_pos(tabstop3 + 4*8,menuLine,text_str_512K);
	
	if ((mainMenu_chipMemory==1)&&((c!=MENUDISK_CHIPMEM)||(!bb)))
		write_text_inv_pos(tabstop3 + 9*8,menuLine,text_str_1M);
	else
		write_text_pos(tabstop3 + 9*8,menuLine,text_str_1M);

	if ((mainMenu_chipMemory==2)&&((c!=MENUDISK_CHIPMEM)||(!bb)))
		write_text_inv_pos(tabstop3 + 12*8,menuLine,text_str_2M);
	else
		write_text_pos(tabstop3 + 12*8,menuLine,text_str_2M);

	if ((mainMenu_chipMemory==3)&&((c!=MENUDISK_CHIPMEM)||(!bb)))
		write_text_inv_pos(tabstop3 + 15*8,menuLine,text_str_4M);
	else
		write_text_pos(tabstop3 + 15*8,menuLine,text_str_4M);
	
	if ((mainMenu_chipMemory==4)&&((c!=MENUDISK_CHIPMEM)||(!bb)))
		write_text_inv_pos(tabstop3 + 18*8,menuLine,text_str_8M);
	else
		write_text_pos(tabstop3 + 18*8,menuLine,text_str_8M);
	
	menuLine+=12;
	write_text_pos(leftMargin,menuLine,"Slow Memory");
	if ((mainMenu_slowMemory==0)&&((c!=MENUDISK_SLOWMEM)||(!bb)))
		write_text_inv_pos(tabstop3,menuLine,text_str_off);
	else
		write_text_pos(tabstop3,menuLine,text_str_off);
	
	if ((mainMenu_slowMemory==1)&&((c!=MENUDISK_SLOWMEM)||(!bb)))
		write_text_inv_pos(tabstop3 + 4*8,menuLine,text_str_512K);
	else
		write_text_pos(tabstop3 + 4*8,menuLine,text_str_512K);
	
	if ((mainMenu_slowMemory==2)&&((c!=MENUDISK_SLOWMEM)||(!bb)))
		write_text_inv_pos(tabstop3 + 9*8,menuLine,text_str_1M);
	else
		write_text_pos(tabstop3 + 9*8,menuLine,text_str_1M);

	if ((mainMenu_slowMemory==3)&&((c!=MENUDISK_SLOWMEM)||(!bb)))
		write_text_inv_pos(tabstop3 + 12*8,menuLine,text_str_1_5M);
	else
		write_text_pos(tabstop3 + 12*8,menuLine,text_str_1_5M);
	
	menuLine+=12;
	write_text_pos(leftMargin,menuLine,"Fast Memory");
	if ((mainMenu_fastMemory==0)&&((c!=MENUDISK_FASTMEM)||(!bb)))
		write_text_inv_pos(tabstop3,menuLine,text_str_off);
	else
		write_text_pos(tabstop3,menuLine,text_str_off);

	if ((mainMenu_fastMemory==1)&&((c!=MENUDISK_FASTMEM)||(!bb)))
		write_text_inv_pos(tabstop3 + 9*8,menuLine,text_str_1M);
	else
		write_text_pos(tabstop3 + 9*8,menuLine,text_str_1M);
	
	if ((mainMenu_fastMemory==2)&&((c!=MENUDISK_FASTMEM)||(!bb)))
		write_text_inv_pos(tabstop3 + 12*8,menuLine,text_str_2M);
	else
		write_text_pos(tabstop3 + 12*8,menuLine,text_str_2M);

	if ((mainMenu_fastMemory==3)&&((c!=MENUDISK_FASTMEM)||(!bb)))
		write_text_inv_pos(tabstop3 + 15*8,menuLine,text_str_4M);
	else
		write_text_pos(tabstop3 + 15*8,menuLine,text_str_4M);

	if ((mainMenu_fastMemory==4)&&((c!=MENUDISK_FASTMEM)||(!bb)))
		write_text_inv_pos(tabstop3 + 18*8,menuLine,text_str_8M);
	else
		write_text_pos(tabstop3 + 18*8,menuLine,text_str_8M);

	menuLine+=8;
	write_text_pos(leftMargin, menuLine, text_str_memdisk_separator);
	menuLine+=8;
	
	write_text_pos(leftMargin,menuLine,"Boot HD");
	if ((mainMenu_bootHD==0)&&((c!=MENUDISK_BOOTHD)||(!bb)))
		write_text_inv_pos(tabstop3,menuLine,text_str_off);
	else
		write_text_pos(tabstop3,menuLine,text_str_off);

	if ((mainMenu_bootHD==1)&&((c!=MENUDISK_BOOTHD)||(!bb)))
		write_text_inv_pos(tabstop3 + 7*8,menuLine,"Dir");
	else
		write_text_pos(tabstop3 + 7*8,menuLine,"Dir");
	
	if ((mainMenu_bootHD==2)&&((c!=MENUDISK_BOOTHD)||(!bb)))
		write_text_inv_pos(tabstop3 + 12*8,menuLine,"File1");
	else
		write_text_pos(tabstop3 + 12*8,menuLine,"File1");
	
	menuLine+=12;

	{
		char str[256];
		int i;
		
		strcpy(str, "HD Dir");
		if ((c==MENUDISK_HDDIR)&&(bb))
			write_text_inv_pos(leftMargin + 2*8,menuLine,str);
		else
			write_text_pos(leftMargin + 2*8,menuLine,str);
		for (i = strlen(uae4all_hard_dir); i > 0; i--)
			if ((uae4all_hard_dir[i] == '/')||(uae4all_hard_dir[i] == '\\'))
				break;
		if (i > 0) {
			strcpy(str, &uae4all_hard_dir[i+1]);
			if (strlen(str) > (MENU_MEMDISK_WINDOW_WIDTH - tabstop1))
				str[MENU_MEMDISK_WINDOW_WIDTH - tabstop1] = '\0';
			write_text_pos(tabstop1,menuLine,str);
		} else
			write_text_pos(tabstop1,menuLine,"");
		
		menuLine += 12;
		
		strcpy(str, "HD File1");
		if ((c==MENUDISK_HDFILE)&&(bb)&&current_hdf==0)
			write_text_inv_pos(leftMargin + 2*8,menuLine,str);
		else
			write_text_pos(leftMargin + 2*8,menuLine,str);
		for (i = strlen(uae4all_hard_file0); i > 0; i--)
			if ((uae4all_hard_file0[i] == '/') || (uae4all_hard_file0[i] == '\\'))
				break;
		if (i > 0) {
			strcpy(str, &uae4all_hard_file0[i+1]);
			if (strlen(str) > MENU_MEMDISK_WINDOW_WIDTH - tabstop1)
				str[MENU_MEMDISK_WINDOW_WIDTH - tabstop1] = '\0';
			write_text_pos(tabstop1,menuLine,str);
		} else
			write_text_pos(tabstop1,menuLine,"");

		menuLine += 12;

		strcpy(str, "HD File2");
		if ((c==MENUDISK_HDFILE)&&(bb)&&current_hdf==1)
			write_text_inv_pos(leftMargin + 2*8,menuLine,str);
		else
			write_text_pos(leftMargin + 2*8,menuLine,str);
		for (i = strlen(uae4all_hard_file1); i > 0; i--)
			if ((uae4all_hard_file1[i] == '/') || (uae4all_hard_file1[i] == '\\'))
				break;
		if (i > 0) {
			strcpy(str, &uae4all_hard_file1[i+1]);
			if (strlen(str) > MENU_MEMDISK_WINDOW_WIDTH - tabstop1)
				str[MENU_MEMDISK_WINDOW_WIDTH - tabstop1] = '\0';
			write_text_pos(tabstop1,menuLine,str);
		} else
			write_text_pos(tabstop1,menuLine,"");

		menuLine += 12;

		strcpy(str, "HD File3");
		if ((c==MENUDISK_HDFILE)&&(bb)&&current_hdf==2)
			write_text_inv_pos(leftMargin + 2*8,menuLine,str);
		else
			write_text_pos(leftMargin + 2*8,menuLine,str);
		for (i = strlen(uae4all_hard_file2); i > 0; i--)
			if ((uae4all_hard_file2[i] == '/') || (uae4all_hard_file2[i] == '\\'))
				break;
		if (i > 0) {
			strcpy(str, &uae4all_hard_file2[i+1]);
			if (strlen(str) > MENU_MEMDISK_WINDOW_WIDTH - tabstop1)
				str[MENU_MEMDISK_WINDOW_WIDTH - tabstop1] = '\0';
			write_text_pos(tabstop1,menuLine,str);
		} else
			write_text_pos(tabstop1,menuLine,"");
			
		menuLine += 12;

		strcpy(str, "HD File4");
		if ((c==MENUDISK_HDFILE)&&(bb)&&current_hdf==3)
			write_text_inv_pos(leftMargin + 2*8,menuLine,str);
		else
			write_text_pos(leftMargin + 2*8,menuLine,str);
		for (i = strlen(uae4all_hard_file3); i > 0; i--)
			if ((uae4all_hard_file3[i] == '/') || (uae4all_hard_file3[i] == '\\'))
				break;
		if (i > 0) {
			strcpy(str, &uae4all_hard_file3[i+1]);
			if (strlen(str) > MENU_MEMDISK_WINDOW_WIDTH - tabstop1)
				str[MENU_MEMDISK_WINDOW_WIDTH - tabstop1] = '\0';
			write_text_pos(tabstop1,menuLine,str);
		} else
			write_text_pos(tabstop1,menuLine,"");

	}

	menuLine += 12;
	
	write_text_pos(leftMargin, menuLine, "(Press X to eject HD)");
	
	menuLine += 12;
		
	if ((c==MENUDISK_SAVEHDCONF)&&(bb))
		write_text_inv_pos(leftMargin, menuLine, "Save Config for current HD");
	else
		write_text_pos(leftMargin, menuLine, "Save Config for current HD");
	
	menuLine+=8;
	write_text_pos(leftMargin, menuLine, text_str_memdisk_separator);
	menuLine+=8;

	write_text_pos(leftMargin,menuLine,"Floppy speed:");

	if ((mainMenu_floppyspeed==100)&&((c!=MENUDISK_FLOPPYSPEED)||(bb)))
		write_text_inv_pos(tabstop3,menuLine,"1x");
	else
		write_text_pos(tabstop3,menuLine,"1x");

	if ((mainMenu_floppyspeed==200)&&((c!=MENUDISK_FLOPPYSPEED)||(bb)))
		write_text_inv_pos(tabstop4+1*8,menuLine,"2x");
	else
		write_text_pos(tabstop4+1*8,menuLine,"2x");

	if ((mainMenu_floppyspeed==400)&&((c!=MENUDISK_FLOPPYSPEED)||(bb)))
		write_text_inv_pos(tabstop5+2*8,menuLine,"4x");
	else
		write_text_pos(tabstop5+2*8,menuLine,"4x");

	if ((mainMenu_floppyspeed==800)&&((c!=MENUDISK_FLOPPYSPEED)||(bb)))
		write_text_inv_pos(tabstop6+3*8,menuLine,"8x");
	else
		write_text_pos(tabstop6+3*8,menuLine,"8x");

	menuLine+=8;
	write_text_pos(leftMargin, menuLine, text_str_memdisk_separator);

	text_flip();

	b++;
}


static int key_memDiskMenu(int *c)
{
	int end=0;
	int left=0, right=0, up=0, down=0, hit0=0, hit1=0, hit2=0, del=0;

	SDL_Event event;

	while (SDL_PollEvent(&event) > 0)
	{
		if (uib_handle_event(&event)) continue;
		left=right=up=down=hit0=hit1=hit2=del=0;
		if (event.type == SDL_QUIT)
			end=-1;
		else if (event.type == SDL_KEYDOWN)
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
				case AK_X:
				case DS_X: del=1; break;
//				case SDLK_LCTRL: hit2=1; break; //allow user to quit menu completely at any time
				default:
					break;
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
		else if (hit1)
		{
			end=-1;
		}
		else if (up)
		{
			if (menuMemDisk==MENUDISK_RETURNMAIN) menuMemDisk=MENUDISK_END - 1;
			else if (menuMemDisk==MENUDISK_HDFILE && current_hdf>0) 
					current_hdf--;
			else if (menuMemDisk==MENUDISK_SAVEHDCONF)
			{
				current_hdf=3;
				menuMemDisk=MENUDISK_HDFILE;
			}
			else
				menuMemDisk--;
		}
		else if (down)
		{
			if (menuMemDisk==MENUDISK_END - 1) menuMemDisk=MENUDISK_RETURNMAIN;
			else if (menuMemDisk==MENUDISK_HDFILE && current_hdf<3) 
					current_hdf++;
			else if (menuMemDisk==MENUDISK_HDDIR)
			{
				current_hdf=0;
				menuMemDisk=MENUDISK_HDFILE;
			}
			else
				menuMemDisk++;	
		}
		switch (menuMemDisk)
		{
			case MENUDISK_RETURNMAIN:
				if (hit0)
					end = 1;
				break;
			case MENUDISK_CHIPMEM:
				if ((left)||(right)) {
					if (right) {
						if (mainMenu_chipMemory < 4)
							mainMenu_chipMemory++;
						else
							mainMenu_chipMemory = 0;
					} else if (left) {
						if (mainMenu_chipMemory > 0)
							mainMenu_chipMemory--;
						else
							mainMenu_chipMemory = 4;
					}
					UpdateMemorySettings();
				}
				break;
			case MENUDISK_SLOWMEM:
				if ((left)||(right)) {
					if (right) {
						if (mainMenu_slowMemory < 3)
							mainMenu_slowMemory++;
						else
							mainMenu_slowMemory = 0;
					} else if (left) {
						if (mainMenu_slowMemory > 0)
							mainMenu_slowMemory--;
						else
							mainMenu_slowMemory = 3;
					}
					UpdateMemorySettings();
				}
				break;
			case MENUDISK_FASTMEM:
				if ((left) || (right)) {
					if (right) {
						if (mainMenu_fastMemory < 4)
							mainMenu_fastMemory++;
						else
							mainMenu_fastMemory = 0;
					} else if (left) {
						if (mainMenu_fastMemory > 0)
							mainMenu_fastMemory--;
						else
							mainMenu_fastMemory = 4;
					}
				
					/* Fast memory > 0 => max 2MB chip memory */
					if ((mainMenu_fastMemory > 0) && (mainMenu_chipMemory > 2))
						mainMenu_chipMemory = 2;
					UpdateMemorySettings();
				}
				break;
			case MENUDISK_BOOTHD:
				if (left) {
					if (mainMenu_bootHD > 0)
						mainMenu_bootHD--;
					else
						mainMenu_bootHD = 2;
				} else if (right) {
					if (mainMenu_bootHD < 2)
						mainMenu_bootHD++;
					else
						mainMenu_bootHD = 0;
				}
				break;
			case MENUDISK_HDDIR:
				if (hit0) {
					if (run_menuLoad(currentDir, MENU_LOAD_HD_DIR)) {
						make_hard_dir_cfg_line(uae4all_hard_dir);
						loadconfig(4);
					}
				} else if (del) {
					uae4all_hard_dir[0] = '\0';
				}
				break;
			case MENUDISK_HDFILE:
				if (hit0) {
					if (run_menuLoad(currentDir, MENU_LOAD_HDF)) {
						if (current_hdf==0)
							make_hard_file_cfg_line(uae4all_hard_file0);
						else if (current_hdf==1)
							make_hard_file_cfg_line(uae4all_hard_file1);
						else if (current_hdf==2)
							make_hard_file_cfg_line(uae4all_hard_file2);
						else if (current_hdf==3)
							make_hard_file_cfg_line(uae4all_hard_file3);
						if (current_hdf==0) {
							loadconfig(2);
						}
					}
				} else if (del) {
					if (current_hdf==0)
						uae4all_hard_file0[0] = '\0';
					else if (current_hdf==1)
						uae4all_hard_file1[0] = '\0';
					else if (current_hdf==2)
						uae4all_hard_file2[0] = '\0';
					else if (current_hdf==3)
						uae4all_hard_file3[0] = '\0';
				}
				break;
			case MENUDISK_SAVEHDCONF:
				if (hit0)
				{
					if (saveconfig(2))
						showWarning("Config saved for current HD.");
				}
				break;
			case MENUDISK_FLOPPYSPEED:
				if (left)
				{
					if (mainMenu_floppyspeed>100)
						mainMenu_floppyspeed/=2;
					else
						mainMenu_floppyspeed=800;
				}
				else if (right)
				{
					if (mainMenu_floppyspeed<800)
						mainMenu_floppyspeed*=2;
					else
						mainMenu_floppyspeed=100;
				}
		}
	}
	
	*c = menuMemDisk;
	
	return end;
}

static void raise_memDiskMenu()
{
	int i;

	text_draw_background();
	text_flip();
#if !defined(__PSP2__) && !defined(__SWITCH__)
	for(i=0;i<10;i++)
	{
		text_draw_background();
		text_draw_window(80-64,(10-i)*24,160+64+64,220,text_str_memdisk_title);
		text_flip();
	}
#endif
}

static void unraise_memDiskMenu()
{
	int i;
#if !defined(__PSP2__) && !defined(__SWITCH__)
	for(i=9;i>=0;i--)
	{
		text_draw_background();
		text_draw_window(80-64,(10-i)*24,160+64+64,220,text_str_memdisk_title);
		text_flip();
	}
#endif
	text_draw_background();
	text_flip();
}

int run_menuMemDisk()
{	
	int end=0,c=0;
	raise_memDiskMenu();
	
	while(!end)
	{
		draw_memDiskMenu(c);
		end = key_memDiskMenu(&c);
	}
	
	unraise_memDiskMenu();
	return end;
}
