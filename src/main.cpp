int kickstart=1;
int oldkickstart=-1;	/* reload KS at startup */

extern char launchDir[300];

extern "C" int main( int argc, char *argv[] );

/*
  * UAE - The Un*x Amiga Emulator
  *
  * Main program
  *
  * Copyright 1995 Ed Hanway
  * Copyright 1995, 1996, 1997 Bernd Schmidt
  */
#include "sysconfig.h"
#include "sysdeps.h"
#include <assert.h>
#include "config.h"
#include "uae.h"
#include "options.h"
#include "thread.h"
#include "debug_uae4all.h"
#include "gensound.h"
#include "events.h"
#include "memory-uae.h"
#include "audio.h"
#include "sound.h"
#include "custom.h"
#include "m68k/m68k_intrf.h"
#include "disk.h"
#include "debug.h"
#include "xwin.h"
#include "joystick.h"
#include "keybuf.h"
#include "gui.h"
#include "zfile.h"
#include "autoconf.h"
#include "osemu.h"
#include "exectasks.h"
#include "bsdsocket.h"
#include "drawing.h"
#include "menu.h" 
#include "menu_config.h"
#include "gp2xutil.h"
#include "keyboard.h"
#include "uibottom.h"

/* PocketUAE */
#include "native2amiga.h"
#include <malloc.h>

#ifdef USE_SDL
#include "SDL.h"
extern SDL_Surface *current_screenshot;
#endif
#ifdef GP2X
#include "gp2xutil.h"
#endif

long int version = 256*65536L*UAEMAJOR + 65536L*UAEMINOR + UAESUBREV;

struct uae_prefs currprefs, changed_prefs; 

int no_gui = 0;
int joystickpresent = 0;
int cloanto_rom = 0;

int64_t g_uae_epoch = 0;

struct gui_info gui_data;

char warning_buffer[256];

/* If you want to pipe printer output to a file, put something like
 * "cat >>printerfile.tmp" above.
 * The printer support was only tested with the driver "PostScript" on
 * Amiga side, using apsfilter for linux to print ps-data.
 *
 * Under DOS it ought to be -p LPT1: or -p PRN: but you'll need a
 * PostScript printer or ghostscript -=SR=-
 */


void discard_prefs ()
{
}

void default_prefs ()
{
    produce_sound = 2;
    prefs_gfx_framerate = 0;

    strcpy (prefs_df[0], ROM_PATH_PREFIX "df0.adf");
    strcpy (prefs_df[1], ROM_PATH_PREFIX "df1.adf");

	snprintf(romfile, 256, "%s/kickstarts/%s",launchDir,kickstarts_rom_names[kickstart]);
	if(strlen(extended_rom_names[kickstart]) == 0)
	  snprintf(extfile, 256, "");
	else
	  snprintf(extfile, 256, "%s/kickstarts/%s",launchDir,extended_rom_names[kickstart]);
	FILE *f=fopen (romfile, "r" );
	if(!f){
		strcpy (romfile, "kick.rom");
	}
	else fclose(f);
	
	snprintf(romkeyfile, 256, "%s/kickstarts/%s",launchDir,"rom.key");	

	f=fopen (romkeyfile, "r" );
	if(!f)
	{
		strcpy (romkeyfile, "rom.key");
	}
	else fclose(f);
	
	/* 1MB */
    prefs_chipmem_size = 0x00100000;
    prefs_bogomem_size = 0;
	changed_prefs.fastmem_size = 0;
}

int quit_program = 0;

void uae_reset (void)
{
    //gui_purge_events();
    black_screen_now();
    quit_program = 2;
    set_special (SPCFLAG_BRK);
}

void uae_quit (void)
{
    if (quit_program != -1)
	quit_program = -1;
}

void reset_all_systems (void)
{
    init_eventtab ();
    memory_reset ();
    // the following is a workaround to prevent failed fdopen commands for hdf files
    filesys_reset ();
    reset_hdConf();
    filesys_start_threads ();
}

/* Okay, this stuff looks strange, but it is here to encourage people who
 * port UAE to re-use as much of this code as possible. Functions that you
 * should be using are do_start_program() and do_leave_program(), as well
 * as real_main(). Some OSes don't call main() (which is braindamaged IMHO,
 * but unfortunately very common), so you need to call real_main() from
 * whatever entry point you have. You may want to write your own versions
 * of start_program() and leave_program() if you need to do anything special.
 * Add #ifdefs around these as appropriate.
 */
void do_start_program (void)
{
	quit_program = 2;
	m68k_go (1);
}

void do_leave_program (void)
{
#ifdef USE_SDL
#if defined(__PSP2__) || defined(__SWITCH__) //On Vita, only remove keyboard graphics from memory when quitting the emu
#ifdef __PSP2__ // NOT __SWITCH__
	//De-Initialize touch panels
	psp2QuitTouch();
#endif
#endif
  if(current_screenshot != NULL)
    SDL_FreeSurface(current_screenshot);
#endif
	     
    graphics_leave ();
    close_sound ();
    zfile_exit ();
#ifdef USE_SDL
    SDL_Quit ();
#endif
    memory_cleanup ();
#ifdef __SWITCH__
    socExit();
#endif
}

void start_program (void)
{
    do_start_program ();
}

void leave_program (void)
{
    do_leave_program ();
}

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000

static u32 *SOC_buffer = NULL;

typedef struct {
	unsigned int sdlkey, key;
	const char *name;
} sdl_3dsbuttons;

