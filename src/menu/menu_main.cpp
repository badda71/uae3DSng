#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "keyboard.h"

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <SDL/SDL.h>

#include "gp2xutil.h"
#include "menu.h"
#include "menu_config.h"
#include "autoconf.h"
#include "options.h"
#include "sound.h"
#include "zfile.h"
#include "gui.h"
#include "gp2x.h"
#include "disk.h"
#include "cpuctrl.h"
#include "custom.h"
#include "savestate.h"
#include <3ds.h>
 
int inside_menu = 0;

extern int kickstart;
extern int oldkickstart;
extern int bReloadKickstart;
extern unsigned int sound_rate;
extern int moveX;
extern int moveY;
extern int timeslice_mode;
extern int emulating;
extern int gp2xMouseEmuOn;
extern int gp2xButtonRemappingOn;

extern int init_sound(void);
extern void leave_program(void);
extern void extractFileName(char * str,char *buffer);
extern void update_display(void);
extern int saveAdfDir(void);
extern void setCpuSpeed(void);
extern void show_error(const char *);
extern void close_joystick();
extern void init_joystick();

extern char launchDir[300];
extern char currentDir[300];

extern char filename0[256];
extern char filename1[256];
extern char filename2[256];
extern char filename3[256];

extern char config_load_filename[300];

static const char *text_str_title=    "----- UAE3DS -----";
static const char *text_str_df0=		"DF0:";
static const char *text_str_df1=		"DF1:";
static const char *text_str_df2=		"DF2:";
static const char *text_str_df3=		"DF3:";
static const char* text_str_hdnmem="Harddisk and Memory Options";
static const char *text_str_display="Display and Sound";
static const char *text_str_savestates="Savestates";
static const char *text_str_eject="Eject All Drives";
const char *text_str_separator="---------------------------------";
static const char *text_str_reset="Reset (R)";
static const char *text_str_exit= "Quit (L)";
static const char *text_str_custom= "Custom Control Config";
static const char *text_str_more= "More Options";
static const char *text_str_releasenotes= "Release Notes";

int mainMenu_case=-1;
int mainMenu_system=-1;
int quit_pressed_in_submenu=0;

int nr_drives=DEFAULT_DRIVES;
int current_drive=0;

int force_quit=0;

int lastCpuSpeed=600;
int ntsc=0;

