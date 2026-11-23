#include <SDL/SDL.h>
extern "C" { SDL_Surface *zoomSurface(SDL_Surface *src, double zoomx, double zoomy, int smooth); SDL_Surface *rotozoomSurface(SDL_Surface *src, double angle, double zoom, int smooth); }
#define SMOOTHING_OFF 0
#define SMOOTHING_ON 1
