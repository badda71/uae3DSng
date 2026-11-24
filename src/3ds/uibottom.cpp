/*
 * uae3DSng - bottom screen UI: touch keyboard, touchpad mouse, status LEDs and messages
 *
 * Derived from uae3DS (uibottom.cpp, Copyright 2020 Sebastian Weber). Differences to uae3DS:
 *  - the touchscreen is read directly (hidTouchRead) instead of via SDL mouse events, so the result does
 *    not depend on the size of the emulator's SDL surface;
 *  - keys go straight into the Amiga keyboard buffer (Amiga key codes), mouse movement and the left button
 *    go straight into the mouse state of the custom chips;
 *  - the keyboard slide animation is time based (no worker thread).
 *
 * Drawing runs in the SDL 3DS driver's video thread (SDL_RequestCall), everything else in the emulator thread.
 */
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL.h>
#include <SDL_image.h>
#include <3ds.h>
#include <citro3d.h>
#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "keyboard.h"
#include "gui.h"
#include "menu_config.h"
#include "uibottom.h"
#include "log3ds.h"

uikbd_key uikbd_keypos[] = {
	//  x,  y,   w,   h,         key,stky, flgs, name
	// toggle kb button
	{   0,-15,  36,  15,         255,   0,  0,  "ToggleKB"},
	// 1st Row
	{   0,  0,  32,  16,      AK_ESC,   0,  0,  "ESC"},
	{  32,  0,  16,  16,       AK_F1,   0,  0,  "F1"},
	{  48,  0,  16,  16,       AK_F2,   0,  0,  "F2"},
	{  64,  0,  16,  16,       AK_F3,   0,  0,  "F3"},
	{  80,  0,  16,  16,       AK_F4,   0,  0,  "F4"},
	{  96,  0,  16,  16,       AK_F5,   0,  0,  "F5"},
	{ 112,  0,  16,  16,       AK_F6,   0,  0,  "F6"},
	{ 128,  0,  16,  16,       AK_F7,   0,  0,  "F7"},
	{ 144,  0,  16,  16,       AK_F8,   0,  0,  "F8"},
	{ 160,  0,  16,  16,       AK_F9,   0,  0,  "F9"},
	{ 176,  0,  16,  16,      AK_F10,   0,  0,  "F10"},
	{ 192,  0,  32,  16,      AK_DEL,   0,  0,  "DEL"},
	{ 224,  0,  32,  16,     AK_HELP,   0,  0,  "HELP"},
	{ 256,  0,  16,  16, AK_NPLPAREN,   0,  0,  "NP ("},
	{ 272,  0,  16,  16, AK_NPRPAREN,   0,  0,  "NP )"},
	{ 288,  0,  16,  16,    AK_NPDIV,   0,  0,  "NP /"},
	{ 304,  0,  16,  16,    AK_NPMUL,   0,  0,  "NP *"},
	// 2nd Row
	{   0, 16,  32,  16,AK_BACKQUOTE,   0,  0,  "`"},
	{  32, 16,  16,  16,        AK_1,   0,  0,  "1"},
	{  48, 16,  16,  16,        AK_2,   0,  0,  "2"},
	{  64, 16,  16,  16,        AK_3,   0,  0,  "3"},
	{  80, 16,  16,  16,        AK_4,   0,  0,  "4"},
	{  96, 16,  16,  16,        AK_5,   0,  0,  "5"},
	{ 112, 16,  16,  16,        AK_6,   0,  0,  "6"},
	{ 128, 16,  16,  16,        AK_7,   0,  0,  "7"},
	{ 144, 16,  16,  16,        AK_8,   0,  0,  "8"},
	{ 160, 16,  16,  16,        AK_9,   0,  0,  "9"},
	{ 176, 16,  16,  16,        AK_0,   0,  0,  "0"},
	{ 192, 16,  16,  16,    AK_MINUS,   0,  0,  "-"},
	{ 208, 16,  16,  16,    AK_EQUAL,   0,  0,  "="},
	{ 224, 16,  16,  16,AK_BACKSLASH,   0,  0,  "\\"},
	{ 240, 16,  16,  16,       AK_BS,   0,  0,  "Backspace"},
	{ 256, 16,  16,  16,      AK_NP7,   0,  0,  "NP 7"},
	{ 272, 16,  16,  16,      AK_NP8,   0,  0,  "NP 8"},
	{ 288, 16,  16,  16,      AK_NP9,   0,  0,  "NP 9"},
	{ 304, 16,  16,  16,    AK_NPSUB,   0,  0,  "NP -"},
	// 3rd Row
	{   0, 32,  32,  16,      AK_TAB,   0,  0,  "TAB"},
	{  32, 32,  16,  16,        AK_Q,   0,  0,  "Q"},
	{  48, 32,  16,  16,        AK_W,   0,  0,  "W"},
	{  64, 32,  16,  16,        AK_E,   0,  0,  "E"},
	{  80, 32,  16,  16,        AK_R,   0,  0,  "R"},
	{  96, 32,  16,  16,        AK_T,   0,  0,  "T"},
	{ 112, 32,  16,  16,        AK_Y,   0,  0,  "Y"},
	{ 128, 32,  16,  16,        AK_U,   0,  0,  "U"},
	{ 144, 32,  16,  16,        AK_I,   0,  0,  "I"},
	{ 160, 32,  16,  16,        AK_O,   0,  0,  "O"},
	{ 176, 32,  16,  16,        AK_P,   0,  0,  "P"},
	{ 192, 32,  16,  16, AK_LBRACKET,   0,  0,  "["},
	{ 208, 32,  16,  16, AK_RBRACKET,   0,  0,  "]"},
	{ 224, 32,  32,  32,      AK_RET,   0,  0,  "RETURN"},
	{ 256, 32,  16,  16,      AK_NP4,   0,  0,  "NP 4"},
	{ 272, 32,  16,  16,      AK_NP5,   0,  0,  "NP 5"},
	{ 288, 32,  16,  16,      AK_NP6,   0,  0,  "NP 6"},
	{ 304, 32,  16,  16,    AK_NPADD,   0,  0,  "NP +"},
	// 4th Row
	{   0, 48,  32,  16,     AK_CTRL,   2,  0,  "CTRL"},
	{  32, 48,  16,  16,        AK_A,   0,  0,  "A"},
	{  48, 48,  16,  16,        AK_S,   0,  0,  "S"},
	{  64, 48,  16,  16,        AK_D,   0,  0,  "D"},
	{  80, 48,  16,  16,        AK_F,   0,  0,  "F"},
	{  96, 48,  16,  16,        AK_G,   0,  0,  "G"},
	{ 112, 48,  16,  16,        AK_H,   0,  0,  "H"},
	{ 128, 48,  16,  16,        AK_J,   0,  0,  "J"},
	{ 144, 48,  16,  16,        AK_K,   0,  0,  "K"},
	{ 160, 48,  16,  16,        AK_L,   0,  0,  "L"},
	{ 176, 48,  16,  16,AK_SEMICOLON,   0,  0,  ";"},
	{ 192, 48,  16,  16,    AK_QUOTE,   0,  0,  "'"},
	{ 256, 48,  16,  16,      AK_NP1,   0,  0,  "NP 1"},
	{ 272, 48,  16,  16,      AK_NP2,   0,  0,  "NP 2"},
	{ 288, 48,  16,  16,      AK_NP3,   0,  0,  "NP 3"},
	{ 304, 48,  16,  48,      AK_ENT,   0,  0,  "ENTER"},
	// 5th Row
	{   0, 64,  48,  16,      AK_LSH,   1,  0,  "LSHIFT"},
	{  48, 64,  16,  16,        AK_Z,   0,  0,  "Z"},
	{  64, 64,  16,  16,        AK_X,   0,  0,  "X"},
	{  80, 64,  16,  16,        AK_C,   0,  0,  "C"},
	{  96, 64,  16,  16,        AK_V,   0,  0,  "V"},
	{ 112, 64,  16,  16,        AK_B,   0,  0,  "B"},
	{ 128, 64,  16,  16,        AK_N,   0,  0,  "N"},
	{ 144, 64,  16,  16,        AK_M,   0,  0,  "M"},
	{ 160, 64,  16,  16,    AK_COMMA,   0,  0,  ","},
	{ 176, 64,  16,  16,   AK_PERIOD,   0,  0,  "."},
	{ 192, 64,  16,  16,    AK_SLASH,   0,  0,  "/"},
	{ 208, 64,  32,  16,      AK_RSH,   1,  0,  "RSHIFT"},
	{ 240, 64,  16,  16,       AK_UP,   0,  0,  "C_UP"},
	{ 256, 64,  32,  16,      AK_NP0,   0,  0,  "NP 0"},
	{ 288, 64,  16,  16,    AK_NPDEL,   0,  0,  "NP ."},
	// 6th row
	{   0, 80,  32,  17,     AK_LALT,   4,  0,  "LALT"},
	{  32, 80,  16,  17,     AK_LAMI,   8,  0,  "LAMI"},
	{  48, 80, 144,  17,      AK_SPC,   0,  0,  "SPACE"},
	{ 192, 80,  16,  17,     AK_RAMI,   8,  0,  "RAMI"},
	{ 208, 80,  16,  17,     AK_RALT,   4,  0,  "RALT"},
	{ 224, 80,  16,  17,       AK_LF,   0,  0,  "C_LEFT"},
	{ 240, 80,  16,  17,       AK_DN,   0,  0,  "C_DOWN"},
	{ 256, 80,  16,  17,       AK_RT,   0,  0,  "C_RIGHT"},
	// Finish
	{   0,  0,   0,   0,          -1,   0,  0,  ""}
};


