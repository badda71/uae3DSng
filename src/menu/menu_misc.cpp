#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "menu.h"

#include <sys/stat.h>
#include <unistd.h>
#include<dirent.h>

#include "uae.h"
#include "options.h"
#include "sound.h"
#include <SDL/SDL.h>
#include "gp2x.h"
#include <SDL/SDL_ttf.h>
#include "custom.h"
#include "menu_config.h"
#include "keyboard.h"
#include "uibottom.h"

#include <3ds.h>

const char *text_str_misc_separator="----------------------------------------";
static const char *text_str_misc_title=    "Miscellanous";
static const char *text_str_0px="0px";
static const char *text_str_1px="1px";
static const char *text_str_3px="3px";
static const char *text_str_5px="5px";
static const char *text_str_8px="8px";
static const char *text_str_normal="normal";
static const char *text_str_short="short";
static const char *text_str_none="no";
static const char *text_str_mouse_multiplier="Mouse Speed";
int menuMisc = 0;

extern int kickstart;
extern int quit_pressed_in_submenu;
extern int emulating;

enum { 
	MENUMISC_RETURNMAIN = 0,
	MENUMISC_CPU,
	MENUMISC_CHIPSET,
	MENUMISC_KICKSTART,
	MENUMISC_CPUSPEED,
	MENUMISC_BLITTER,
	MENUMISC_SPRITECOLLISIONS,
	MENUMISC_JOYSTICK,
	MENUMISC_AUTOFIRERATE,
	MENUMISC_MOUSEEMULATION,
	MENUMISC_MOUSEMULTIPLIER,
	MENUMISC_END
};
	
