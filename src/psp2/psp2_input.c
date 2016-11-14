#include "psp2_input.h"

SDLKey getKey(Uint8 button) {
	
	switch(button) {

		case PAD_UP:
		return SDLK_UP;

		case PAD_DOWN:
		return SDLK_DOWN;

		case PAD_RIGHT:
		return SDLK_RIGHT;
	
		case PAD_LEFT:
		return SDLK_LEFT;

		case PAD_START:
		return SDLK_END;

		case PAD_SELECT:
		return SDLK_PAGEUP;

		case PAD_CROSS:
		return SDLK_PAGEDOWN;

		case PAD_CIRCLE:
		return SDLK_HOME;

		case PAD_TRIANGLE:
		return SDLK_LALT;

		case PAD_SQUARE:
		return SDLK_LCTRL;

		case PAD_L:
		return SDLK_RSHIFT;

		case PAD_R:
		return SDLK_RCTRL;

		default:
		break;
	}
	
	return 0;
}

int PSP2_PollEvent(SDL_Event *event) {
	
	int ret = SDL_PollEvent(event);

	if(event != NULL) {

		switch (event->type) {
			
			case SDL_JOYAXISMOTION:
			break;
			 
			case SDL_JOYBUTTONDOWN:
				event->type = SDL_KEYDOWN;
				event->key.keysym.sym = getKey(event->jbutton.button);
			break;
			 
			 case SDL_JOYBUTTONUP:
				event->type = SDL_KEYUP;
				event->key.keysym.sym = getKey(event->jbutton.button);
			 break;
			 
			 default:
			 break;
		 }
	 }
	 
	 return ret;
}
