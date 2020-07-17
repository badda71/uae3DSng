#define _MENU_CONFIG_CPP

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "menu.h"
#include "menu_config.h"
#include "autoconf.h"
#include "options.h"
#include "gui.h"
#include "sound.h"
#include "custom.h"
#include "gp2x.h"
#include "cfgfile.h"

extern int kickstart;
extern int blitter_in_partial_mode;
extern int sound_rate;
extern int screenWidth;
extern int moveX;
extern int moveY;


#if !( defined(PANDORA) )
extern int timeslice_mode;
#endif
extern char launchDir[300];
extern char currentDir[300];
extern int nr_drives;
static char config_filename[255] = "uae4all.cfg";

extern void extractFileName(char * str,char *buffer);

int mainMenu_max_tap_time=250;
int mainMenu_click_time=100;
int mainMenu_single_tap_timeout=250;
int mainMenu_max_double_tap_time=250;
int mainMenu_locked_drag_timeout=5000;
int mainMenu_tap_and_drag_gesture=1;
int mainMenu_locked_drags=0;

char filename0[256] = "";
char filename1[256] = "";
char filename2[256] = "";
char filename3[256] = "";

char config_load_filename[300] = "";

int mainMenu_chipMemory = DEFAULT_CHIPMEM_SELECT;
int mainMenu_slowMemory = 0;	/* off */
int mainMenu_fastMemory = 0;	/* off */

int mainMenu_bootHD = DEFAULT_ENABLE_HD;
int mainMenu_filesysUnits = 0;

int mainMenu_drives = DEFAULT_DRIVES;
int mainMenu_floppyspeed = 100;
int mainMenu_CPU_model = DEFAULT_CPU_MODEL;
int mainMenu_chipset = DEFAULT_CHIPSET_SELECT;
int mainMenu_spriteCollisions = DEFAULT_SPRITECOLLISIONS;
int mainMenu_sound = DEFAULT_SOUND;
int mainMenu_soundStereo = 1; // Default is stereo
int mainMenu_soundStereoSep = 3; // Default is 100% stereo separation
int mainMenu_CPU_speed = 0;

int mainMenu_cpuSpeed = 600;

int mainMenu_joyPort = 2; // Default to port 1 on Vita because mouse is always on.
int mainMenu_autofireRate = 8;
int mainMenu_mouseMultiplier = DEFAULT_MOUSEMULTIPLIER;
int mainMenu_mouseEmulation = 1;

int mainMenu_displayedLines = 240;
int mainMenu_displayHires = 0;
char presetMode[20] = "320x240 upscaled";
int presetModeId = 2;
int mainMenu_cutLeft = 0;
int mainMenu_cutRight = 0;
int mainMenu_ntsc = DEFAULT_NTSC;
int mainMenu_frameskip = 0;
int visibleAreaWidth = 400;


int saveMenu_n_savestate = 0;

// The following params in use, but can't be changed with gui
int mainMenu_autosave = DEFAULT_AUTOSAVE;

// The following params not in use, but stored to write them back to the config file
int gp2xClockSpeed = -1;
int mainMenu_scanlines = 0;
int mainMenu_enableScreenshots = DEFAULT_ENABLESCREENSHOTS;
int mainMenu_enableScripts = DEFAULT_ENABLESCRIPTS;

#if defined(AROS)
int mainMenu_vsync=0;
#endif
char custom_kickrom[256] = "./kick.rom\0";
int mainMenu_useSavesFolder = DEFAULT_USESAVESFOLDER;

