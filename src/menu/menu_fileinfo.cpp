#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "menu.h"
#include<SDL.h>
#include <sys/stat.h>
#include <unistd.h>
#include<dirent.h>
#include "uae.h"
#include "options.h"
#include "sound.h"
#include "gp2x.h"
#include "uibottom.h"

static const char *text_str_fileinfo_title=    "File info";
char* fileInfo_fileName;

static void draw_fileinfoMenu(int c)
{
	int menuWidth = 35;
	int menuHeight = 4 + strlen(fileInfo_fileName) / menuWidth;
	
	int leftMargin = ((50 - menuWidth) / 2)*8;
	int menuLine = ((31 - menuHeight) / 2)*8;

	text_draw_background();
	text_draw_window(leftMargin/8,menuLine/8,menuWidth,menuHeight,text_str_fileinfo_title);

	write_text_pos(leftMargin, menuLine, "File info:");
	menuLine+=8;
	write_text_pos(leftMargin, menuLine, "----------");
	menuLine+=12;

	// now wrap the filename if necessary (at 35)
	int i = 0;
	char line [menuWidth+1];
	
	for (i = 0; i < strlen(fileInfo_fileName); i+=menuWidth)
	{
		strncpy(line, fileInfo_fileName + i, menuWidth);
		line[menuWidth] = '\0';
		write_text_pos(leftMargin, menuLine, line);
		menuLine+=8;
	}
	
	text_flip();
}

static int key_fileinfoMenu(int *c)
{
	int end=0;
	int left=0, right=0, up=0, down=0, hit0=0, hit1=0;
	SDL_Event event;

	while (SDL_PollEvent(&event) > 0)
	{
		if (uib_handle_event(&event)) continue;
		if (event.type == SDL_QUIT)
			end=-1;
		else if (event.type == SDL_KEYUP )
			end=-1;
		else if (event.type == SDL_JOYBUTTONUP )
			end=-1;
	}
	return end;
}

static void raise_fileinfoMenu()
{
	int i;

	text_draw_background();
	text_flip();
#if !defined(__PSP2__) && !defined(__SWITCH__)
	for(i=0;i<10;i++)
	{
		text_draw_background();
		text_draw_window(80-64,(10-i)*24,160+64+64,220,text_str_fileinfo_title);
		text_flip();
	}
#endif
}

static void unraise_fileinfoMenu()
{
	int i;

#if !defined(__PSP2__) && !defined(__SWITCH__)
	for(i=9;i>=0;i--)
	{
		text_draw_background();
		text_draw_window(80-64,(10-i)*24,160+64+64,220,text_str_fileinfo_title);
		text_flip();
	}
#endif
	text_draw_background();
	text_flip();
}

int run_menuFileinfo(char* fileName)
{
	int end=0,c=0;

	fileInfo_fileName = fileName;

	while(!end)
	{
		draw_fileinfoMenu(c);
		end=key_fileinfoMenu(&c);
	}

	return end;
}
