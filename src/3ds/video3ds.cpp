/*
 * uae3DSng - video glue, see video3ds.h
 */
#include <math.h>
#include <stdio.h>
#include <SDL/SDL.h>
#include "video3ds.h"

extern "C" {
	/* UAE3DS_SDL/src/video/n3ds/SDL_n3dsvideo.c */
	void N3DS_SetScalingDirect(float x, float y, int permanent);
	void N3DS_SetYOffset(int y);
}
extern SDL_Surface *prSDLScreen;

#define TOP_W  400.0f
#define TOP_H  240.0f

/* menu_config.cpp: the display settings (saved in the config file) */
extern int mainMenu_displayMode;    /* DISPLAY_1TO1 .. DISPLAY_CUSTOM */
extern int mainMenu_zoom;           /* Custom: percent, DISPLAY_ZOOM_MIN .. DISPLAY_ZOOM_MAX */
extern int mainMenu_vpan;           /* Custom: pixels, 0 = centered, positive = picture lower */
/* the render window variables of the (shared) menu / drawing code */
extern int mainMenu_displayedLines, mainMenu_displayHires, visibleAreaWidth, presetModeId, moveX, moveY;
void SetPresetMode(int mode);

static float snap1(float f) { return (f > 0.995f && f < 1.005f) ? 1.0f : f; }

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

const char *video3ds_mode_name(int mode)
{
	switch (mode) {
	case DISPLAY_1TO1:      return "1:1";
	case DISPLAY_FITHEIGHT: return "Fit height";
	case DISPLAY_FITWIDTH:  return "Fit width";
	default:                return "Custom";
	}
}

/* scale of the picture on the screen (screen pixels per picture pixel) in the current mode */
static float display_scale(void)
{
	switch (mainMenu_displayMode) {
	case DISPLAY_FITHEIGHT: return TOP_H / (float)DISPLAY_WIN_H;
	case DISPLAY_FITWIDTH:  return TOP_W / (float)DISPLAY_WIN_W;
	case DISPLAY_CUSTOM:    return (float)clampi(mainMenu_zoom, DISPLAY_ZOOM_MIN, DISPLAY_ZOOM_MAX) / 100.0f;
	default:                return 1.0f;
	}
}

int video3ds_zoom(void) { return (int)(display_scale() * 100.0f + 0.5f); }

/* how far the picture may be moved from the centered position: until its top or bottom edge meets the edge of the screen
 * (a picture that is smaller than the screen is kept inside it) */
static int pan_limit(float scale)
{
	return (int)(fabsf((float)DISPLAY_WIN_H * scale - TOP_H) * 0.5f);
}

int video3ds_pan(void)
{
	if (mainMenu_displayMode != DISPLAY_CUSTOM) return 0;
	int lim = pan_limit(display_scale());
	return clampi(mainMenu_vpan, -lim, lim);
}

int video3ds_visible_top(void)
{
	float s = display_scale();
	float top = ((TOP_H - (float)DISPLAY_WIN_H * s) * 0.5f) + (float)video3ds_pan();   /* screen line of the picture's first line */
	if (top >= 0.0f) return 0;
	return clampi((int)ceilf(-top / s), 0, DISPLAY_WIN_H - 1);
}

void video3ds_apply_display(void)
{
	float s = snap1(display_scale());
	/* the driver draws the (w1 x h1) surface centered on the screen, scaled; the pan is added to the vertical position.
	 * It is filtered (GPU_LINEAR) unless the scale is exactly 1.0. */
	N3DS_SetScalingDirect(s, s, 1);
	N3DS_SetYOffset(video3ds_pan());
}

/* Zoom / pan adjustments always work on Custom: coming from another mode, Custom starts with the zoom that is on the screen */
static void enter_custom(void)
{
	if (mainMenu_displayMode == DISPLAY_CUSTOM) return;
	mainMenu_zoom = clampi(video3ds_zoom(), DISPLAY_ZOOM_MIN, DISPLAY_ZOOM_MAX);
	mainMenu_vpan = 0;
	mainMenu_displayMode = DISPLAY_CUSTOM;
}

int video3ds_zoom_step(int dz)
{
	enter_custom();
	int old = mainMenu_zoom;
	mainMenu_zoom = clampi(mainMenu_zoom + dz, DISPLAY_ZOOM_MIN, DISPLAY_ZOOM_MAX);
	mainMenu_vpan = video3ds_pan();          /* a smaller picture has a smaller pan range */
	return mainMenu_zoom != old;
}

int video3ds_pan_step(int dp)
{
	enter_custom();
	int lim = pan_limit(display_scale());
	int old = clampi(mainMenu_vpan, -lim, lim);
	mainMenu_vpan = clampi(old + dp, -lim, lim);
	return mainMenu_vpan != old;
}

void video3ds_readout(char *buf, int size)
{
	snprintf(buf, size, "Zoom %d%%   Pan %+d", video3ds_zoom(), video3ds_pan());
}

/* The emulator renders the 320x256 PAL window (Amiga lines 44..299) whatever the settings are: lores, no hires, and the
 * display variables the shared menu / drawing code still has (presets, displayed lines, ...) say so. */
void video3ds_fix_window(void)
{
	SetPresetMode(2);
	presetModeId = 2;
	mainMenu_displayedLines = DISPLAY_WIN_H;
	visibleAreaWidth = DISPLAY_WIN_W;
	mainMenu_displayHires = 0;
	moveX = 0;
	moveY = 18;                              /* minfirstline (26) + 18 = line 44, the first line of the PAL display window */
	mainMenu_displayMode = clampi(mainMenu_displayMode, 0, DISPLAY_NUMMODES - 1);
	mainMenu_zoom = clampi(mainMenu_zoom, DISPLAY_ZOOM_MIN, DISPLAY_ZOOM_MAX);
	mainMenu_vpan = clampi(mainMenu_vpan, -DISPLAY_WIN_H, DISPLAY_WIN_H);
}

/* The Vita/Switch code asks for a destination rectangle (x, y, sw, sh) in 960x544 pixels. Only the menu's text screen still
 * calls this (through the shared init_text()): it is shown 1:1. */
extern "C" void SDL_SetVideoModeScaling(int x, int y, float sw, float sh)
{
	(void)x; (void)y; (void)sw; (void)sh;
	video3ds_set_native(0);
}

extern "C" void video3ds_set_native(int yoffset)
{
	N3DS_SetScalingDirect(1.0f, 1.0f, 1);
	N3DS_SetYOffset(yoffset);
}

extern "C" void SDL_SetVideoModeBilinear(int on) { (void)on; }
extern "C" void SDL_SetVideoModeSync(int on) { (void)on; }