void SetDefaultMenuSettings(int general)
{
    mainMenu_chipMemory = DEFAULT_CHIPMEM_SELECT;
    mainMenu_slowMemory = 0;	/* off */
    mainMenu_fastMemory = 0;	/* off */
    UpdateMemorySettings();

	if(general < 2) 
	{
		mainMenu_bootHD=DEFAULT_ENABLE_HD;
		kill_hd_configs();
    }

    if(general > 0) {
		  //reset floppies
        uae4all_image_file0[0] = '\0';
        uae4all_image_file1[0] = '\0';
        uae4all_image_file2[0] = '\0';
        uae4all_image_file3[0] = '\0';

        mainMenu_drives = DEFAULT_DRIVES;
    }
    mainMenu_floppyspeed = 100;

    mainMenu_CPU_model = DEFAULT_CPU_MODEL;
    mainMenu_chipset = DEFAULT_CHIPSET_SELECT;
    UpdateChipsetSettings();
    mainMenu_spriteCollisions = DEFAULT_SPRITECOLLISIONS;
    kickstart = DEFAULT_KICKSTART;
    mainMenu_sound = DEFAULT_SOUND;
    sound_rate = DEFAULT_SOUND_FREQ;
    mainMenu_soundStereo = 1; // Default is stereo
    mainMenu_soundStereoSep = 3; // Default is 100% stereo separation
    mainMenu_CPU_speed = 0;

    mainMenu_cpuSpeed = 600;
    mainMenu_joyPort = 2; // Default to port 1 on Vita because mouse is always on.
    mainMenu_autofireRate = 8;
    mainMenu_mouseMultiplier = DEFAULT_MOUSEMULTIPLIER;
    mainMenu_mouseEmulation = 1;
    SetPresetMode(2);
    moveX = 0;
    moveY = 16;
    mainMenu_cutLeft = 0;
    mainMenu_cutRight = 0;
    mainMenu_ntsc = DEFAULT_NTSC;
    mainMenu_frameskip = 0;

    // The following params can't be changed in gui
    mainMenu_autosave = DEFAULT_AUTOSAVE;

    gp2xClockSpeed = -1;
    mainMenu_scanlines = 0;
    mainMenu_enableScreenshots = DEFAULT_ENABLESCREENSHOTS;
    mainMenu_enableScripts = DEFAULT_ENABLESCRIPTS;
    mainMenu_useSavesFolder = DEFAULT_USESAVESFOLDER;
}


void UpdateCPUModelSettings()
{
    switch (mainMenu_CPU_model) {
    case 1:
        changed_prefs.cpu_level = M68020;
        break;
    default:
        changed_prefs.cpu_level = M68000;
        break;
    }
}


void UpdateMemorySettings()
{
    prefs_chipmem_size = 0x000080000 << mainMenu_chipMemory;

    /* >2MB chip memory => 0 fast memory */
    if ((mainMenu_chipMemory > 2) && (mainMenu_fastMemory > 0)) {
        mainMenu_fastMemory = 0;
        changed_prefs.fastmem_size = 0;
    }

    switch (mainMenu_slowMemory) {
    case 1:
    case 2:
        prefs_bogomem_size = 0x00080000 << (mainMenu_slowMemory - 1);
        break;
    case 3:
        prefs_bogomem_size = 0x00180000;	/* 1.5M */
        break;
    case 4:
        prefs_bogomem_size = 0x001C0000;	/* 1.8M */
        break;
    default:
        prefs_bogomem_size = 0;
    }

    switch (mainMenu_fastMemory) {
    case 0:
        changed_prefs.fastmem_size = 0;
        break;
    default:
        changed_prefs.fastmem_size = 0x00080000 << mainMenu_fastMemory;
    }

}


void UpdateChipsetSettings()
{
    switch (mainMenu_chipset & 0xff) {
    case 1:
        changed_prefs.chipset_mask = CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE;
        break;
    case 2:
        changed_prefs.chipset_mask = CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE | CSMASK_AGA;
        break;
    default:
        changed_prefs.chipset_mask = CSMASK_ECS_AGNUS;
        break;
    }
    switch (mainMenu_chipset & 0xff00) {
    case 0x100:
    //Immediate Blitter
        changed_prefs.immediate_blits = true;
        blitter_in_partial_mode = 0;
        break;
    //Improved Blitter
    case 0x200:
        changed_prefs.immediate_blits = false;
        blitter_in_partial_mode = 1;
        break;
    //Normal Blitter
    default:
        changed_prefs.immediate_blits = false;
        blitter_in_partial_mode = 0;
        break;
    }
}