static void draw_mainMenu(int c)
{
	static int b=0;
	int bb=(b%6)/3;
	int menuWidth = 33;
	int menuHeight = 26;
	
	int leftMargin = ((50 - menuWidth) / 2)*8;
	int menuLine = ((31 - menuHeight) / 2)*8;
	int tabstop1 = leftMargin+13*8;
	int tabstop2 = leftMargin+15*8;
	int tabstop3 = leftMargin+17*8;
	int tabstop4 = leftMargin+19*8;
	int tabstop5 = leftMargin+21*8;
	int tabstop6 = leftMargin+23*8;
	int tabstop7 = leftMargin+25*8;
	int tabstop8 = leftMargin+27*8;
	int tabstop9 = leftMargin+29*8;

	text_draw_background();
	text_draw_window(leftMargin/8,menuLine/8,menuWidth,menuHeight,text_str_title);

	// 0
	if ((c==0)&&(bb))
		write_text_inv_pos(leftMargin,menuLine,text_str_df0);
	else
		write_text_pos(leftMargin,menuLine,text_str_df0);
	if(strcmp(uae4all_image_file0, "")==0)
		write_text_inv_pos(leftMargin+6*8,menuLine,"insert disk image");
	else
		write_text_inv_pos(leftMargin+6*8,menuLine,filename0);

	// 1
	menuLine+=12;
	if(nr_drives > 1)
	{
		if((c==1)&&(bb))
			write_text_inv_pos(leftMargin,menuLine,text_str_df1);
		else
			write_text_pos(leftMargin,menuLine,text_str_df1);
		if(strcmp(uae4all_image_file1, "")==0)
			write_text_inv_pos(leftMargin+6*8,menuLine,"insert disk image");
		else
			write_text_inv_pos(leftMargin+6*8,menuLine,filename1);
	}

	// 2
	menuLine+=12;
	if(nr_drives > 2)
	{
		if ((c==2)&&(bb))
			write_text_inv_pos(leftMargin,menuLine,text_str_df2);
		else
			write_text_pos(leftMargin,menuLine,text_str_df2);
		if(strcmp(uae4all_image_file2, "")==0)
			write_text_inv_pos(leftMargin+6*8,menuLine,"insert disk image");
		else
			write_text_inv_pos(leftMargin+6*8,menuLine,filename2);
	}

	// 3
	menuLine+=12;
	if(nr_drives > 3)
	{
		if ((c==3)&&(bb))
			write_text_inv_pos(leftMargin,menuLine,text_str_df3);
		else
			write_text_pos(leftMargin,menuLine,text_str_df3);
		if(strcmp(uae4all_image_file3, "")==0)
			write_text_inv_pos(leftMargin+6*8,menuLine,"insert disk image");
		else
			write_text_inv_pos(leftMargin+6*8,menuLine,filename3);
	}

	menuLine+=8;
	write_text_pos(leftMargin,menuLine,text_str_separator);
	menuLine+=8;

	// 4
	if ((c==4)&&(bb))
		write_text_inv_pos(leftMargin,menuLine,text_str_eject);
	else
		write_text_pos(leftMargin, menuLine,text_str_eject);

	// 5
	menuLine+=12;
	write_text_pos(leftMargin,menuLine,"Number of Drives");
	
	if ((nr_drives==1)&&((c!=5)||(bb)))
		write_text_inv_pos(tabstop3,menuLine,"1");
	else
		write_text_pos(tabstop3,menuLine,"1");

	if ((nr_drives==2)&&((c!=5)||(bb)))
		write_text_inv_pos(tabstop4,menuLine,"2");
	else
		write_text_pos(tabstop4,menuLine,"2");

	if ((nr_drives==3)&&((c!=5)||(bb)))
		write_text_inv_pos(tabstop5,menuLine,"3");
	else
		write_text_pos(tabstop5,menuLine,"3");

	if ((nr_drives==4)&&((c!=5)||(bb)))
		write_text_inv_pos(tabstop6,menuLine,"4");
	else
		write_text_pos(tabstop6,menuLine,"4");

	menuLine+=8;
	write_text_pos(leftMargin,menuLine,text_str_separator);
	menuLine+=8;

	// 6
	write_text_pos(leftMargin,menuLine,"Preset System Setup:");

	if ((mainMenu_system!=1)&&((c!=6)||(bb)))
		write_text_inv_pos(tabstop5+1,menuLine,"A500");
	else
		write_text_pos(tabstop5+1,menuLine,"A500");

	if ((mainMenu_system!=0)&&((c!=6)||(bb)))
		write_text_inv_pos(tabstop8,menuLine,"A1200");
	else
		write_text_pos(tabstop8,menuLine,"A1200");

	// 7
	menuLine+=12;
	if ((c==7)&&(bb))
		write_text_inv_pos(leftMargin,menuLine,text_str_hdnmem);
	else
		write_text_pos(leftMargin,menuLine,text_str_hdnmem);

	// 8
	menuLine+=12;
	if ((c==8)&&(bb))
		write_text_inv_pos(leftMargin,menuLine,text_str_display);
	else
		write_text_pos(leftMargin,menuLine,text_str_display);

	// 9
	menuLine+=12;
	if ((c==9)&&(bb))
		write_text_inv_pos(leftMargin,menuLine,text_str_savestates);
	else
		write_text_pos(leftMargin,menuLine,text_str_savestates);

	// 10
	menuLine+=12;
	if ((c==10)&&(bb))
		write_text_inv_pos(leftMargin,menuLine,text_str_custom);
	else
		write_text_pos(leftMargin,menuLine,text_str_custom);

	// 11
	menuLine+=12;
	if ((c==11)&&(bb))
		write_text_inv_pos(leftMargin,menuLine,text_str_more);
	else
		write_text_pos(leftMargin,menuLine,text_str_more);

	menuLine+=8;
	write_text_pos(leftMargin,menuLine,text_str_separator);

	// 12
	menuLine+=8;
	if ((c==12)&&(bb))
		write_text_inv_pos(leftMargin,menuLine,text_str_reset);
	else
		write_text_pos(leftMargin,menuLine,text_str_reset);

	menuLine+=8;
	write_text_pos(leftMargin,menuLine,text_str_separator);

	// 13
	menuLine+=8;
	write_text_pos(leftMargin,menuLine,"Config");

	if ((c==13)&&(bb))
		write_text_inv_pos(leftMargin+7*8,menuLine,"Load");
	else
		write_text_pos(leftMargin+7*8,menuLine,"Load");

	// 14
	if ((c==14)&&(bb))
		write_text_inv_pos(leftMargin+13*8,menuLine,"Save As");
	else
		write_text_pos(leftMargin+13*8,menuLine,"Save As");

	// 15
	if ((c==15)&&(bb))
		write_text_inv_pos(leftMargin+22*8,menuLine,"Delete");
	else
		write_text_pos(leftMargin+22*8,menuLine,"Delete");

	// 16
	menuLine+=12;
	if ((c==16)&&(bb))
		write_text_inv_pos(leftMargin+7*8,menuLine,"Save General");
	else
		write_text_pos(leftMargin+7*8,menuLine,"Save General");

	// 17
	if ((c==17)&&(bb))
		write_text_inv_pos(leftMargin+20*8,menuLine,"Save Per-Game");
	else
		write_text_pos(leftMargin+20*8,menuLine,"Save Per-Game");

	menuLine+=8;
	write_text_pos(leftMargin,menuLine,text_str_separator);

	// 18
	menuLine+=8;
	if ((c==18)&&(bb))
		write_text_inv_pos(leftMargin,menuLine,text_str_exit);
	else
		write_text_pos(leftMargin,menuLine,text_str_exit);

	text_flip();
	
	b++;
}