// status line stuff

#define TD_PADX 9
#define TD_PADY 2
#define TD_WIDTH 32
#define TD_LED_WIDTH 24
#define TD_PAD (TD_WIDTH - TD_LED_WIDTH)

#define ON_RGB_D 0x00ff00ff
#define OFF_RGB_D 0x0044000ff
#define ON_RGB_P 0xff0000ff
#define OFF_RGB_P 0x4400000ff

#define TD_NUM_WIDTH 7
#define TD_NUM_HEIGHT 7

#define TD_TOTAL_HEIGHT (TD_PADY * 2 + TD_NUM_HEIGHT)
#define TD_STARTX 218

int td_text_length = 0;

// data definitions
typedef struct {
	unsigned w;
	unsigned h;
	float fw,fh;
	C3D_Tex tex;
} DS3_Image;

// exposed variables
volatile int uib_must_redraw = UIB_NO;

// emulator state used here
extern int uae4all_keystate[256];
extern int buttonstate[3];
extern int lastmx, lastmy, newmousecounters;
extern void record_key(int kc);
int nowSuperThrottle = 0;               // "turbo" icon is shown while this is set

// touchpad tuning (uae3DS defaults)
static int uib_max_tap_time = 250;
static int uib_click_time = 100;
static int uib_single_tap_timeout = 250;
static int uib_max_double_tap_time = 250;
static int uib_locked_drag_timeout = 5000;
static int uib_tap_and_drag_gesture = 1;
static int uib_locked_drags = 0;
static int uib_mouse_speed = 2;          // touchpad pixels -> Amiga pixels (uae3DS: msens)