void SetPresetMode(int mode)
{
    presetModeId = mode;

    switch(mode) {
    case 0:
        mainMenu_displayedLines = 200;
        screenWidth = 768;
        strcpy(presetMode, "320x200 upscaled");
        break;

    case 1:
        mainMenu_displayedLines = 216;
        screenWidth = 716;
        strcpy(presetMode, "320x216 upscaled");
        break;

    case 2:
        mainMenu_displayedLines = 240;
        screenWidth = 640;
        strcpy(presetMode, "320x240 upscaled");
        break;

    case 3:
        mainMenu_displayedLines = 256;
        screenWidth = 600;
        strcpy(presetMode, "320x256 upscaled");
        break;

    case 4:
        mainMenu_displayedLines = 262;
        screenWidth = 588;
        strcpy(presetMode, "320x262 upscaled");
        break;

    case 5:
        mainMenu_displayedLines = 270;
        screenWidth = 570;
        strcpy(presetMode, "320x270 upscaled");
        break;

    case 6:
        mainMenu_displayedLines = 200;
        screenWidth = 640;
        strcpy(presetMode, "320x200 NTSC");
        break;

    case 7:
        mainMenu_displayedLines = 200;
        screenWidth = 800;
        strcpy(presetMode, "320x200 fullscreen");
        break;

    case 10:
        mainMenu_displayedLines = 200;
        screenWidth = 768;
        strcpy(presetMode, "640x200 upscaled");
        break;

    case 11:
        mainMenu_displayedLines = 216;
        screenWidth = 716;
        strcpy(presetMode, "640x216 upscaled");
        break;

    case 12:
        mainMenu_displayedLines = 240;
        screenWidth = 640;
        strcpy(presetMode, "640x240 upscaled");
        break;

    case 13:
        mainMenu_displayedLines = 256;
        screenWidth = 600;
        strcpy(presetMode, "640x256 upscaled");
        break;

    case 14:
        mainMenu_displayedLines = 262;
        screenWidth = 588;
        strcpy(presetMode, "640x262 upscaled");
        break;

    case 15:
        mainMenu_displayedLines = 270;
        screenWidth = 570;
        strcpy(presetMode, "640x270 upscaled");
        break;

    case 16:
        mainMenu_displayedLines = 200;
        screenWidth = 640;
        strcpy(presetMode, "640x200 NTSC");
        break;

    case 17:
        mainMenu_displayedLines = 200;
        screenWidth = 800;
        strcpy(presetMode, "640x200 fullscreen");
        break;

    case 20:
        mainMenu_displayedLines = 200;
        screenWidth = 800;
        strcpy(presetMode, "352x200 upscaled");
        break;

    case 21:
        mainMenu_displayedLines = 216;
        screenWidth = 784;
        strcpy(presetMode, "352x216 upscaled");
        break;

    case 22:
        mainMenu_displayedLines = 240;
        screenWidth = 704;
        strcpy(presetMode, "352x240 upscaled");
        break;

    case 23:
        mainMenu_displayedLines = 256;
        screenWidth = 660;
        strcpy(presetMode, "352x256 upscaled");
        break;

    case 24:
        mainMenu_displayedLines = 262;
        screenWidth = 640;
        strcpy(presetMode, "352x262 upscaled");
        break;

    case 25:
        mainMenu_displayedLines = 270;
        screenWidth = 624;
        strcpy(presetMode, "352x270 upscaled");
        break;

    case 26:
        mainMenu_displayedLines = 200;
        screenWidth = 704;
        strcpy(presetMode, "352x200 NTSC");
        break;

    case 27:
        mainMenu_displayedLines = 200;
        screenWidth = 800;
        strcpy(presetMode, "352x200 fullscreen");
        break;

    case 30:
        mainMenu_displayedLines = 200;
        screenWidth = 800;
        strcpy(presetMode, "704x200 upscaled");
        break;

    case 31:
        mainMenu_displayedLines = 216;
        screenWidth = 784;
        strcpy(presetMode, "704x216 upscaled");
        break;

    case 32:
        mainMenu_displayedLines = 240;
        screenWidth = 704;
        strcpy(presetMode, "704x240 upscaled");
        break;

    case 33:
        mainMenu_displayedLines = 256;
        screenWidth = 660;
        strcpy(presetMode, "704x256 upscaled");
        break;

    case 34:
        mainMenu_displayedLines = 262;
        screenWidth = 640;
        strcpy(presetMode, "704x262 upscaled");
        break;

    case 35:
        mainMenu_displayedLines = 270;
        screenWidth = 624;
        strcpy(presetMode, "704x270 upscaled");
        break;

    case 36:
        mainMenu_displayedLines = 200;
        screenWidth = 704;
        strcpy(presetMode, "704x200 NTSC");
        break;

    case 37:
        mainMenu_displayedLines = 200;
        screenWidth = 800;
        strcpy(presetMode, "704x200 fullscreen");
        break;

    case 40:
        mainMenu_displayedLines = 200;
        screenWidth = 800;
        strcpy(presetMode, "384x200 upscaled");
        break;

    case 41:
        mainMenu_displayedLines = 216;
        screenWidth = 800;
        strcpy(presetMode, "384x216 upscaled");
        break;

    case 42:
        mainMenu_displayedLines = 240;
        screenWidth = 768;
        strcpy(presetMode, "384x240 upscaled");
        break;

    case 43:
        mainMenu_displayedLines = 256;
        screenWidth = 720;
        strcpy(presetMode, "384x256 upscaled");
        break;

    case 44:
        mainMenu_displayedLines = 262;
        screenWidth = 704;
        strcpy(presetMode, "384x262 upscaled");
        break;

    case 45:
        mainMenu_displayedLines = 270;
        screenWidth = 684;
        strcpy(presetMode, "384x270 upscaled");
        break;

    case 46:
        mainMenu_displayedLines = 200;
        screenWidth = 800;
        strcpy(presetMode, "384x200 NTSC");
        break;

    case 47:
        mainMenu_displayedLines = 200;
        screenWidth = 800;
        strcpy(presetMode, "384x200 fullscreen");
        break;

    case 50:
        mainMenu_displayedLines = 200;
        screenWidth = 800;
        strcpy(presetMode, "768x200 upscaled");
        break;

    case 51:
        mainMenu_displayedLines = 216;
        screenWidth = 800;
        strcpy(presetMode, "768x216 upscaled");
        break;

    case 52:
        mainMenu_displayedLines = 240;
        screenWidth = 768;
        strcpy(presetMode, "768x240 upscaled");
        break;

    case 53:
        mainMenu_displayedLines = 256;
        screenWidth = 720;
        strcpy(presetMode, "768x256 upscaled");
        break;

    case 54:
        mainMenu_displayedLines = 262;
        screenWidth = 704;
        strcpy(presetMode, "768x262 upscaled");
        break;

    case 55:
        mainMenu_displayedLines = 270;
        screenWidth = 684;
        strcpy(presetMode, "768x270 upscaled");
        break;

    case 56:
        mainMenu_displayedLines = 200;
        screenWidth = 800;
        strcpy(presetMode, "768x200 NTSC");
        break;

    case 57:
        mainMenu_displayedLines = 200;
        screenWidth = 800;
        strcpy(presetMode, "768x200 fullscreen");
        break;

    default:
        mainMenu_displayedLines = 240;
        screenWidth = 640;
        strcpy(presetMode, "320x240 upscaled");
        presetModeId = 2;
        break;
    }

    switch(presetModeId / 10) {
    case 0:
        mainMenu_displayHires = 0;
        visibleAreaWidth = 320;
        break;

    case 1:
        mainMenu_displayHires = 1;
        visibleAreaWidth = 640;
        break;

    case 2:
        mainMenu_displayHires = 0;
        visibleAreaWidth = 352;
        break;

    case 3:
        mainMenu_displayHires = 1;
        visibleAreaWidth = 704;
        break;

    case 4:
        mainMenu_displayHires = 0;
        visibleAreaWidth = 384;
        break;

    case 5:
        mainMenu_displayHires = 1;
        visibleAreaWidth = 768;
        break;
    }
}