void showWarning(const char *msg)
{
	text_draw_window(4,9,37,4,"Message");
	write_text_pos(5,11,msg);
	//write_text_pos(11,16,"Press any button to continue");
	text_flip();
	SDL_Event ev;
	SDL_Delay(1000);
	while(SDL_PollEvent(&ev))
	{
		if (ev.type==SDL_QUIT)
			exit(1);
		SDL_Delay(10);
	}
}

void setSystem()
{
	if(mainMenu_system > 0)
	{
		mainMenu_chipMemory=2;
		mainMenu_slowMemory=0;
		mainMenu_fastMemory=4;
		kickstart=3;
		mainMenu_CPU_model=1;
		mainMenu_chipset=2;
		mainMenu_chipset|=0x100;//set blitter to "immediate"
		mainMenu_CPU_speed=1;
	}
	else
	{
		mainMenu_chipMemory=1;
		mainMenu_slowMemory=0;
		mainMenu_fastMemory=0;
		kickstart=1;
		mainMenu_CPU_model=0;
		mainMenu_chipset=0;
		mainMenu_chipset|=0x100;//set blitter to "immediate"
		mainMenu_CPU_speed=0;
	}
	UpdateMemorySettings();
	UpdateCPUModelSettings();
	UpdateChipsetSettings();
}

