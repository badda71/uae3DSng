/*
 * uae3DSng - video glue, see video3ds.h
 */
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

extern int mainMenu_shader;   /* menu_config.cpp: 3DS: 0 = 1:1 (no scaling), 1 = fit the screen height */

static float snap1(float f) { return (f > 0.995f && f < 1.005f) ? 1.0f : f; }

/* The Vita/Switch code asks for a destination rectangle (x, y, sw, sh) in 960x544 pixels. Only its proportions are used here: the
 * 3DS top screen is 400x240, so the old proportional mapping scaled a 320x240 picture to 0.94 horizontally / 1.0 vertically, which
 * made the driver filter (GPU_LINEAR whenever the scale is not 1.0) blur the picture although "nothing" was scaled.
 *   shader 0 (NONE): 1:1, i.e. GPU_NEAREST. A hires (half pixel width) picture is halved horizontally (exactly 2:1).
 *   otherwise:      uniform scale so that the picture is as high as the screen (0.94 for 256 lines, 1.0 for 240 lines).
 * The pixel aspect of the request (sw/w : sh/h) is kept in both cases. */
extern "C" void SDL_SetVideoModeScaling(int x, int y, float sw, float sh)
{
	(void)x; (void)y;
	if (!prSDLScreen || prSDLScreen->w <= 0 || prSDLScreen->h <= 0) return;

	float w = (float)prSDLScreen->w, h = (float)prSDLScreen->h;
	float aspect = (sw / w) / (sh / h);                       /* horizontal / vertical scale of the request: 1 lores, 0.5 hires */
	float fy = (mainMenu_shader == 0) ? 1.0f : TOP_H / h;
	float fx = fy * aspect;
	if (fx * w > TOP_W) {                                      /* never wider than the screen: shrink both axes */
		float k = TOP_W / (fx * w);
		fx *= k;
		fy *= k;
	}
	N3DS_SetScalingDirect(snap1(fx), snap1(fy), 1);
	N3DS_SetYOffset(0);
}

extern "C" void video3ds_set_native(int yoffset)
{
	N3DS_SetScalingDirect(1.0f, 1.0f, 1);
	N3DS_SetYOffset(yoffset);
}

extern "C" void SDL_SetVideoModeBilinear(int on) { (void)on; }
extern "C" void SDL_SetVideoModeSync(int on) { (void)on; }
