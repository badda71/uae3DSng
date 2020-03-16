#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"

#include "uae.h"
#include "options.h"
#include "menu.h"
#include "menu_config.h"
#include "sound.h"
#include "disk.h"
#include "memory-uae.h"
#include "custom.h"
#include "xwin.h"
#include "drawing.h"

#include <3ds.h>

extern int screenWidth;
extern int mainMenu_case;

extern int lastCpuSpeed;
extern int ntsc;

extern char launchDir[300];
extern char currentDir[300];

extern int displaying_menu;

extern void gp2x_stop_sound(void);

#ifdef __SWITCH__
extern void update_joycon_mode();
#endif

int saveAdfDir() {
    char path[300];
    snprintf(path, 300, "%s/conf/adfdir.conf", launchDir);
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    char buffer[310];
    snprintf((char *) buffer, 310, "path=%s\n", currentDir);
    fputs(buffer, f);
    fclose(f);
    return 1;
}

void extractFileName(char *str, char *buffer) {
    char *p = str + strlen(str) - 1;
    while (*p != '/')
        p--;
    p++;
    strcpy(buffer, p);
}

void stateFilenameToThumbFilename(char *src, char *dst) {
    char buffer[255] = "";
    extractFileName(src, buffer);
    char *p = buffer + strlen(buffer) - 4;
    *p = 0;
    p--;
    strcat(p, ".png");
    strcpy(dst, THUMB_PREFIX);
    strcat(dst, buffer);
}

void exit_safely(int quit_via_home) {
    saveAdfDir();	
    leave_program();

#ifdef __SWITCH__
    mainMenu_singleJoycons = 0;
    update_joycon_mode();
//    if (quit_via_home)
//        appletUnlockExit();
#endif
    exit(0);
}

void update_display() {
    char layersize[20];
    snprintf(layersize, 20, "%dx480", screenWidth);

    char bordercut[20];
    snprintf(bordercut, 20, "%d,%d,0,0", mainMenu_cutLeft, mainMenu_cutRight);

    if (prSDLScreen != NULL) {
        // clear old screen
        for (int i=0; i<4; i++)
		{
			SDL_FillRect(prSDLScreen,NULL,SDL_MapRGB(prSDLScreen->format, 0, 0, 0));
			SDL_Flip(prSDLScreen);
		}
		SDL_FreeSurface(prSDLScreen);
      prSDLScreen = NULL;
    }

	displaying_menu = 0;

    prSDLScreen = SDL_SetVideoMode(visibleAreaWidth, mainMenu_displayedLines, 16, SDL_CONSOLEBOTTOM);
log_citra("update_display: SDL_SetVideoMode(%i, %i, 16)\n", visibleAreaWidth, mainMenu_displayedLines);

    float sh;
    float sw;
    int x;
    int y;

    //is a shader active?
    if (0) // (mainMenu_shader != 0)
    {
    	sh = 544;
      if (mainMenu_displayHires)
      	sw = (0.5f*(float)visibleAreaWidth*((float)544/(float)mainMenu_displayedLines));
      else
      	sw = ((float)visibleAreaWidth*((float)544/(float)mainMenu_displayedLines));
    	x = (960 - sw) / 2;
    	y = (544 - sh) / 2;

    	//This requires a recent SDL-Vita branch SDL12 for example
    	//https://github.com/rsn8887/SDL-Vita/tree/SDL12
    	//to compile
   	//SDL_SetVideoModeScaling(x, y, sw, sh);
   	//SDL_SetVideoModeBilinear(1);
    }
    else //otherwise do regular integer 2* scaling without filtering to ensure good picture quality
    {
    	sh = (float) (2 * mainMenu_displayedLines);
    	if (mainMenu_displayHires)
    		sw = (float) (1 * visibleAreaWidth);
    	else
    		sw = (float) (2 * visibleAreaWidth);
    	x = (960 - sw) / 2;
      y = (544 - sh) / 2;
      //SDL_SetVideoModeScaling(x, y, sw, sh);
      //SDL_SetVideoModeBilinear(0);
	 }
//	 printf("update_display: SDL_SetVideoModeScaling(%i, %i, %i, %i)\n", x, y, (int)sw, (int)sh);

    //SDL_SetVideoModeSync(1);

    // clear new screen
    for (int i=0; i<4; i++)
	{
		SDL_FillRect(prSDLScreen,NULL,SDL_MapRGB(prSDLScreen->format, 0, 0, 0));
		SDL_Flip(prSDLScreen);
	}

    if (mainMenu_displayHires)
        InitDisplayArea(visibleAreaWidth >> 1);
    else
        InitDisplayArea(visibleAreaWidth);
}


static bool cpuSpeedChanged = false;

void setCpuSpeed() {
}