static int key_mainMenu(int *cp)
{
	int back_c=-1;
	int c=(*cp);
	int end=0;
	int left=0, right=0, up=0, down=0, hit0=0, hit1=0, hit2=0, hit3=0, hit4=0, hit5=0, hit6=0, hitH=0, hitS=0, hitQ=0, hitN1=0, hitN2=0, hitN3=0, hitN4=0;
	SDL_Event event;
	
	force_quit=0;

	while (SDL_PollEvent(&event) > 0)
	{
		left=right=up=down=hit0=hit1=hit2=hit3=hit4=hit5=hit6=hitH=hitS=hitQ=hitN1=hitN2=hitN3=hitN4=0;
		if (event.type == SDL_QUIT)
		{
			mainMenu_case=MAIN_MENU_CASE_QUIT;
			end=-1;
		}
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
				case DS_L:
				case AK_L: hitQ=1; break;
				case DS_R:
				case AK_R: hit4=1; break;
//				case SDLK_RCTRL: hit4=1; break;
//				case SDLK_h: hitH=1; break;
//				case SDLK_s: hitS=1; break;
//				case SDLK_q: hitQ=1; break;
//				case SDLK_1: hitN1=1; break;
//				case SDLK_2: hitN2=1; break;
//				case SDLK_3: hitN3=1; break;
//				case SDLK_4: hitN4=1;
			}
		}

		if (hit1)
		{
			mainMenu_case=MAIN_MENU_CASE_RUN;
			end=1;
		}
		else if (hit2)
		{
			mainMenu_case=MAIN_MENU_CASE_CANCEL;
			end=1;
		}
		else if (hit3)
		{
			mainMenu_case=MAIN_MENU_CASE_DISPLAY;
			end=1;
		}
		else if (hit4)
		{
			// reset
			back_c = c;
			hit0 = 1;
			c = 12;
		}
		else if (hit5)
		{
			// more options
			back_c = c;
			hit0 = 1;
			c = 11;
		}
		else if (hit6)
		{
			// custom controls
			back_c = c;
			hit0 = 1;
			c = 10;
		}
		else if (hitH)
		{
			mainMenu_case=MAIN_MENU_CASE_MEMDISK;
			end=1;
		}
		else if (hitS)
		{
			mainMenu_case=MAIN_MENU_CASE_SAVESTATES;
			end=1;
		}
		else if (hitQ && right)
		{
			force_quit=1;
			mainMenu_case=MAIN_MENU_CASE_QUIT;
			end=1;
		}
		else if (hitQ)
		{
			mainMenu_case=MAIN_MENU_CASE_QUIT;
			end=1;
		}
		else if(hitN1)
		{
			current_drive=0;
			mainMenu_case=MAIN_MENU_CASE_LOAD;
			end=1;
		}
		else if(hitN2)
		{
			current_drive=1;
			mainMenu_case=MAIN_MENU_CASE_LOAD;
			end=1;
		}
		else if(hitN3)
		{
			current_drive=2;
			mainMenu_case=MAIN_MENU_CASE_LOAD;
			end=1;
		}
		else if(hitN4)
		{
			current_drive=3;
			mainMenu_case=MAIN_MENU_CASE_LOAD;
			end=1;
		}
		else if (up)
		{
			if(nr_drives<2 && c==4)
				c=0;
			else if(nr_drives<3 && c==4)
				c=1;
			else if(nr_drives<4 && c==4)
				c=2;
			else
				c--;
			if (c < 0) c = 18;
		}
		else if (down)
		{
			if(nr_drives<4 && c==2)
				c=4;
			else if(nr_drives<3 && c==1)
				c=4;
			else if(nr_drives<2 && c==0)
				c=4;
			else
				c=(c+1)%19;
		}

	/* New Menu
	0 = DF0:
	1 = DF1:
	2 = DF2:
	3 = DF3:
	4 = eject all drives
	5 = number of drives
	6 = preset system setup
	7 = display settings
	8 = sound
	9 = savestates
	10 = custom controls
	11 = more options
	12 = reset
	13 = load config
	14 = save config as...
	15 = delete config
	16 = save general config
	17 = save config current game
	18 = exit
	*/
		switch(c)
		{
			case 0:
				if (hit0)
				{
					current_drive=0;
					mainMenu_case=MAIN_MENU_CASE_LOAD;
					end=1;
				}
				break;
			case 1:
				if (hit0)
				{
					current_drive=1;
					mainMenu_case=MAIN_MENU_CASE_LOAD;
					end=1;
				}
				break;
			case 2:
				if (hit0)
				{
					current_drive=2;
					mainMenu_case=MAIN_MENU_CASE_LOAD;
					end=1;
				}
				break;
			case 3:
				if (hit0)
				{
					current_drive=3;
					mainMenu_case=MAIN_MENU_CASE_LOAD;
					end=1;
				}
				break;
			case 4:
				if (hit0)
				{
					strcpy(uae4all_image_file0, "");
					strcpy(uae4all_image_file1, "");
					strcpy(uae4all_image_file2, "");
					strcpy(uae4all_image_file3, "");
				}
				break;
			case 5:
				if (left)
				{
					if (nr_drives>1)
						nr_drives--;
					else
						nr_drives=4;
				}
				else if (right)
				{
					if (nr_drives<4)
						nr_drives++;
					else
						nr_drives=1;
				}	
				break;
			case 6:
				if (left)
				{
					if (mainMenu_system==0)
						mainMenu_system=1;
					else
						mainMenu_system=0;
					setSystem();
				}
				else if (right)
				{
					if (mainMenu_system<1)
						mainMenu_system=1;
					else
						mainMenu_system=0;
					setSystem();
				}
				break;
			case 7:
				if (hit0)
				{
					mainMenu_case=MAIN_MENU_CASE_MEMDISK;
					end=1;
				}
				break;
			case 8:
				if (hit0)
				{
					mainMenu_case=MAIN_MENU_CASE_DISPLAY;
					end=1;
				}
				break;
			case 9:
				if (hit0)
				{
					mainMenu_case=MAIN_MENU_CASE_SAVESTATES;
					end=1;
				}
				break;
			case 10:
				if (hit0)
				{
					mainMenu_case=MAIN_MENU_CASE_CONTROLS;
					end=1;
				}
				break;
			case 11:
				if (hit0)
				{
					mainMenu_case=MAIN_MENU_CASE_MISC;
					printf("Launch main menu MISC\n");
					end=1;
				}
				break;
			case 12:
				if (hit0)
				{
					mainMenu_case=MAIN_MENU_CASE_RESET;
					end=1;
				}
				break;
			case 13:
				if (hit0)
				{
					mainMenu_case=MAIN_MENU_CASE_LOAD_CONFIG;
					end=1;
				}
				break;
			case 14:
				if (hit0)
				{
					mainMenu_case=MAIN_MENU_CASE_SAVE;
					if (saveconfig(4))
						showWarning("Config saved.");
				}
				break;
			case 15:
				if (hit0)
				{
					mainMenu_case=MAIN_MENU_CASE_DELETE_CONFIG;
					end=1;
				}
				break;
			case 16:
				if (hit0)
				{
					mainMenu_case=MAIN_MENU_CASE_SAVE;
					saveconfig(1);
					showWarning("General config file saved.");
				}
				break;
			case 17:
				if (hit0)
				{
					mainMenu_case=MAIN_MENU_CASE_SAVE;
					if (saveconfig())
						showWarning("Config saved for this game.");
				}
				break;
			case 18:
				if (hit0 && right)
				{
					force_quit=1;
					mainMenu_case=MAIN_MENU_CASE_QUIT;
					end=1;
				}
				else if (hit0)
				{
					mainMenu_case=MAIN_MENU_CASE_QUIT;
					end=1;
				}
				break;

		}
		if (back_c>=0)
		{
			c=back_c;
			back_c=-1;
		}			
	}

	(*cp)=c;
	return end;
}