static void draw_miscMenu(int c)
{
	static int b=0;
	int bb=(b%6)/3;
	char cpuSpeed[8];
	char tmpString[16];

	int menuWidth = 40;
	int menuHeight = 26;
	int leftMargin = ((50 - menuWidth) / 2)*8;
	int menuLine = ((31 - menuHeight) / 2)*8;

	int tabstop1 = leftMargin+14*8;
	int tabstop2 = leftMargin+16*8;
	int tabstop3 = leftMargin+18*8;
	int tabstop4 = leftMargin+20*8;
	int tabstop5 = leftMargin+22*8;
	int tabstop6 = leftMargin+24*8;
	int tabstop7 = leftMargin+26*8;
	int tabstop8 = leftMargin+28*8;
	int tabstop9 = leftMargin+30*8;

	text_draw_background();
	text_draw_window(leftMargin/8,menuLine/8,menuWidth,menuHeight,text_str_misc_title);

	// MENUMISC_RETURNMAIN
	if (menuMisc == MENUMISC_RETURNMAIN && bb)
		write_text_inv_pos(leftMargin, menuLine, "Return to main menu");
	else
		write_text_pos(leftMargin, menuLine, "Return to main menu");

	menuLine+=8;
	write_text_pos(leftMargin,menuLine,text_str_misc_separator);
	menuLine+=8;

	// MENUMISC_CPU
	write_text_pos(leftMargin,menuLine,"CPU");
	if ((mainMenu_CPU_model==0)&&((menuMisc!=MENUMISC_CPU)||(bb)))
	  write_text_inv_pos(tabstop2,menuLine,"68000");
	else
	  write_text_pos(tabstop2,menuLine,"68000");
	
	if ((mainMenu_CPU_model==1)&&((menuMisc!=MENUMISC_CPU)||(bb)))
	  write_text_inv_pos(tabstop5,menuLine,"68020");
	else
	  write_text_pos(tabstop5,menuLine,"68020");

	// MENUMISC_CHIPSET
	menuLine+=12;
	write_text_pos(leftMargin,menuLine,"Chipset");
	
	if (((mainMenu_chipset & 0xff)==0)&&((menuMisc!=MENUMISC_CHIPSET)||(bb)))
		write_text_inv_pos(tabstop2,menuLine,"OCS");
	else
		write_text_pos(tabstop2,menuLine,"OCS");
	
	if (((mainMenu_chipset & 0xff)==1)&&((menuMisc!=MENUMISC_CHIPSET)||(bb)))
		write_text_inv_pos(tabstop4,menuLine,"ECS");
	else
		write_text_pos(tabstop4,menuLine,"ECS");
	
	if (((mainMenu_chipset & 0xff)==2)&&((menuMisc!=MENUMISC_CHIPSET)||(bb)))
		write_text_inv_pos(tabstop6,menuLine,"AGA");
	else
		write_text_pos(tabstop6,menuLine,"AGA");
				
	// MENUMISC_KICKSTART
	menuLine+=12;
	write_text_pos(leftMargin,menuLine,"Kickstart");
	if ((kickstart==0)&&((menuMisc!=MENUMISC_KICKSTART)||(bb)))
		write_text_inv_pos(tabstop2,menuLine,"1.2");
	else
		write_text_pos(tabstop2,menuLine,"1.2");

	if ((kickstart==1)&&((menuMisc!=MENUMISC_KICKSTART)||(bb)))
		write_text_inv_pos(tabstop4,menuLine,"1.3");
	else
		write_text_pos(tabstop4,menuLine,"1.3");

	if ((kickstart==2)&&((menuMisc!=MENUMISC_KICKSTART)||(bb)))
		write_text_inv_pos(tabstop6,menuLine,"2.0");
	else
		write_text_pos(tabstop6,menuLine,"2.0");

	if ((kickstart==3)&&((menuMisc!=MENUMISC_KICKSTART)||(bb)))
		write_text_inv_pos(tabstop8,menuLine,"3.1");
	else
		write_text_pos(tabstop8,menuLine,"3.1");

	if ((kickstart==4)&&((menuMisc!=MENUMISC_KICKSTART)||(bb)))
		write_text_inv_pos(tabstop9+2*8,menuLine,"Custom");
	else
		write_text_pos(tabstop9+2*8,menuLine,"Custom");

	// MENUMISC_CPUSPEED
	menuLine+=12;
	write_text_pos(leftMargin,menuLine,"CPU Speed");
	if ((mainMenu_CPU_speed==0)&&((menuMisc!=MENUMISC_CPUSPEED)||(bb)))
		write_text_inv_pos(tabstop2,menuLine,"7MHz");
	else
		write_text_pos(tabstop2,menuLine,"7MHz");

	if ((mainMenu_CPU_speed==1)&&((menuMisc!=MENUMISC_CPUSPEED)||(bb)))
		write_text_inv_pos(tabstop5,menuLine,"14MHz");
	else
		write_text_pos(tabstop5,menuLine,"14MHz");

	if ((mainMenu_CPU_speed==2)&&((menuMisc!=MENUMISC_CPUSPEED)||(bb)))
		write_text_inv_pos(tabstop8,menuLine,"28MHz");
	else
		write_text_pos(tabstop8,menuLine,"28MHz");

	if ((mainMenu_CPU_speed==3)&&((menuMisc!=MENUMISC_CPUSPEED)||(bb)))
		write_text_inv_pos(tabstop9+4*8,menuLine,"56MHz");
	else
		write_text_pos(tabstop9+4*8,menuLine,"56MHz");

	// MENUMISC_BLITTER
	menuLine+=12;
	write_text_pos(leftMargin,menuLine,"Blitter");
	
	if (((mainMenu_chipset & 0xff00)!=0x100 && (mainMenu_chipset & 0xff00)!=0x200)&&((menuMisc!=MENUMISC_BLITTER)||(bb)))
		write_text_inv_pos(tabstop2,menuLine,"Normal");
	else
		write_text_pos(tabstop2,menuLine,"Normal");

	if (((mainMenu_chipset & 0xff00)==0x100)&&((menuMisc!=MENUMISC_BLITTER)||(bb)))
		write_text_inv_pos(tabstop5,menuLine,"Immediate");
	else
		write_text_pos(tabstop5,menuLine,"Immediate");

	if (((mainMenu_chipset & 0xff00)==0x200)&&((menuMisc!=MENUMISC_BLITTER)||(bb)))
		write_text_inv_pos(tabstop9+2*8,menuLine,"Improved");
	else
		write_text_pos(tabstop9+2*8,menuLine,"Improved");

	// MENUMISC_SPRITECOLLISIONS
	menuLine+=12;
	write_text_pos(leftMargin,menuLine,"Sprite Collisions");
	if ((mainMenu_spriteCollisions==0)&&((menuMisc!=MENUMISC_SPRITECOLLISIONS)||(bb)))
	  write_text_inv_pos(tabstop4,menuLine,"Off");
	else
	  write_text_pos(tabstop4,menuLine,"Off");
	
	if ((mainMenu_spriteCollisions==1)&&((menuMisc!=MENUMISC_SPRITECOLLISIONS)||(bb)))
	  write_text_inv_pos(tabstop6,menuLine,"On");
	else
	  write_text_pos(tabstop6,menuLine,"On");

	menuLine+=8;
	write_text_pos(leftMargin,menuLine,text_str_misc_separator);
	menuLine+=8;

  // MENUMISC_JOYSTICK
	write_text_pos(leftMargin,menuLine,"Joystick on");
  
	if ((mainMenu_joyPort==1)&&((menuMisc!=MENUMISC_JOYSTICK)||(bb)))
		write_text_inv_pos(tabstop2,menuLine,"Amiga Port0");
	else
		write_text_pos(tabstop2,menuLine,"Amiga Port0");

	if ((mainMenu_joyPort==2)&&((menuMisc!=MENUMISC_JOYSTICK)||(bb)))
		write_text_inv_pos(tabstop9-1*8,menuLine,"Amiga Port1");
	else
		write_text_pos(tabstop9-1*8,menuLine,"Amiga Port1");
  
	// MENUMISC_AUTOFIRERATE
	menuLine+=12;
	write_text_pos(leftMargin,menuLine,"Autofire Rate");

	if ((mainMenu_autofireRate==8)&&((menuMisc!=MENUMISC_AUTOFIRERATE)||(bb)))
		write_text_inv_pos(tabstop2,menuLine,"Light");
	else
		write_text_pos(tabstop2,menuLine,"Light");

	if ((mainMenu_autofireRate==4)&&((menuMisc!=MENUMISC_AUTOFIRERATE)||(bb)))
		write_text_inv_pos(tabstop6-2*8,menuLine,"Medium");
	else
		write_text_pos(tabstop6-2*8,menuLine,"Medium");

	if ((mainMenu_autofireRate==2)&&((menuMisc!=MENUMISC_AUTOFIRERATE)||(bb)))
		write_text_inv_pos(tabstop9-1*8,menuLine,"Heavy");
	else
		write_text_pos(tabstop9-1*8,menuLine,"Heavy");

	menuLine+=8;
	write_text_pos(leftMargin,menuLine,text_str_misc_separator);
	menuLine+=8;

	// MENUMISC_MOUSEEMULATION
	write_text_pos(leftMargin,menuLine,"C-Pad Mouse");	
	if ((mainMenu_mouseEmulation==0) && ((menuMisc!=MENUMISC_MOUSEEMULATION)||(bb)))
		write_text_inv_pos(tabstop2,menuLine,"Off");
	else
		write_text_pos(tabstop2,menuLine,"Off");
	if ((mainMenu_mouseEmulation==1) && ((menuMisc!=MENUMISC_MOUSEEMULATION)||(bb)))
		write_text_inv_pos(tabstop4,menuLine,"On");
	else
		write_text_pos(tabstop4,menuLine,"On");	

	// MENUMISC_MOUSEMULTIPLIER
	menuLine+=12;
	write_text_pos(leftMargin,menuLine,text_str_mouse_multiplier);

	snprintf((char*)cpuSpeed, 8, "%4.2f", mainMenu_mouseMultiplier/100.0f);
	if ((menuMisc!=MENUMISC_MOUSEMULTIPLIER)||(bb))
		write_text_inv_pos(tabstop2,menuLine,cpuSpeed);
	else
		write_text_pos(tabstop2,menuLine,cpuSpeed);

	text_flip();
	b++;
}