// static sprites
static DS3_Image background_spr;
static DS3_Image kbd1_spr;
static DS3_Image kbd2_spr;
static DS3_Image twistyup_spr;
static DS3_Image twistydn_spr;
static DS3_Image keymask_spr;
static DS3_Image turbo_icon_spr;

// dynamic sprites
static DS3_Image statusbar_spr;

// SDL Surfaces
static SDL_Surface *statusbar_img = NULL;
static SDL_Surface *font_img = NULL;

// static variables
static Handle repaintRequired;
static int uib_isinit = 0;
static u8 *gpusrc = NULL;
static u8 *status_gpusrc = NULL;
static volatile int kb_y_pos = 240;       // current top edge of the keyboard (240 = hidden)
static int kb_shown = 1;
static int kb_anim_active = 0, kb_anim_from = 0, kb_anim_to = 0;
static Uint32 kb_anim_start = 0;
#define KB_ANIM_MS 250
static int kb_activekey = -1;
static int sticky = 0;
static unsigned char keysPressed[256];

// sprite handling funtions
extern C3D_Mtx projection2;
extern C3D_RenderTarget* VideoSurface2;
extern int uLoc_projection;
extern Handle privateSem1;
extern "C" void SDL_RequestCall(void(*callback)(void*,int), void *param);

#define CLEAR_COLOR 0x000000FF
// Used to convert textures to 3DS tiled format
// Note: vertical flip flag set so 0,0 is top left of texture
#define TEXTURE_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

#define TEX_MIN_SIZE 64

static unsigned int mynext_pow2(unsigned int v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v >= TEX_MIN_SIZE ? v : TEX_MIN_SIZE;
}

#define B2T(x) (int)(((x)*400.0f)/320.0f+0.5f)

//---------------------------------------------------------------------------------
static inline void  drawImage( DS3_Image *img, int x, int y, int w, int h, int deg) {
//---------------------------------------------------------------------------------
	if (!img) return;
	if (!w) w=img->w;
	if (!h) h=img->h;
	float fw = (float)w;
	float fx = (float)x;

	int x1,x2,x3,x4,y1,y2,y3,y4;
	if (deg) {
		// rotated draw
		float fh = (float)h;
		float fy = (float)y;
		float rad = ((float)deg) * M_PI / 180.0f;
		float c = cosf(rad);
		float s = sinf(rad);
		float cx = fx + fw / 2.0f;
		float cy = fy + fh / 2.0f;
		x1 = B2T(c*(fx-cx)-s*(fy-cy)+cx);
		y1 = (int)(s*(fx-cx)+c*(fy-cy)+cy+0.5f);
		x2 = B2T(c*(fx-cx)-s*(fy+fh-cy)+cx);
		y2 = (int)(s*(fx-cx)+c*(fy+fh-cy)+cy+0.5f);
		x3 = B2T(c*(fx+fw-cx)-s*(fy-cy)+cx);
		y3 = (int)(s*(fx+fw-cx)+c*(fy-cy)+cy+0.5f);
		x4 = B2T(c*(fx+fw-cx)-s*(fy+fh-cy)+cx);
		y4 = (int)(s*(fx+fw-cx)+c*(fy+fh-cy)+cy+0.5f);
	} else {
		x1 = B2T(fx);
		y1 = y;
		x2 = B2T(fx);
		y2 = y + h;
		x3 = B2T(fx + fw);
		y3 = y;
		x4 = B2T(fx + fw);
		y4 = y + h;
	}

	C3D_TexBind(0, &(img->tex));
	// Draw a textured quad directly
	C3D_ImmDrawBegin(GPU_TRIANGLE_STRIP);
		C3D_ImmSendAttrib( x1, y1, 0.5f, 0.0f);	// v0 = position
		C3D_ImmSendAttrib( 0.0f, 0.0f, 0.0f, 0.0f);	// v1 = texcoord0

		C3D_ImmSendAttrib( x2, y2, 0.5f, 0.0f);
		C3D_ImmSendAttrib( 0.0f, img->fh, 0.0f, 0.0f);

		C3D_ImmSendAttrib( x3, y3, 0.5f, 0.0f);		// v0 = position
		C3D_ImmSendAttrib( img->fw, 0.0f, 0.0f, 0.0f);

		C3D_ImmSendAttrib( x4, y4, 0.5f, 0.0f);		// v0 = position
		C3D_ImmSendAttrib( img->fw, img->fh, 0.0f, 0.0f);
	C3D_ImmDrawEnd();
}