void kill_hd_configs()
{
	 //properly close all open hdf and hd dirs
	for (int i = 0; i < mainMenu_filesysUnits; i++) {
        kill_filesys_unit(currprefs.mountinfo, 0);
    }
    mainMenu_filesysUnits = 0;
}

void reset_hdConf()
{
	 kill_hd_configs();
    /* Reset HD config */

    switch (mainMenu_bootHD) {
    case 0:
        // nothing to do, already killed above
        break;
    case 1:
        if (uae4all_hard_dir[0] != '\0') {
            parse_filesys_spec(0, uae4all_hard_dir);
            mainMenu_filesysUnits++;
        }
        if (uae4all_hard_file0[0] != '\0') {
            parse_hardfile_spec(uae4all_hard_file0);
            mainMenu_filesysUnits++;
        }
        if (uae4all_hard_file1[0] != '\0') {
            parse_hardfile_spec(uae4all_hard_file1);
            mainMenu_filesysUnits++;
        }
        if (uae4all_hard_file2[0] != '\0') {
            parse_hardfile_spec(uae4all_hard_file2);
            mainMenu_filesysUnits++;
        }
        if (uae4all_hard_file3[0] != '\0') {
            parse_hardfile_spec(uae4all_hard_file3);
            mainMenu_filesysUnits++;
        }
        break;
    case 2:
        if (uae4all_hard_file0[0] != '\0') {
            parse_hardfile_spec(uae4all_hard_file0);
            mainMenu_filesysUnits++;
        }
        if (uae4all_hard_file1[0] != '\0') {
            parse_hardfile_spec(uae4all_hard_file1);
            mainMenu_filesysUnits++;
        }
        if (uae4all_hard_file2[0] != '\0') {
            parse_hardfile_spec(uae4all_hard_file2);
            mainMenu_filesysUnits++;
        }
        if (uae4all_hard_file3[0] != '\0') {
            parse_hardfile_spec(uae4all_hard_file3);
            mainMenu_filesysUnits++;
        }
        if (uae4all_hard_dir[0] != '\0') {
            parse_filesys_spec(0, uae4all_hard_dir);
            mainMenu_filesysUnits++;
        }
        break;
    }
}


static void replace(char * str,char replace, char toreplace)
{
    while(*str) {
        if (*str==toreplace) *str=replace;
        str++;
    }
}


