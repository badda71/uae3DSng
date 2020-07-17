extern void SetDefaultMenuSettings(int general);
extern void UpdateMemorySettings();
extern void UpdateCPUModelSettings();
extern void UpdateChipsetSettings();
extern void SetPresetMode(int mode);
extern void reset_hdConf(void);
extern void kill_hd_configs();
#if defined(__PSP2__) || defined(__SWITCH__)
#endif

#ifndef _MENU_CONFIG_CPP
extern int mainMenu_max_tap_time;
extern int mainMenu_click_time;
extern int mainMenu_single_tap_timeout;
extern int mainMenu_max_double_tap_time;
extern int mainMenu_locked_drag_timeout;
extern int mainMenu_tap_and_drag_gesture;
extern int mainMenu_locked_drags;

extern int mainMenu_chipMemory;
extern int mainMenu_slowMemory;
extern int mainMenu_fastMemory;
extern int mainMenu_bootHD;
extern int mainMenu_filesysUnits;
extern int mainMenu_drives;
extern int mainMenu_floppyspeed;
extern int mainMenu_CPU_model;
extern int mainMenu_chipset;
extern int mainMenu_spriteCollisions;
extern int mainMenu_sound;
extern int mainMenu_soundStereo;
extern int mainMenu_soundStereoSep;
extern int mainMenu_CPU_speed;
extern int mainMenu_cpuSpeed;
extern int mainMenu_joyPort;
extern int mainMenu_autofireRate;
extern int mainMenu_mouseMultiplier;
extern int mainMenu_mouseEmulation;
extern int mainMenu_displayedLines;
extern int mainMenu_displayHires;
extern char presetMode[20];
extern int presetModeId;
extern int mainMenu_cutLeft;
extern int mainMenu_cutRight;
extern int mainMenu_ntsc;
extern int mainMenu_frameskip;
extern int mainMenu_autofire;
extern int visibleAreaWidth;

extern int saveMenu_n_savestate;

extern int mainMenu_autosave;

extern int gp2xClockSpeed;
extern int mainMenu_scanlines;
extern int mainMenu_enableScreenshots;
extern int mainMenu_enableScripts;

#if defined(AROS)
extern int mainMenu_vsync;
#endif
extern char custom_kickrom[256];
#endif