#include <SDL/SDL.h>
extern "C" { SDL_Surface *IMG_Load(const char *file); SDL_Surface *IMG_Load_RW(SDL_RWops *src, int freesrc); int IMG_Init(int); void IMG_Quit(void); }
