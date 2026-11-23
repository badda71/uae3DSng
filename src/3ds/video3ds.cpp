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

#define VITA_W 960.0f
#define VITA_H 544.0f
#define TOP_W  400.0f
#define TOP_H  240.0f

extern "C" void SDL_SetVideoModeScaling(int x, int y, float sw, float sh)
{
	(void)x; (void)y;
	if (!prSDLScreen || prSDLScreen->w <= 0 || prSDLScreen->h <= 0) return;

	float dw = sw * (TOP_W / VITA_W);          /* destination size on the 3DS top screen */
	float dh = sh * (TOP_H / VITA_H);
	float fx = dw / (float)prSDLScreen->w;
	float fy = dh / (float)prSDLScreen->h;
	if (fx * prSDLScreen->w > TOP_W) fx = TOP_W / (float)prSDLScreen->w;   /* never exceed the screen */
	if (fy * prSDLScreen->h > TOP_H) fy = TOP_H / (float)prSDLScreen->h;

	N3DS_SetScalingDirect(fx, fy, 1);
	N3DS_SetYOffset(0);
}

extern "C" void SDL_SetVideoModeBilinear(int on) { (void)on; }
extern "C" void SDL_SetVideoModeSync(int on) { (void)on; }