static void makeTexture(C3D_Tex *tex, const u8 *mygpusrc, unsigned hw, unsigned hh) {
	s32 i;
	svcWaitSynchronization(privateSem1, U64_MAX);
	// init texture
	C3D_TexDelete(tex);
	C3D_TexInit(tex, hw, hh, GPU_RGBA8);
	C3D_TexSetFilter(tex, GPU_NEAREST, GPU_NEAREST);

	// Convert image to 3DS tiled texture format
	GSPGPU_FlushDataCache(mygpusrc, hw*hh*4);
	C3D_SyncDisplayTransfer ((u32*)mygpusrc, GX_BUFFER_DIM(hw,hh), (u32*)(tex->data), GX_BUFFER_DIM(hw,hh), TEXTURE_TRANSFER_FLAGS);
	GSPGPU_FlushDataCache(tex->data, hw*hh*4);
	svcReleaseSemaphore(&i, privateSem1, 1);
}

// this function is NOT thread safe - it uses static buffer gpusrc!!
static void makeImage(DS3_Image *img, const u8 *pixels, unsigned w, unsigned h, int noconv) {

	img->w=w;
	img->h=h;
	unsigned hw=mynext_pow2(w);
	img->fw=(float)(w)/hw;
	unsigned hh=mynext_pow2(h);
	img->fh=(float)(h)/hh;

	if (noconv) { // pixels are already in a transferable format (buffer in linear RAM, ABGR pixel format, pow2-dimensions)
		makeTexture(&(img->tex), pixels, hw, hh);
	} else {
		// GX_DisplayTransfer needs input buffer in linear RAM
		// memset(gpusrc,0,hw*hh*4);

		// copy to linear buffer, convert from RGBA to ABGR
		const u8* src=pixels; u8 *dst;
		for(unsigned y = 0; y < h; y++) {
			dst=gpusrc+y*hw*4;
			for (unsigned x=0; x<w; x++) {
				int r = *src++;
				int g = *src++;
				int b = *src++;
				int a = *src++;

				*dst++ = a;
				*dst++ = b;
				*dst++ = g;
				*dst++ = r;
			}
		}
		makeTexture(&(img->tex), gpusrc, hw, hh);
	}

	return;
}

static int loadImage(DS3_Image *img, const char *fname) {
	SDL_Surface *s=IMG_Load(fname);
	if (!s) return -1;

	makeImage(img, (u8*)s->pixels, s->w,s->h,0);
	SDL_FreeSurface(s);
	return 0;
}


// bottom handling functions
// =========================
static inline void requestRepaint() {
	svcSignalEvent(repaintRequired);
}