static int key_miscMenu(int *c)
{
	int end=0;
	int left=0, right=0, up=0, down=0, hit0=0, hit1=0, hit2=0;
	SDL_Event event;

	while (SDL_PollEvent(&event) > 0)
	{
		if (uib_handle_event(&event)) continue;
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
			if (menuMisc==0) menuMisc=MENUMISC_END-1;
			else menuMisc--;
		}
		else if (down)
		{
			if (menuMisc==MENUMISC_END-1) menuMisc=0;
			else menuMisc++;
		}
		switch (menuMisc)
		{
			case MENUMISC_RETURNMAIN:
				if (hit0)
					end = 1;
				break;
			case MENUMISC_CPU:
				if (left)
				{
				    if (mainMenu_CPU_model > 0)
				       mainMenu_CPU_model--;
				    else
				       mainMenu_CPU_model=1;
				}
				else if (right)
				{
				    if (mainMenu_CPU_model < 1)
				       mainMenu_CPU_model++;
				    else
				       mainMenu_CPU_model=0;
				}
				UpdateCPUModelSettings();
				break;
			case MENUMISC_CHIPSET:
				if (left)
				{
					switch (mainMenu_chipset & 0xff) //low is chipset, high is blitter
					{
						case 1:
							mainMenu_chipset=(mainMenu_chipset & 0xff00) | 0;
							break;
						case 2:
							mainMenu_chipset=(mainMenu_chipset & 0xff00) | 1;
							break;
						case 0:
							mainMenu_chipset=(mainMenu_chipset & 0xff00) | 2;
							break;
						default:
							mainMenu_chipset=(mainMenu_chipset & 0xff00) | 0;
							break;
					}
				}
				else if (right)
				{
					switch (mainMenu_chipset & 0xff) 
					{
						case 0:
							mainMenu_chipset=(mainMenu_chipset & 0xff00) | 1;
							break;
						case 1:
							mainMenu_chipset=(mainMenu_chipset & 0xff00) | 2;
							break;
						case 2:
							mainMenu_chipset=(mainMenu_chipset & 0xff00) | 0;
							break;
						default:
							mainMenu_chipset=(mainMenu_chipset & 0xff00) | 0;
							break;
					}
				}
				UpdateChipsetSettings();
				break;
			
			case MENUMISC_BLITTER:
				if (left)
				{
					switch (mainMenu_chipset & 0xff00) //low is chipset, high is blitter
					{
						case 0x200:
							mainMenu_chipset=(mainMenu_chipset & 0xff) | 0x100;
							break;
						case 0x100:
							mainMenu_chipset=(mainMenu_chipset & 0xff) | 0;
							break;
						case 0:
							mainMenu_chipset=(mainMenu_chipset & 0xff) | 0x200;
							break;
						default:
							mainMenu_chipset=(mainMenu_chipset & 0xff) | 0;
							break;
					}
				}
				else if (right)
				{
					switch (mainMenu_chipset & 0xff00)
					{
						case 0x200:
							mainMenu_chipset=(mainMenu_chipset & 0xff) | 0;
							break;
						case 0x100:
							mainMenu_chipset=(mainMenu_chipset & 0xff) | 0x200;
							break;
						case 0:
							mainMenu_chipset=(mainMenu_chipset & 0xff) | 0x100;
							break;
						default:
							mainMenu_chipset=(mainMenu_chipset & 0xff) | 0;
							break;
					}
				}
				UpdateChipsetSettings();
				break;
				
			case MENUMISC_SPRITECOLLISIONS:
				if (left||right)
					mainMenu_spriteCollisions = !mainMenu_spriteCollisions;
			   break;
		
			case MENUMISC_KICKSTART:
				if (left)
				{
					if (kickstart>0)
						kickstart--;
					else
						kickstart=4;
				}
				else if (right)
				{
					if (kickstart<4)
						kickstart++;
					else
						kickstart=0;
				}
				break;
			case MENUMISC_CPUSPEED:
				if (left)
				{
					if (mainMenu_CPU_speed>0)
						mainMenu_CPU_speed--;
					else
						mainMenu_CPU_speed=3;
				}
				else if (right)
				{
					if (mainMenu_CPU_speed<3)
						mainMenu_CPU_speed++;
					else
						mainMenu_CPU_speed=0;
				}
				break;
			case MENUMISC_JOYSTICK:
				if (left)
				{
					if (mainMenu_joyPort>1)
						mainMenu_joyPort--;
					else
						mainMenu_joyPort=2;
				}
				else if (right)
				{
					if (mainMenu_joyPort<2)
						mainMenu_joyPort++;
					else
						mainMenu_joyPort=1;
				}
				break;  
			case MENUMISC_AUTOFIRERATE:
				if(left)
				{
					if(mainMenu_autofireRate==2)
						mainMenu_autofireRate=4;
					else if(mainMenu_autofireRate==4)
						mainMenu_autofireRate=8;
					else
						mainMenu_autofireRate=2;
				}
				else if (right)
				{
					if(mainMenu_autofireRate==2)
						mainMenu_autofireRate=8;
					else if(mainMenu_autofireRate==8)
						mainMenu_autofireRate=4;
					else
						mainMenu_autofireRate=2;
				}
 				break;
			case MENUMISC_MOUSEMULTIPLIER:
				if (left)
				{
					if (mainMenu_mouseMultiplier <= 50)
						mainMenu_mouseMultiplier = 25;
					else
						mainMenu_mouseMultiplier -= 25;
				}
				else if (right)
				{
					if (mainMenu_mouseMultiplier >= 375)
						mainMenu_mouseMultiplier = 400;
					else
						mainMenu_mouseMultiplier += 25;
				}
				break;
			case MENUMISC_MOUSEEMULATION:
				if ((left)||(right))
					mainMenu_mouseEmulation = !mainMenu_mouseEmulation;
				break;
		}
	}

	return end;
}

static void raise_miscMenu()
{
	int i;

	text_draw_background();
	text_flip();
#if !defined(__PSP2__) && !defined(__SWITCH__)
	for(i=0;i<10;i++)
	{
		text_draw_background();
		text_draw_window(80-64,(10-i)*24,160+64+64,220,text_str_misc_title);
		text_flip();
	}
#endif
}

static void unraise_miscMenu()
{
	int i;
#if !defined(__PSP2__) && !defined(__SWITCH__)
	for(i=9;i>=0;i--)
	{
		text_draw_background();
		text_draw_window(80-64,(10-i)*24,160+64+64,220,text_str_misc_title);
		text_flip();
	}
#endif
	text_draw_background();
	text_flip();
}

int run_menuMisc()
{
	SDL_Event event;
	SDL_Delay(150);
	while(SDL_PollEvent(&event))
		SDL_Delay(10);
	int end=0, c=0;
	raise_miscMenu();
	while(!end)
	{
		draw_miscMenu(c);
		end=key_miscMenu(&c);
	}
	unraise_miscMenu();
	return end;
}