// mappings like dingoo SDL
sdl_3dsbuttons buttons3ds[] = {
	{KEY_A, DS_A, "A btn"},
	{KEY_B, DS_B, "B btn"},
	{KEY_X, DS_X, "X btn"},
	{KEY_Y, DS_Y, "Y btn"},
	{KEY_L, DS_L, "L btn"},
	{KEY_R, DS_R, "R btn"},
	{KEY_ZL, DS_ZL, "ZL btn"},
	{KEY_ZR, DS_ZR, "ZR btn"},
	{KEY_START, DS_START, "START btn"},
	{KEY_SELECT, DS_SELECT, "SELECT btn"},
	{KEY_DUP, DS_UP1, "DPAD UP"},
	{KEY_DDOWN, DS_DOWN1, "DPAD DOWN"},
	{KEY_DLEFT, DS_LEFT1, "DPAD LEFT"},
	{KEY_DRIGHT, DS_RIGHT1, "DPAD RIGHT"},
	{KEY_CSTICK_UP, DS_UP3, "CSTK UP"},
	{KEY_CSTICK_DOWN, DS_DOWN3, "CSTK DOWN"},
	{KEY_CSTICK_LEFT, DS_LEFT3, "CSTK LEFT"},
	{KEY_CSTICK_RIGHT, DS_RIGHT3, "CSTK RIGHT"},
	{KEY_TOUCH, DS_TOUCH, "TOUCH"},
	{KEY_CPAD_UP, DS_UP2, "CPAD UP"},	// included in KEY_UP
	{KEY_CPAD_DOWN, DS_DOWN2, "CPAD DOWN"},
	{KEY_CPAD_LEFT, DS_LEFT2, "CPAD LEFT"},
	{KEY_CPAD_RIGHT, DS_RIGHT2, "CPAD RIGHT"},
	{0,0,0}
};

void real_main (int argc, char **argv)
{
    //appletLockExit();

	// allocate buffer for SOC service
	SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);

	if(SOC_buffer == NULL) {
		write_log("soc buffer memalign: failed to allocate");
		exit(0);
	}

	// Now intialise soc:u service
	int ret;
	if ((ret = socInit(SOC_buffer, SOC_BUFFERSIZE)) != 0) {
    	write_log("socInit: 0x%08X\n", (unsigned int)ret);
		exit(0);
	}

#ifdef USE_SDL
    SDL_Init (SDL_INIT_VIDEO 
#if !defined(NO_SOUND) && !defined(GP2X)
 			| SDL_INIT_AUDIO
#endif
	);
#endif

	// init romfs file system
	romfsInit();
	atexit((void (*)())romfsExit);

	// init 3DS buttons to their values
	for (int i=0; buttons3ds[i].key!=0; ++i)
		SDL_N3DSKeyBind(buttons3ds[i].sdlkey, (SDLKey)buttons3ds[i].key);

	// Initialize timebase
	g_uae_epoch = read_processor_time();
	syncbase = 1000000; // Microseconds

	mkdir(UAE_PATH, 0777);
	mkdir(UAE_PATH "/roms", 0777);
	mkdir(UAE_PATH "/saves", 0777);
	mkdir(UAE_PATH "/conf", 0777);
	mkdir(UAE_PATH "/kickstarts", 0777);
	mkdir(UAE_PATH "/thumbs", 0777);
	mkdir(UAE_PATH "/tmp", 0777);
	strcpy(launchDir, UAE_PATH);

	/* PocketUAE prefs */
    default_prefs_uae (&currprefs);
    default_prefs();
#ifdef GP2X
    gp2x_init(argc, argv);
#endif
	// Set everthing to default and clear HD settings
	SetDefaultMenuSettings(1);
    
	loadconfig (1);
	if (! graphics_setup ()) {
		exit (1);
	}
	rtarea_init ();

	hardfile_install();

	if (! setup_sound ()) {
		write_log ("Sound driver unavailable: Sound output disabled\n");
		produce_sound = 0;
	}
	uib_init();

	int err = gui_init ();
	if (err == -1) {
		write_log ("Failed to initialize the GUI\n");
	} else if (err == -2) {
		exit (0);
	}
	if (sound_available && produce_sound > 1 && ! init_audio ()) {
		write_log ("Sound driver unavailable: Sound output disabled\n");
		produce_sound = 0;
	}

	/* Install resident module to get 8MB chipmem, if requested */
	rtarea_setup ();

	keybuf_init ();

#ifdef USE_AUTOCONFIG
    expansion_init ();
#endif

    memory_init ();

    filesys_install (); 
    native2amiga_install ();

    custom_init (); /* Must come after memory_init */
    DISK_init ();

    m68k_init(0);

    gui_update ();

#ifdef GP2X
    switch_to_hw_sdl(1);
#endif
	{
		start_program ();
	}
    leave_program ();
}

#ifndef NO_MAIN_IN_MAIN_C
int main (int argc, char *argv[])
{
    real_main (argc, argv);
    return 0;
}


void default_prefs_uae (struct uae_prefs *p)
{
    p->chipset_mask = CSMASK_ECS_AGNUS;
    
    p->cpu_level = M68000;
    
    p->fastmem_size = 0x00000000;
#if !( defined(PANDORA) )
    p->z3fastmem_size = 0x00000000;
    p->gfxmem_size = 0x00000000;
#endif

    p->mountinfo = alloc_mountinfo ();
}

void discard_prefs_uae (struct uae_prefs *p)
{
    free_mountinfo (p->mountinfo);
}
#endif