static void raise_mainMenu()
{	
/*
	text_draw_background();
	text_flip();
#if !defined(__PSP2__) && !defined(__SWITCH__)
	int i;
	for(i=0;i<10;i++)
	{
		text_draw_background();
		text_draw_window(40,(10-i)*24,260,200,text_str_title);
		text_flip();
	}
#endif*/
}

static void unraise_mainMenu()
{
/*#if !defined(__PSP2__) && !defined(__SWITCH__)
	int i;
	for(i=9;i>=0;i--)
	{
		text_draw_background();
		text_draw_window(40,(10-i)*24,260,200,text_str_title);
		text_flip();
	}
#endif
	text_draw_background();
	text_flip();*/
}

int run_mainMenu()
{
	static int c=0;
	int end;
	int old_sound_rate = sound_rate;
	int old_stereo = mainMenu_soundStereo;
	mainMenu_case=-1;
	init_text(0);
	
#if defined(__PSP2__) || defined(__SWITCH__)
	inside_menu = 1;
	SDL_Event event;
	while (SDL_PollEvent(&event) > 0);
#endif
   
	while(mainMenu_case<0)
	{
		raise_mainMenu();
		end=0;
		while(!end)
		{
			draw_mainMenu(c);
			end=key_mainMenu(&c);
		}
		unraise_mainMenu();
		switch(mainMenu_case)
		{
		case MAIN_MENU_CASE_LOAD:
		{
			int autostate_loaded = 0;
			if(run_menuLoad(currentDir, MENU_LOAD_FLOPPY) && current_drive==0)
			{
				// Check for disk-specific config
				char path[300];
				create_configfilename(path, uae4all_image_file0, 0);
				FILE *f=fopen(path,"rt");
				if(f)
				{
					// config file exists -> load
					fclose(f);
					loadconfig();
				}
				// Check for disk-specific Auto Savestate and load it automatically if possible
				if (emulating)
				{
					int old_saveMenu_n_savestate = saveMenu_n_savestate;
					saveMenu_n_savestate=11;
					make_savestate_filenames(savestate_filename,screenshot_filename);
					f=fopen(savestate_filename,"rb");
					if (f)
					{
						fclose(f);
						savestate_state=STATE_DORESTORE;
						autostate_loaded=1;
					} else 
					{
						saveMenu_n_savestate=old_saveMenu_n_savestate;
						make_savestate_filenames(savestate_filename,screenshot_filename);
					}
				}
			}
			if (autostate_loaded==1)
			{
				setCpuSpeed();
				mainMenu_case=1;
			} 
			else 
			{
				mainMenu_case=-1;
			}
			break;
		}
		case MAIN_MENU_CASE_MEMDISK:
			run_menuMemDisk();
			if (quit_pressed_in_submenu) //User quit menu while in sub-menu
			{
				if (emulating)
				{
					setCpuSpeed();
					mainMenu_case=1;
				}
				else
					mainMenu_case=-1;
				quit_pressed_in_submenu=0;
			}
			else
				mainMenu_case=-1;
			break;
		case MAIN_MENU_CASE_SAVESTATES:
			run_menuSavestates();
			if(savestate_state == STATE_DORESTORE || savestate_state == STATE_DOSAVE)
			{
				setCpuSpeed();
				mainMenu_case=1;
			}
			else if (quit_pressed_in_submenu) //User quit menu while in sub-menu
			{
				if (emulating)
				{
					setCpuSpeed();
					mainMenu_case=1;
				}
				else
					mainMenu_case=-1;
				quit_pressed_in_submenu=0;
			}
			else
				mainMenu_case=-1;
			break;
		case MAIN_MENU_CASE_EJECT:
			mainMenu_case=3;
			break;
		case MAIN_MENU_CASE_CANCEL:
			if (emulating)
			{
				setCpuSpeed();
				mainMenu_case=1;
			}
			else
				mainMenu_case=-1;
			break;
		case MAIN_MENU_CASE_RESET:
			setCpuSpeed();
			gp2xMouseEmuOn=0;
			gp2xButtonRemappingOn=0;
			mainMenu_drives=nr_drives;
			if (kickstart!=oldkickstart) 
			{
				oldkickstart=kickstart;
				snprintf(romfile, 256, "%s/kickstarts/%s",launchDir,kickstarts_rom_names[kickstart]);
				bReloadKickstart=1;
				uae4all_init_rom(romfile);
			}
			if (emulating)
			{
				mainMenu_case=2;	
				break;
			}
		case MAIN_MENU_CASE_RUN:
			setCpuSpeed();
			mainMenu_case=1;
			break;
		case MAIN_MENU_CASE_CONTROLS:
			{
				run_menuControls();
				if (quit_pressed_in_submenu) //User quit menu while in sub-menu
				{
					if (emulating)
					{
						setCpuSpeed();
						mainMenu_case=1;
					}
					else
						mainMenu_case=-1;
					quit_pressed_in_submenu=0;
				}
				else
					mainMenu_case=-1;
			}
			break;
		case MAIN_MENU_CASE_DISPLAY:
			{
				run_menuDisplay();
				if (quit_pressed_in_submenu) //User quit menu while in sub-menu
				{
					if (emulating)
					{
						setCpuSpeed();
						mainMenu_case=1;
					}
					else
						mainMenu_case=-1;
					quit_pressed_in_submenu=0;
				}
				else
					mainMenu_case=-1;
			}
			break;
		case MAIN_MENU_CASE_MISC:
			{
				run_menuMisc();
				if (quit_pressed_in_submenu) //User quit menu while in sub-menu
				{
					if (emulating)
					{
						setCpuSpeed();
						mainMenu_case=1;
					}
					else
						mainMenu_case=-1;
					quit_pressed_in_submenu=0;
				}
				else
					mainMenu_case=-1;
			}
			break;
		case MAIN_MENU_CASE_LOAD_CONFIG:
		{
			char path[300];
			snprintf(path, 300, "%s/conf", launchDir);

			if(run_menuLoad(path, MENU_LOAD_CONFIG))
				loadconfig(5);
			mainMenu_case=-1;
			break;
		}
		case MAIN_MENU_CASE_DELETE_CONFIG:
		{
			char path[300];
			snprintf(path, 300, "%s/conf", launchDir);

			if(run_menuLoad(path, MENU_LOAD_DELETE_CONFIG)) {
				FILE *f=fopen(config_load_filename,"rb");
				if (f) {
					fclose(f);
					if (remove(config_load_filename) == 0) {
						showWarning("Config File deleted");
					} else {
						showWarning("Config File does not exist.");
					}
				}
			}
			mainMenu_case=-1;
			break;
		}
		case MAIN_MENU_CASE_QUIT:
			if (gui_data.hdled == HDLED_WRITE && force_quit == 0) 
			{
				showWarning("Amiga is writing to HDF. Press PAD_right+L to force quit.");
				break;
		   }
			exit_safely(0);
			break;
		default:
			mainMenu_case=-1;
		}
	}

	if (sound_rate != old_sound_rate || mainMenu_soundStereo != old_stereo)
		init_sound();
	
	//See if new joysticks have been paired
	close_joystick();
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);	
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	init_joystick();
	
	update_display();

#if defined(__PSP2__) || defined(__SWITCH__)
	inside_menu = 0;
#endif
	return mainMenu_case;
}