int create_configfilename(char *dest, char *basename, int fromDir)
{
    char *p;
    p = basename + strlen(basename) - 1;
    while (*p != '/')
        p--;
    p++;
    if(fromDir == 0) {
        int len = strlen(p) + 1;
        char filename[len];
        strcpy(filename, p);
        char *pch = &filename[len];
        while (pch != filename && *pch != '.')
            pch--;
        if (pch) {
            *pch='\0';
            snprintf(dest, 300, "%s/conf/%s.conf", launchDir, filename);
            return 1;
        }
    } else {
        snprintf(dest, 300, "%s/conf/%s.conf", launchDir, p);
        return 1;
    }

    return 0;
}


int saveconfig(int general)
{
    char path[300];
    switch(general) {
    case 0:
        if (!uae4all_image_file0[0])
            return 0;
        if(!create_configfilename(path, uae4all_image_file0, 0))
            return 0;
        break;
    case 1:
        snprintf(path, 300, "%s/conf/uaeconfig.conf", launchDir);
        break;
    case 2:
        path[0] = '\0';
        if(mainMenu_bootHD == 1 && uae4all_hard_dir[0] != '\0')
            create_configfilename(path, uae4all_hard_dir, 1);
        if(mainMenu_bootHD == 2 && uae4all_hard_file0[0] != '\0')
            create_configfilename(path, uae4all_hard_file0, 0);
        if(path[0] == '\0')
            return 0;
        break;
    case 3:
        snprintf(path, 300, config_filename, launchDir);
        break;
    }


    FILE *f=fopen(path,"w");
    if (!f) return 0;
    char buffer[255];

    snprintf((char*)buffer, 255, "kickstart=%d\n",kickstart);
    fputs(buffer,f);
#if defined(PANDORA)
    snprintf((char*)buffer, 255, "scaling=%d\n",0);
#else
    snprintf((char*)buffer, 255, "scaling=%d\n",mainMenu_enableHWscaling);
#endif
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "mousemultiplier=%d\n",mainMenu_mouseMultiplier);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "mouseemulation=%d\n",mainMenu_mouseEmulation);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "frameskip=%d\n",mainMenu_frameskip);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "sound=%d\n",mainMenu_sound + mainMenu_soundStereo * 10);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "soundstereosep=%d\n",mainMenu_soundStereoSep);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "soundrate=%d\n",sound_rate);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "autosave=%d\n",mainMenu_autosave);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "gp2xclock=%d\n",gp2xClockSpeed);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "joyport=%d\n", mainMenu_joyPort);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "autofireRate=%d\n",mainMenu_autofireRate);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "scanlines=%d\n",mainMenu_scanlines);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "enableScreenshots=%d\n",mainMenu_enableScreenshots);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "floppyspeed=%d\n",mainMenu_floppyspeed);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "drives=%d\n",nr_drives);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "videomode=%d\n",mainMenu_ntsc);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "mainMenu_cpuSpeed=%d\n",mainMenu_cpuSpeed);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "presetModeId=%d\n",presetModeId);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "moveX=%d\n",moveX);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "moveY=%d\n",moveY);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "displayedLines=%d\n",mainMenu_displayedLines);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "cpu=%d\n",mainMenu_CPU_model);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "chipset=%d\n",mainMenu_chipset);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "spritecollisions=%d\n",mainMenu_spriteCollisions);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "cpu=%d\n",-mainMenu_CPU_speed);
    fputs(buffer,f);

    char namebuffer[256];
    if (uae4all_image_file0[0]) {
        strcpy(namebuffer,uae4all_image_file0);
        replace (namebuffer,'|',' ');
        snprintf((char*)buffer, 255, "df0=%s\n",namebuffer);
        fputs(buffer,f);
    } else {
        fputs("df0=\n",f);
    }
    if (uae4all_image_file1[0] && nr_drives > 1) {
        strcpy(namebuffer,uae4all_image_file1);
        replace (namebuffer,'|',' ');
        snprintf((char*)buffer, 255, "df1=%s\n",namebuffer);
        fputs(buffer,f);
    } else {
        fputs("df1=\n",f);
    }
    if (uae4all_image_file2[0] && nr_drives > 2) {
        strcpy(namebuffer,uae4all_image_file2);
        replace (namebuffer,'|',' ');
        snprintf((char*)buffer, 255, "df2=%s\n",namebuffer);
        fputs(buffer,f);
    } else {
            fputs("df2=\n",f);
    }
    if (uae4all_image_file3[0] && nr_drives > 3) {
        strcpy(namebuffer,uae4all_image_file3);
        replace (namebuffer,'|',' ');
        snprintf((char*)buffer, 255, "df3=%s\n",namebuffer);
        fputs(buffer,f);
    } else {
            fputs("df3=\n",f);
    }
    snprintf((char*)buffer, 255, "script=%d\n",mainMenu_enableScripts);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "screenshot=%d\n",mainMenu_enableScreenshots);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "boot_hd=%d\n",mainMenu_bootHD);
    fputs(buffer,f);
    if (uae4all_hard_dir[0] == '\0')
        snprintf((char*)buffer, 255, "hard_disk_dir=%s\n","*");
    else
        snprintf((char*)buffer, 255, "hard_disk_dir=%s\n",uae4all_hard_dir);
    fputs(buffer,f);
    //HDF0
    if (uae4all_hard_file0[0] == '\0')
        snprintf((char*)buffer, 255, "hard_disk_file0=%s\n","*");
    else
        snprintf((char*)buffer, 255, "hard_disk_file0=%s\n",uae4all_hard_file0);
    fputs(buffer,f);
    //HDF1
    if (uae4all_hard_file1[0] == '\0')
        snprintf((char*)buffer, 255, "hard_disk_file1=%s\n","*");
    else
        snprintf((char*)buffer, 255, "hard_disk_file1=%s\n",uae4all_hard_file1);
    fputs(buffer,f);
    //HDF2
    if (uae4all_hard_file2[0] == '\0')
        snprintf((char*)buffer, 255, "hard_disk_file2=%s\n","*");
    else
        snprintf((char*)buffer, 255, "hard_disk_file2=%s\n",uae4all_hard_file2);
    fputs(buffer,f);
    //HDF3
    if (uae4all_hard_file3[0] == '\0')
        snprintf((char*)buffer, 255, "hard_disk_file3=%s\n","*");
    else
        snprintf((char*)buffer, 255, "hard_disk_file3=%s\n",uae4all_hard_file3);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "chipmemory=%d\n",mainMenu_chipMemory);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "slowmemory=%d\n",mainMenu_slowMemory);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "fastmemory=%d\n",mainMenu_fastMemory);
    fputs(buffer,f);
