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
    exit(0);
}

extern "C" void N3DS_SetYOffset(int);
extern "C" void N3DS_SetXOffset(int);
extern "C" void N3DS_SetScalingDirect(float, float, int);

void update_display() {
/*
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
*/
	displaying_menu = 0;

    prSDLScreen = SDL_SetVideoMode(visibleAreaWidth, mainMenu_displayedLines, 16, SDL_HWSURFACE | mainMenu_scaling);
log_citra("update_display: SDL_SetVideoMode(%i, %i, 16)\n", visibleAreaWidth, mainMenu_displayedLines);
	N3DS_SetYOffset(mainMenu_yoffset);
	N3DS_SetXOffset(mainMenu_xoffset);
	if (mainMenu_scaling == 0) {
		N3DS_SetScalingDirect((float)mainMenu_scalingFac/(float)100, (float)mainMenu_scalingFac/(float)100, 1);
	}

    // clear new screen
	SDL_FillRect(prSDLScreen,NULL,SDL_MapRGB(prSDLScreen->format, 0, 0, 0));
	SDL_Flip(prSDLScreen);

    if (mainMenu_displayHires)
        InitDisplayArea(visibleAreaWidth >> 1);
    else
        InitDisplayArea(visibleAreaWidth);
}


static bool cpuSpeedChanged = false;

void setCpuSpeed() {
}
