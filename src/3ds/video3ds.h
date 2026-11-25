/*
 * uae3DSng - video glue: the Vita/Switch ports call SDL_SetVideoModeScaling/Bilinear/Sync
 * (provided by their patched SDL). The UAE3DS_SDL fork has no such calls but exposes
 * N3DS_SetScalingDirect()/N3DS_SetYOffset() in its video driver; map onto those.
 */
#ifndef UAE3DSNG_VIDEO3DS_H
#define UAE3DSNG_VIDEO3DS_H
#ifdef __cplusplus
extern "C" {
#endif
/* Only used for the Vita-style callers that are left (the menu text screen); the game picture goes through video3ds_apply_display(). */
void SDL_SetVideoModeScaling(int x, int y, float sw, float sh);
void SDL_SetVideoModeBilinear(int on);   /* driver picks GPU_LINEAR whenever scale != 1.0 */
void SDL_SetVideoModeSync(int on);       /* driver always syncs to vblank */
/* Show the current surface 1:1 (no scaling), shifted down by yoffset pixels (negative = up), centered horizontally. */
void video3ds_set_native(int yoffset);

/* ---- How the game picture is put on the top screen (400x240) ----------------------------------------------------------------
 * The emulator always renders the whole PAL window, 320x256 (Amiga lines 44..299). Where on the screen it ends up, and how big,
 * is decided on the GPU only, so the settings below change instantly:
 *   1:1         one picture pixel = one screen pixel, sharp. 240 of the 256 lines are visible (the middle ones).
 *   Fit height  the 256 lines exactly fill the screen height (94%); borders left and right.
 *   Fit width   the 320 pixels exactly fill the screen width (125%); only 192 of the lines are visible, see the vertical pan.
 *   Custom      Zoom 50..200% (100% = 1:1) and a vertical pan. Horizontally the picture is always centered.
 * Everything but exactly 100% is filtered (smooth). The vertical pan is in screen pixels, 0 = centered, positive = picture lower;
 * it is limited so that the picture's edge never moves past the edge of the screen. Only Custom has a pan, the other modes are
 * centered: the first zoom or pan adjustment (menu or Start + DPad) switches to Custom, starting from what is on the screen. */
enum { DISPLAY_1TO1 = 0, DISPLAY_FITHEIGHT, DISPLAY_FITWIDTH, DISPLAY_CUSTOM, DISPLAY_NUMMODES };
#define DISPLAY_WIN_W    320
#define DISPLAY_WIN_H    256
#define DISPLAY_ZOOM_MIN  50
#define DISPLAY_ZOOM_MAX 200

const char *video3ds_mode_name(int mode);
/* what is on the screen now: zoom in percent (rounded), vertical pan in pixels; both for the current mode */
int video3ds_zoom(void);
int video3ds_pan(void);
/* put the picture on the screen according to mainMenu_displayMode / _zoom / _vpan (call after every change and after every
 * SDL_SetVideoMode of the game surface) */
void video3ds_apply_display(void);
/* change the zoom by dz percent / the pan by dp pixels (limited). Anything but Custom becomes Custom first (with the zoom it has).
 * Return 1 if the value changed (0 at the limit). They do not touch the screen: video3ds_apply_display() does (not in the menu,
 * where the surface is the text screen). */
int video3ds_zoom_step(int dz);
int video3ds_pan_step(int dp);
/* the first line of the picture that is on the screen (0 unless it is zoomed in / panned so that the top is off the screen): things
 * the emulator draws into the picture itself (the Guru Meditation banner) go below it */
int video3ds_visible_top(void);
/* "Zoom 105%  Pan -3": what the on-screen readout says */
void video3ds_readout(char *buf, int size);
/* the render window is fixed: make the display variables of the menu code say so (after loading a config or setting the defaults) */
void video3ds_fix_window(void);
#ifdef __cplusplus
}
#endif
#endif