#if defined(AROS)
	snprintf((char*)buffer, 255, "VSync=%d\n",mainMenu_vsync);
 	fputs(buffer,f);
#endif

    strcpy(namebuffer,custom_kickrom);
    replace (namebuffer,'|',' ');
    snprintf((char*)buffer, 255, "custom_kickrom=%s\n",namebuffer);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "useSavesFolder=%d\n",mainMenu_useSavesFolder);
    fputs(buffer,f);
    fclose(f);
    return 1;
}


void loadconfig(int general)
{
// general == 0 (default): loading disk specific config after inserting floppy #1 
// general == 1: first time, loading general config
// general == 2: loading hdf-file-specific config after inserting hdf file #1
// general == 4: loading hd-dir-specific config after selecting hd dir
// general == 5: loading general config via the load config... filerequester option on main menu 
#if defined(__PSP2__) || defined(__SWITCH__)
	if (general == 1)
	{
    //first time opening the screen on Vita
	 int visibleAreaWidth_old = visibleAreaWidth;
	 int mainMenu_displayedLines_old = mainMenu_displayedLines;
	 visibleAreaWidth = 400;
	 mainMenu_displayedLines = 240;
	 update_display();
	 visibleAreaWidth=visibleAreaWidth_old;
	 mainMenu_displayedLines = mainMenu_displayedLines_old;
	}
#endif

    char path[300];

    if(general == 1) {
        snprintf(path, 300, "%s/conf/adfdir.conf", launchDir);
        FILE *f1=fopen(path,"rt");
        if(!f1) {
            printf ("No config file %s!\n",path);
            strcpy(currentDir, launchDir);
                strcat(currentDir, "/roms/");
        } else {
            fscanf(f1,"path=%s\n",&currentDir);
            fclose(f1);
            printf("adfdir loaded. currentDir=%s\n", currentDir);
        }
    }

    if (general == 1)
        snprintf(path, 300, "%s/conf/uaeconfig.conf", launchDir);
    else if (general == 3)
        snprintf(path, 300, config_filename, launchDir);
    else if(general == 0)
        create_configfilename(path, uae4all_image_file0, 0);
    else if (general == 2) {
        path[0] = '\0';
        if(uae4all_hard_file0[0] != '\0')
            create_configfilename(path, uae4all_hard_file0, 0);
        if(path[0] == '\0')
            return;
    }
    else if (general == 4) {
        path[0] = '\0';
        if(uae4all_hard_dir[0] != '\0')
            create_configfilename(path, uae4all_hard_dir, 1);
        if(path[0] == '\0')
            return;
    }
    else if (general == 5) {
        snprintf(path, 300, config_load_filename);
    }

    FILE *f=fopen(path,"rt");
    if (!f) {
        printf ("No config file %s!\n",path);
    } else {
        // Set everthing to default and clear HD settings
        SetDefaultMenuSettings(general);

        char filebuffer[256];
#if defined(PANDORA)
        int dummy;
#endif
        fscanf(f,"kickstart=%d\n",&kickstart);
#if defined(PANDORA)
        fscanf(f,"scaling=%d\n",&dummy);
#else
        fscanf(f,"scaling=%d\n",&mainMenu_enableHWscaling);
#endif
        fscanf(f,"mousemultiplier=%d\n",&mainMenu_mouseMultiplier);
        //remain compatible with old configuration versions
        if (mainMenu_mouseMultiplier < 10) {
            if (mainMenu_mouseMultiplier == 1 || mainMenu_mouseMultiplier == 2 || mainMenu_mouseMultiplier == 4) {
                mainMenu_mouseMultiplier *= 100;
            }
            else {
                mainMenu_mouseMultiplier = DEFAULT_MOUSEMULTIPLIER;
            }
        }
        fscanf(f,"mouseemulation=%d\n",&mainMenu_mouseEmulation );
        fscanf(f,"frameskip=%d\n",&mainMenu_frameskip);
        fscanf(f,"sound=%d\n",&mainMenu_sound );
        if (mainMenu_sound >= 10) {
            mainMenu_soundStereo = 1;
            mainMenu_sound -= 10;
        } else
            mainMenu_soundStereo = 0;
        fscanf(f,"soundstereosep=%d\n",&mainMenu_soundStereoSep );
        fscanf(f,"soundrate=%d\n",&sound_rate);
        fscanf(f,"autosave=%d\n",&mainMenu_autosave);
        fscanf(f,"gp2xclock=%d\n", &gp2xClockSpeed);
        fscanf(f,"joyport=%d\n",&mainMenu_joyPort);
        fscanf(f,"autofireRate=%d\n",&mainMenu_autofireRate);
        fscanf(f,"scanlines=%d\n",&mainMenu_scanlines);
        fscanf(f,"enableScreenshots=%d\n",&mainMenu_enableScreenshots);
        fscanf(f,"floppyspeed=%d\n",&mainMenu_floppyspeed);
        fscanf(f,"drives=%d\n",&nr_drives);
        fscanf(f,"videomode=%d\n",&mainMenu_ntsc);
        fscanf(f,"mainMenu_cpuSpeed=%d\n",&mainMenu_cpuSpeed);
        fscanf(f,"presetModeId=%d\n",&presetModeId);
        fscanf(f,"moveX=%d\n",&moveX);
        fscanf(f,"moveY=%d\n",&moveY);
        fscanf(f,"displayedLines=%d\n",&mainMenu_displayedLines);
		fscanf(f,"cpu=%d\n",&mainMenu_CPU_model);
        fscanf(f,"chipset=%d\n",&mainMenu_chipset);
        fscanf(f,"spritecollisions=%d\n",&mainMenu_spriteCollisions);
        fscanf(f,"cpu=%d\n",&mainMenu_CPU_speed);
        if(mainMenu_CPU_speed < 0) {
            // New version of this option
            mainMenu_CPU_speed = -mainMenu_CPU_speed;
        } else {
            // Old version (500 5T 1200 12T 12T2)
            if(mainMenu_CPU_speed >= 2) {
                // 1200: set to 68020 with 14 MHz
                mainMenu_CPU_model = 1;
                mainMenu_CPU_speed--;
                if(mainMenu_CPU_speed > 2)
                    mainMenu_CPU_speed = 2;
            }
        }

        memset(filebuffer, 0, 256);
        //fscanf cannot be used to read zero length strings, so read the equal sign, too
        if (fscanf(f,"df0%s\n",filebuffer)) {
            replace(filebuffer,' ','|');
            strcpy(uae4all_image_file0,filebuffer+1);
            extractFileName(uae4all_image_file0,filename0);
        } else {
            memset(uae4all_image_file0, 0, 256);
            memset(filename0, 0, 256);
        }
        if (fscanf(f,"df1%s\n",filebuffer)) {
            replace(filebuffer,' ','|');
            strcpy(uae4all_image_file1,filebuffer+1);
            extractFileName(uae4all_image_file1,filename1);
        } else {
            memset(uae4all_image_file1, 0, 256);
            memset(filename1, 0, 256);
        }
        if (fscanf(f,"df2%s\n",filebuffer)) {
            replace(filebuffer,' ','|');
            strcpy(uae4all_image_file2,filebuffer+1);
            extractFileName(uae4all_image_file2,filename2);
        } else {
            memset(uae4all_image_file2, 0, 256);
            memset(filename2, 0, 256);
        }
        if (fscanf(f,"df3%s\n",filebuffer)) {
            replace(filebuffer,' ','|');
            strcpy(uae4all_image_file3,filebuffer+1);
            extractFileName(uae4all_image_file3,filename3);
        } else {
            memset(uae4all_image_file3, 0, 256);
            memset(filename3, 0, 256);
        }

        mainMenu_drives=nr_drives;
        // in versions <=1.70, some config files are missing the following
        // hd settings, so skip them if the `script=`` line is absent
        int oldconfig = fscanf(f,"script=%d\n",&mainMenu_enableScripts);
        if (oldconfig != 0) {
            fscanf(f,"screenshot=%d\n", &mainMenu_enableScreenshots);
            fscanf(f,"boot_hd=%d\n",&mainMenu_bootHD);

            fscanf(f,"hard_disk_dir=",uae4all_hard_dir);
            uae4all_hard_dir[0] = '\0';
            {
                char c[2] = {0, 0};
                *c = fgetc(f);
                while (*c && (*c != '\n')) {
                    strcat(uae4all_hard_dir, c);
                    *c = fgetc(f);
                }
            }
            if (uae4all_hard_dir[0] == '*')
                uae4all_hard_dir[0] = '\0';
    			//HDF0
            fscanf(f,"hard_disk_file0=",uae4all_hard_file0);
            uae4all_hard_file0[0] = '\0';
            {
                char c[2] = {0, 0};
                *c = fgetc(f);
                while (*c && (*c != '\n')) {
                    strcat(uae4all_hard_file0, c);
                    *c = fgetc(f);
                }
            }
            if (uae4all_hard_file0[0] == '*')
                uae4all_hard_file0[0] = '\0';
            //HDF1
            fscanf(f,"hard_disk_file1=",uae4all_hard_file1);
            uae4all_hard_file1[0] = '\0';
            {
                char c[2] = {0, 0};
                *c = fgetc(f);
                while (*c && (*c != '\n')) {
                    strcat(uae4all_hard_file1, c);
                    *c = fgetc(f);
                }
            }
            if (uae4all_hard_file1[0] == '*')
                uae4all_hard_file1[0] = '\0';
            //HDF2
            fscanf(f,"hard_disk_file2=",uae4all_hard_file2);
            uae4all_hard_file2[0] = '\0';
            {
                char c[2] = {0, 0};
                *c = fgetc(f);
                while (*c && (*c != '\n')) {
                    strcat(uae4all_hard_file2, c);
                    *c = fgetc(f);
                }
            }
            if (uae4all_hard_file2[0] == '*')
                uae4all_hard_file2[0] = '\0';
            //HDF3
            fscanf(f,"hard_disk_file3=",uae4all_hard_file3);
            uae4all_hard_file3[0] = '\0';
            {
                char c[2] = {0, 0};
                *c = fgetc(f);
                while (*c && (*c != '\n')) {
                    strcat(uae4all_hard_file3, c);
                    *c = fgetc(f);
                }
            }
            if (uae4all_hard_file3[0] == '*')
                uae4all_hard_file3[0] = '\0';
        }
        fscanf(f,"chipmemory=%d\n",&mainMenu_chipMemory);
        fscanf(f,"slowmemory=%d\n",&mainMenu_slowMemory);
        fscanf(f,"fastmemory=%d\n",&mainMenu_fastMemory);
#if defined(AROS)
		fscanf(f,"VSync=%d\n",&mainMenu_vsync);
#endif
        memset(filebuffer, 0, 256);
        fscanf(f,"custom_kickrom=%s\n",filebuffer);
        replace(filebuffer,' ','|');
        if (filebuffer[0]) {
            strcpy(custom_kickrom, filebuffer);
        }
        // ignore this option. All saves are always made into saves folder now
        fscanf(f,"useSavesFolder=%d\n",&mainMenu_useSavesFolder);
        mainMenu_useSavesFolder = DEFAULT_USESAVESFOLDER;
        fclose(f);
    }
// make sure the just-loaded mainMenu_displayedLines is not changed by setPresetMode
    int old_displayedLines = mainMenu_displayedLines;
    SetPresetMode(presetModeId);
    mainMenu_displayedLines = old_displayedLines;
    UpdateCPUModelSettings();
    UpdateChipsetSettings();
    {
#if !defined(__PSP2__) && !defined(__SWITCH__)
        update_display();
#endif
    }
    UpdateMemorySettings();
}
