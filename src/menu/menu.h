#include<stdio.h>
#include<SDL.h>

extern SDL_Surface *prSDLScreen;

#define MENU_FILE_SPLASH DATA_PREFIX "gp2xsplash.bmp"
#define MENU_FILE_BACKGROUND DATA_PREFIX "bg_top.png"
#define MENU_FILE_WINDOW DATA_PREFIX "window.bmp"
#define MENU_FILE_TEXT DATA_PREFIX "8x8font.bmp"
#define MENU_DIR_DEFAULT "."

typedef enum {
	MB_NONE,
	MB_OK,
	MB_YESNO
} mb_mode;

enum str_alignment {
	ALIGN_LEFT,
	ALIGN_RIGHT,
	ALIGN_CENTER
};

enum font_size {
	FONT_NORMAL
};

void init_kickstart();
void showWarning(const char *msg);
void exit_safely(int quit_via_home);
void text_draw_background();
void init_text(int splash);
void quit_text(void);
void write_text(int x, int y, const char * str);
void write_text_pos(int x, int y, const char * str);
void write_text_inv(int x, int y, const char * str);
void write_text_inv_pos(int x, int y, const char * str);
void write_centered_text(int y, char * str);
void write_num(int x, int y, int v);
void write_num_inv(int x, int y, int v);
void text_draw_window(int x, int y, int w, int h, const char *title);
void text_draw_barra(int x, int y, int w, int h, int per, int max);
void text_draw_window_bar(int x, int y, int w, int h, int per, int max, const char *title);
void _text_draw_window(SDL_Surface *sf, int x, int y, int w, int h, const char *title);
void _text_draw_window_bar(SDL_Surface *sf, int x, int y, int w, int h, int per, int max, const char *title);
void CreateScreenshot(int code);
int save_thumb(int code,char *path);
int save_png(SDL_Surface* surface,char *path);
void load_savestate_thumbnail();
void draw_image_pos(SDL_Surface *img, int x, int y);
void write_text_full (SDL_Surface *s, const char *str, int x, int y, int maxchars, enum str_alignment align, enum font_size size, Uint32 col, int inv);
int text_messagebox(char *title, char *text, mb_mode mode);

int createScript(int bIcon=0);

void text_flip(void);
void text_flip_with_image(SDL_Surface *img, int x, int y);
void loadconfig(int general=0);
int saveconfig(int general=0);
int create_configfilename(char *dest, char *basename, int fromDir);

void drawPleaseWait(void);
void menu_raise(void);
void menu_unraise(void);

int run_mainMenu();
int run_menuLoad(char *, int aLoadtype);
int run_menuGame();
int run_menuSavestates();
int run_menuMisc();
int run_menuControls();
int run_menuDisplay();
int run_menuMemDisk();
void update_display();

int run_menuControl();
enum { SCREENSHOT, ICON };

enum { MAIN_MENU_CASE_QUIT, MAIN_MENU_CASE_LOAD, MAIN_MENU_CASE_RUN, MAIN_MENU_CASE_RESET, MAIN_MENU_CASE_CANCEL, MAIN_MENU_CASE_SAVESTATES, MAIN_MENU_CASE_EJECT, MAIN_MENU_CASE_MISC, MAIN_MENU_CASE_SAVE, MAIN_MENU_CASE_DISPLAY, MAIN_MENU_CASE_MEMDISK, MAIN_MENU_CASE_LOAD_CONFIG, MAIN_MENU_CASE_DELETE_CONFIG};
enum { MEMDISK_MENU_CASE_MAIN, MEMDISK_MENU_CASE_MISC };

/* Just 0x0 and not 680x0, so that constants can fit in ARM instructions */
#define M68000 000
#define M68020 020

#define DEFAULT_STATUSLN 1
#define DEFAULT_MOUSEMULTIPLIER 100
#define DEFAULT_SOUND 1
#define DEFAULT_AUTOSAVE 1
#define DEFAULT_SYSTEMCLOCK 0
#define DEFAULT_SYNCTHRESHOLD 2
#define DEFAULT_CHIPSET_SELECT 0x100;//blitter=immediate is new default
#define DEFAULT_NTSC 0
#define DEFAULT_JOYCONF 2
#define DEFAULT_CHIPMEM_SELECT 1
#define DEFAULT_USE1MBCHIP 1
#define DEFAULT_ENABLE_HD 0
#define DEFAULT_DRIVES 1
#define DEFAULT_ENABLESCRIPTS 0
#define DEFAULT_ENABLESCREENSHOTS 0
#define DEFAULT_SCALING 0
#define DEFAULT_KICKSTART 1
#define DEFAULT_CPU_MODEL M68000
#define DEFAULT_SPRITECOLLISIONS 0
#define DEFAULT_USESAVESFOLDER 1

#define MENU_MEMDISK_WINDOW_WIDTH 40

/* Event intervals */
#define SELECTION_DRAWING_INTERVAL	250	/* [ms] inverted/normal text */
#define EVENT_POLLING_INTERVAL		50	/* [ms] SDL events polling */

/* What is being loaded */
#define MENU_LOAD_FLOPPY 1
#define MENU_LOAD_HD_DIR 2
#define MENU_LOAD_HDF 3
#define MENU_LOAD_CONFIG 4
#define MENU_LOAD_KICKROM 5
#define MENU_LOAD_DELETE_CONFIG 6
#define MENU_LOAD_IMPORT_SAVE 7
#define MENU_LOAD_DELETE_SAVE 8

#ifdef __SWITCH__
#define DEFAULT_SWAPAB 1
#define DEFAULT_SINGLEJOYCONS 0
#endif

#define MENU_MIN_HOLDING_TIME 200
#define MENU_MOVE_DELAY 50

extern Uint32 menu_text_color;			// text
extern Uint32 menu_text_color_inactive;	// inactive text
extern Uint32 menu_inv_color;			// blink text underlay
extern Uint32 menu_inv2_color;			// light text underlay (almost like background)
extern Uint32 menu_win0_color;			// window dropshadow
extern Uint32 menu_win1_color;			// window frame
