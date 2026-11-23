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
/* (x,y,sw,sh): destination rectangle in the Vita's 960x544 coordinate space, as the menu code
 * computes it. It is converted to scale factors for the 400x240 top screen; the driver centers. */
void SDL_SetVideoModeScaling(int x, int y, float sw, float sh);
void SDL_SetVideoModeBilinear(int on);   /* driver picks GPU_LINEAR whenever scale != 1.0 */
void SDL_SetVideoModeSync(int on);       /* driver always syncs to vblank */
#ifdef __cplusplus
}
#endif
#endif