static void uib_repaint(void *param, int topupdated) {
	s32 c;
	int i;
	
	if (svcWaitSynchronization(repaintRequired, 0)) return;
	svcClearEvent(repaintRequired);
	svcWaitSynchronization(privateSem1, U64_MAX);

	int ky = kb_y_pos;

	// Render the scene
	C3D_RenderTargetClear(VideoSurface2, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
	C3D_FrameDrawOn(VideoSurface2);

	// background
	drawImage(&background_spr, 0, 0, 0, 0, 0);

	// statusbar
	if (nowSuperThrottle)
		drawImage(&turbo_icon_spr, TD_STARTX - TD_PAD - turbo_icon_spr.w, 0, 0, 0, 0);
	drawImage(&statusbar_spr, 0, 0, 0, 0, 0);

	// keyboard
	DS3_Image *kb=(sticky & 1) == 1 ? &(kbd2_spr):&(kbd1_spr);
	drawImage(kb, 0, ky, 0, 0, 0);
	// keyboard twisty
	DS3_Image *tw = ky > (240 - (int)kb->h) ? &(twistyup_spr):&(twistydn_spr);
	drawImage(tw, 0, ky - tw->h, 0, 0, 0);
	// keys pressed
	uikbd_key *k;
	for (i=0;uikbd_keypos[i].key!=-1; ++i) {
		k=&(uikbd_keypos[i]);
		if (k->flags==1 || keysPressed[i]==0) continue;
		drawImage(&(keymask_spr),k->x,k->y+ky,k->w,k->h,0);
	}

	svcReleaseSemaphore(&c, privateSem1, 1);
}

// shutdown bottom
static void uib_shutdown() {
	if (!uib_isinit) return;

	SDL_RequestCall(NULL, NULL);

	svcCloseHandle(repaintRequired);

	if (gpusrc) linearFree(gpusrc);
	gpusrc = NULL;
	
	SDL_FreeSurface(statusbar_img);
	statusbar_img = NULL;
	if (font_img) SDL_FreeSurface(font_img);
	font_img = NULL;
	if (status_gpusrc) linearFree(status_gpusrc);
	status_gpusrc = NULL;

	uib_isinit = 0;
}

static void keypress_recalc() {
	int state,key;

	memset(keysPressed,0,sizeof(keysPressed));
	for (key = 0; uikbd_keypos[key].key!=-1; ++key) {
		state=0;
		if (uikbd_keypos[key].sticky) {
			if (uikbd_keypos[key].sticky & sticky) state=1;
		} else {
			if (key == kb_activekey) state=1;
		}
		keysPressed[key]=state;
	}
}

// keyboard show/hide: slides over KB_ANIM_MS, advanced from uib_update()
static void toggle_keyboard() {
	int y1 = 240 - (int)kbd1_spr.h;
	kb_shown = !kb_shown;
	kb_anim_from = kb_y_pos;
	kb_anim_to = kb_shown ? y1 : 240;
	kb_anim_start = SDL_GetTicks();
	kb_anim_active = 1;
}

static void keyboard_animate() {
	if (!kb_anim_active) return;
	Uint32 el = SDL_GetTicks() - kb_anim_start;
	if (el >= KB_ANIM_MS) {
		kb_y_pos = kb_anim_to;
		kb_anim_active = 0;
	} else {
		kb_y_pos = kb_anim_from + (int)(((long long)(kb_anim_to - kb_anim_from) * (long long)el) / KB_ANIM_MS);
	}
	uib_must_redraw |= UIB_REPAINT;
}

// exposed functions
void uib_init() {
	if (uib_isinit) return;
	log3ds("uib: init\n");

	// init gpusrc
	gpusrc = (u8*)linearAlloc(512*256*4);
	status_gpusrc = (u8*)linearAlloc(512*16*4);
	if (!gpusrc || !status_gpusrc) {
		log3ds("uib: linearAlloc failed, bottom screen UI disabled\n");
		if (gpusrc) linearFree(gpusrc);
		if (status_gpusrc) linearFree(status_gpusrc);
		gpusrc = status_gpusrc = NULL;
		return;
	}

	// pre-load sprites
	int bad = 0;
	bad |= loadImage(&background_spr,	"romfs:/bg_bot.png");
	bad |= loadImage(&kbd1_spr,		"romfs:/kbd1.png");
	bad |= loadImage(&kbd2_spr,		"romfs:/kbd2.png");
	bad |= loadImage(&twistyup_spr,	"romfs:/twistyup.png");
	bad |= loadImage(&twistydn_spr,	"romfs:/twistydn.png");
	bad |= loadImage(&turbo_icon_spr,	"romfs:/turbo.png");
	static const u8 mask_px[4] = {0x00, 0x00, 0x00, 0x80};
	makeImage(&keymask_spr, mask_px, 1, 1, 0);
	if (bad) log3ds("uib: %d image(s) from romfs failed to load (%s)\n", bad, SDL_GetError());

	font_img = SDL_LoadBMP("romfs:/8x8font.bmp");
	if (font_img) SDL_SetColorKey(font_img, SDL_SRCCOLORKEY, 0);
	else log3ds("uib: font not found, status messages disabled\n");

	// set up status sprite / framebuf
	memset(status_gpusrc, 0, 512*16*4);
	statusbar_img = SDL_CreateRGBSurfaceFrom((void *)status_gpusrc,
                        512, 16, 32, 512*4,
                        0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff);
	SDL_SetAlpha(statusbar_img, 0, 255);

	statusbar_spr.w=320;
	statusbar_spr.h=16;
	statusbar_spr.fw=(float)(statusbar_spr.w)/512.0f;
	statusbar_spr.fh=(float)(statusbar_spr.h)/16.0f;
	makeTexture(&(statusbar_spr.tex), status_gpusrc, 512, 16);
	td_text_length = TD_STARTX - turbo_icon_spr.w - 2 * TD_PAD;

	// other stuff
	kb_shown = 1;
	kb_y_pos = 240 - kbd1_spr.h;

	svcCreateEvent(&repaintRequired, RESET_ONESHOT);
	uib_isinit = 1;
	SDL_RequestCall(uib_repaint, NULL);

	uib_must_redraw |= UIB_REPAINT | UIB_RECALC_KEYPRESS;
	uib_update();
	atexit(uib_shutdown);
	log3ds("uib: ready (keyboard %ux%u)\n", kbd1_spr.w, kbd1_spr.h);
}

// status line funtions

static const char *numbers = {
"------ ------ ------ ------ ------ ------ ------ ------ ------ ------ "
"-xxxxx ---xx- -xxxxx -xxxxx -x---x -xxxxx -xxxxx -xxxxx -xxxxx -xxxxx "
"-x---x ----x- -----x -----x -x---x -x---- -x---- -----x -x---x -x---x "
"-x---x ----x- -xxxxx -xxxxx -xxxxx -xxxxx -xxxxx ----x- -xxxxx -xxxxx "
"-x---x ----x- -x---- -----x -----x -----x -x---x ---x-- -x---x -----x "
"-xxxxx ----x- -xxxxx -xxxxx -----x -xxxxx -xxxxx ---x-- -xxxxx -xxxxx "
"------ ------ ------ ------ ------ ------ ------ ------ ------ ------ "
};

static void uib_paint_led(int x, u32 col, int num)
{
	int y,j;
	
	for (y = 0; y < TD_TOTAL_HEIGHT; ++y) {
		for (j = 0 ; j < TD_LED_WIDTH; ++j) {
			*((u32*)(status_gpusrc + ((x + j + y * 512)<<2))) = col;
		}
	}

	if (num >= 0) {
		x += TD_PADX;
		u8 *nump1 = (u8 *)(numbers + (num/10) * TD_NUM_WIDTH);
		u8 *nump2 = (u8 *)(numbers + (num%10) * TD_NUM_WIDTH);
		for (y = 0; y < TD_NUM_HEIGHT; ++y) {
			for (j = 0 ; j < TD_NUM_WIDTH; ++j) {
				*((u32*)(status_gpusrc + ((x + j + (y + TD_PADY) * 512)<<2))) = 
					nump1[j + y * 10 * TD_NUM_WIDTH] == 'x'? 0xffffffff : 0x000000ff;
				*((u32*)(status_gpusrc + ((TD_NUM_WIDTH + x + j + (y + TD_PADY) * 512)<<2))) = 
					nump2[j + y * 10 * TD_NUM_WIDTH] == 'x'? 0xffffffff : 0x000000ff;
			}
		}
	}
}


// draw text with the 8x8 menu font (16 characters per row, from ' ' on) into the status bar
static void uib_text(const char *s, int x, int y, int maxchars)
{
	if (!font_img) return;
	SDL_Color white = {0xff, 0xff, 0xff, 0};
	SDL_SetPalette(font_img, SDL_LOGPAL, &white, 1, 1);
	for (int i = 0; s[i] && i < maxchars; i++) {
		int c = (s[i] & 0x7f) - 32;
		if (c < 0) c = 0;
		SDL_Rect from, to;
		from.x = (c & 0x0f) * 8; from.y = (c >> 4) * 8; from.w = 8; from.h = 8;
		to.x = x + i * 8; to.y = y; to.w = 8; to.h = 8;
		SDL_BlitSurface(font_img, &from, statusbar_img, &to);
	}
}

SDL_Surface *uib_font(void) { return font_img; }

static void uib_statusbar_recalc()
{
	static uae_u8 dt0 = -1;
	static uae_u8 dt1 = -1;
	static uae_u8 dm0 = -1;
	static uae_u8 dm1 = -1;
	static uae_u8 pow = -1;
	static int fps = -1;
	int upd=0;
	static char buf[100]={0};
	extern struct gui_info gui_data;

	if (dt0 != gui_data.drive_track[0] || 
		dm0 != gui_data.drive_motor[0])
	{
		dt0 = gui_data.drive_track[0];
		dm0 = gui_data.drive_motor[0];
		uib_paint_led(TD_STARTX + TD_WIDTH, dm0 ? ON_RGB_D : OFF_RGB_D, dt0);
		upd=1;
	}
	if (dt1 != gui_data.drive_track[1] || 
		dm1 != gui_data.drive_motor[1])
	{
		dt1 = gui_data.drive_track[1];
		dm1 = gui_data.drive_motor[1];
		uib_paint_led(TD_STARTX + 2*TD_WIDTH, dm1 ? ON_RGB_D : OFF_RGB_D, dt1);
		upd=1;
	}
	
	int cur_fps = gui_data.fps > 99 ? 99 : gui_data.fps;
	if (pow != gui_data.powerled || fps != cur_fps)	
	{
		pow = gui_data.powerled;
		fps = cur_fps;
		uib_paint_led(TD_STARTX, pow ? ON_RGB_P : OFF_RGB_P, fps);
		upd=1;
	}

	// message line: show_message is an end time in SDL ticks (see gui_set_message)
	if (show_message)
	{
		if (SDL_GetTicks() < (Uint32)show_message) {
			if (strcmp(buf, show_message_str)) {
				strncpy(buf, show_message_str, 99);
				SDL_Rect r;
				r.x = 0; r.y = 0; r.w = td_text_length; r.h = TD_TOTAL_HEIGHT;
				SDL_FillRect(statusbar_img, &r, 0x70708aff);
				uib_text(buf, 2, 2, (td_text_length-4)/8);
				upd=1;
			}
		} else {
			SDL_Rect r;
			r.x = 0; r.y = 0; r.w = td_text_length; r.h = TD_TOTAL_HEIGHT;
			SDL_FillRect(statusbar_img, &r, 0x00000000);
			show_message=0;
			*buf=0;
			upd=1;
		}
	}
	if (upd) {
		makeTexture(&(statusbar_spr.tex), (const u8 *)statusbar_img->pixels, 512, 16);
		uib_must_redraw |= UIB_REPAINT;
	}
}

// *****************************
// touchpad: lots of stuff below inspired from synaptics touchpad driver
// https://github.com/freedesktop/xorg-xf86-input-synaptics
// ******************************

enum TapState {
    TS_START,                   /* No tap/drag in progress */
    TS_1,                       /* After first touch */
    TS_MOVE,                    /* Pointer movement enabled */
    TS_2A,                      /* After first release */
    TS_2B,                      /* After second/third/... release */
    TS_SINGLETAP,               /* After timeout after first release */
    TS_3,                       /* After second touch */
    TS_DRAG,                    /* Pointer drag enabled */
    TS_4,                       /* After release when "locked drags" enabled */
    TS_5,                       /* After touch when "locked drags" enabled */
    TS_CLICKPAD_MOVE,           /* After left button press on a clickpad */
	TS_MOUSEDOWN,
	TS_MOUSEUP,
};

enum TapEvent { TE_NONE = 0, TE_DOWN, TE_UP };

static int get_timeout(TapState s)
{
	switch (s) {
    case TS_1:
    case TS_3:
    case TS_5:
        return uib_max_tap_time;
    case TS_SINGLETAP:
        return uib_click_time;
    case TS_2A:
        return uib_single_tap_timeout;
    case TS_2B:
        return uib_max_double_tap_time;
    case TS_4:
        return uib_locked_drag_timeout;
    default:
        break;
	}
	return 0;
}

static int mouse_state = 0;

static void set_tap_state(TapState s)
{
    int x=-1;
	switch (s) {
    case TS_START:
    case TS_1:
    case TS_2A:
    case TS_2B:
	case TS_MOUSEUP:
        x = 0;
        break;
    case TS_3:
    case TS_SINGLETAP:
	case TS_MOUSEDOWN:
        x = 1;
        break;
    default:
        break;
    }
    if (x != -1 && x != mouse_state) {
		buttonstate[0] = x;                  /* left mouse button */
		mouse_state = x;
	}
}

#define SETSTATE(x)								\
    {status = x;								\
	timeout = get_timeout(x);					\
	if (timeout) timeout += SDL_GetTicks();		\
	set_tap_state(x);							\
	is_timeout=0;}

// ev == TE_NONE is the once-per-frame tick that expires timeouts
static int tap_processing(TapEvent ev) {
	static Uint32 timeout = 0;
	static TapState status = TS_START;
	static int status2 = 0;
	int is_timeout = 0;

	if (ev == TE_NONE) {
		switch (status2) {
		case 2:
			set_tap_state(TS_MOUSEDOWN);
			--status2;
			break;
		case 1:
			set_tap_state(TS_MOUSEUP);
			--status2;
			break;
		}
	}
	
	if (timeout && SDL_GetTicks()>=timeout) {
		is_timeout = 1;
		timeout = 0;
	}
	if (ev == TE_NONE && !is_timeout) return 1;

 restart:
    switch (status) {
    case TS_START: /* No tap/drag in progress */
        if (ev == TE_DOWN)
            SETSTATE(TS_1)
        break;
    case TS_1: /* After first touch */
        if (is_timeout) {
            SETSTATE(TS_MOVE)
            goto restart;
        }
        else if (ev == TE_UP) {
            SETSTATE(TS_2A)
        }
        break;
    case TS_MOVE: /* Pointer movement enabled */
        if (ev == TE_UP) {
            SETSTATE(TS_START)
        }
        break;
    case TS_2A: /* After first release */
        if (ev == TE_DOWN)
            SETSTATE(TS_3)
        else if (is_timeout)
            SETSTATE(TS_SINGLETAP)
        break;
    case TS_2B: /* After second/third/... release */
        if (ev == TE_DOWN) {
            SETSTATE(TS_3)
        }
        else if (is_timeout) {
            SETSTATE(TS_START)
            status2=2;
        }
        break;
    case TS_SINGLETAP: /* After timeout after first release */
        if (ev == TE_DOWN)
            SETSTATE(TS_1)
        else if (is_timeout)
            SETSTATE(TS_START)
        break;
    case TS_3: /* After second touch */
        if (is_timeout) {
            if (uib_tap_and_drag_gesture) {
                SETSTATE(TS_DRAG)
            }
            else {
                SETSTATE(TS_1)
            }
            goto restart;
        }
        else if (ev == TE_UP) {
            SETSTATE(TS_2B)
        }
        break;
    case TS_DRAG:
        if (ev == TE_UP) {
            if (uib_locked_drags) {
                SETSTATE(TS_4)
            }
            else {
                SETSTATE(TS_START)
            }
        }
        break;
    case TS_4:
        if (is_timeout) {
            SETSTATE(TS_START)
            goto restart;
        }
        if (ev == TE_DOWN)
            SETSTATE(TS_5)
        break;
    case TS_5:
        if (is_timeout) {
            SETSTATE(TS_DRAG)
            goto restart;
        }
        else if (ev == TE_UP) {
            SETSTATE(TS_START)
        }
        break;
    default:
        break;
    }
	return 1;
}

// *************************
// Touchscreen handling: virtual keyboard or touchpad
// *************************

// send an Amiga key code to the emulated keyboard
static void send_key(int ak, int down)
{
	if (ak < 0 || ak > 0xff) return;
	if (down) {
		if (!uae4all_keystate[ak]) {
			uae4all_keystate[ak] = 1;
			record_key(ak << 1);
		}
	} else {
		uae4all_keystate[ak] = 0;
		record_key((ak << 1) | 1);
	}
}

#define log2 __builtin_ctz

static int process_touchpad = 0;

static void touch_down(int x, int y)
{
	int i;
	for (i = 0; uikbd_keypos[i].key != -1 ; ++i) {
		if (x >= uikbd_keypos[i].x &&
			x <  uikbd_keypos[i].x + uikbd_keypos[i].w &&
			y >= uikbd_keypos[i].y + kb_y_pos &&
			y <  uikbd_keypos[i].y + uikbd_keypos[i].h + kb_y_pos) break;
	}
	if (uikbd_keypos[i].key == -1) {
		// touch outside of keyboard - process touchpad
		if (y < kb_y_pos) {
			process_touchpad = 1;
			tap_processing(TE_DOWN);
		}
		return;
	}
	if (i == kb_activekey) return;           // ignore button down on an already pressed key
	kb_activekey = i;

	if (uikbd_keypos[i].key == 255) {        // keyboard toggle button
		toggle_keyboard();
		kb_activekey = -1;
		return;
	}

	static int sticky2key[4] = {-1,-1,-1,-1};
	if (uikbd_keypos[i].sticky > 0) {
		// sticky key (shift, ctrl, alt, amiga): toggles on every touch
		int pos = log2(uikbd_keypos[i].sticky & 0x0F);
		sticky = sticky ^ uikbd_keypos[i].sticky;
		if (sticky & uikbd_keypos[i].sticky) {
			sticky2key[pos] = uikbd_keypos[i].key;
			send_key(uikbd_keypos[i].key, 1);
		} else {
			int k = sticky2key[pos] == -1 ? uikbd_keypos[i].key : sticky2key[pos];
			sticky2key[pos] = -1;
			send_key(k, 0);
		}
	} else {
		send_key(uikbd_keypos[i].key, 1);
	}
	uib_must_redraw |= UIB_RECALC_KEYPRESS;
}

static void touch_up()
{
	if (process_touchpad) {
		process_touchpad = 0;
		tap_processing(TE_UP);
		return;
	}
	if (kb_activekey == -1) return;
	int i = kb_activekey;
	kb_activekey = -1;
	if (uikbd_keypos[i].sticky <= 0)
		send_key(uikbd_keypos[i].key, 0);
	uib_must_redraw |= UIB_RECALC_KEYPRESS;
}

static void touch_move(int dx, int dy)
{
	if (!process_touchpad) return;
	int scale = ((mainMenu_mouseMultiplier * 16) / 100) * uib_mouse_speed;
	lastmx += dx * scale;
	lastmy += dy * scale;
	newmousecounters = 1;
}

// once per emulated frame: read the touchscreen
static void uib_poll_touch()
{
	static int was_down = 0, lx = 0, ly = 0;
	touchPosition t;
	int down = (hidKeysHeld() & KEY_TOUCH) != 0;

	if (down) {
		hidTouchRead(&t);
		if (t.px == 0 && t.py == 0) down = was_down;      // glitch frame, keep previous state
		else if (!was_down) { touch_down(t.px, t.py); lx = t.px; ly = t.py; }
		else if (t.px != lx || t.py != ly) { touch_move(t.px - lx, t.py - ly); lx = t.px; ly = t.py; }
	}
	if (!down && was_down) touch_up();
	was_down = down;
}

// exposed: call once per emulated frame (from handle_events)
void uib_update(void)
{
	int uib_must_redraw_local;

	// init if needed
	if (!uib_isinit) uib_init();
	if (!uib_isinit) return;

	tap_processing(TE_NONE);       // expire tap timeouts / pending clicks
	uib_poll_touch();
	keyboard_animate();
	uib_statusbar_recalc();

	if (uib_must_redraw) {
		// needed for mutithreading
		uib_must_redraw_local = uib_must_redraw;
		uib_must_redraw = UIB_NO;
		
		if (uib_must_redraw_local & UIB_RECALC_KEYPRESS) {
			keypress_recalc();
		}
		requestRepaint();
	}
}
