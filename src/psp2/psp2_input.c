#include "psp2_input.h"
#ifdef USE_SDL2
#include "sdl2_to_sdl1.h"
#endif
#ifdef __SWITCH__
extern int mainMenu_swapAB;
extern int singleJoycons;
#include <switch.h>
#endif
extern int inside_menu;

SDLKey getKey(Uint8 button) {

#ifdef __SWITCH__
	if (singleJoycons)
	{
		Uint8 oldbutton = button;
		if (hidGetControllerType((HidControllerID) 0) == TYPE_JOYCON_LEFT) {
			switch(oldbutton) {
				case PAD_DOWN:
					button = PAD_CIRCLE;
					break;
				case PAD_RIGHT:
					button = PAD_TRIANGLE;
					break;
				case PAD_UP:
					button = PAD_SQUARE;
					break;
				case PAD_LEFT:
					button = PAD_CROSS;
					break;
				case LSTICK_UP:
					button = LSTICK_LEFT;
					break;
				case LSTICK_LEFT:
					button = LSTICK_DOWN;
					break;
				case LSTICK_DOWN:
					button = LSTICK_RIGHT;
					break;
				case LSTICK_RIGHT:
					button = LSTICK_UP;
					break;
				case PAD_SL_LEFT:
					button = PAD_L;
					break;
				case PAD_SR_LEFT:
					button = PAD_R;
					break;
				case PAD_L:
				case PAD_R:
					button = BUTTON_NONE;
					break;
				default:
					break;
			}
		} else {
			switch(oldbutton) {
				case PAD_CROSS:
					button = PAD_SQUARE;
					break;
				case PAD_CIRCLE:
					button = PAD_CROSS;
					break;
				case PAD_TRIANGLE:
					button = PAD_CIRCLE;
					break;
				case PAD_SQUARE:
					button = PAD_TRIANGLE;
					break;
				case RSTICK_UP:
					button = LSTICK_RIGHT;
					break;
				case RSTICK_LEFT:
					button = LSTICK_UP;
					break;
				case RSTICK_DOWN:
					button = LSTICK_LEFT;
					break;
				case RSTICK_RIGHT:
					button = LSTICK_DOWN;
					break;
				case PAD_START:
					button = PAD_SELECT;
					break;
				case PAD_SL_RIGHT:
					button = PAD_L;
					break;
				case PAD_SR_RIGHT:
					button = PAD_R;
					break;
				case PAD_L:
				case PAD_R:
					button = BUTTON_NONE;
					break;
				default:
					break;
			}
		}
	}
#endif

	switch(button) {
		case PAD_UP:
#ifdef __SWITCH__
		case LSTICK_UP:
#endif
		return SDLK_UP;

		case PAD_DOWN:
#ifdef __SWITCH__
		case LSTICK_DOWN:
#endif
		return SDLK_DOWN;

		case PAD_RIGHT:
#ifdef __SWITCH__
		case LSTICK_RIGHT:
#endif
		return SDLK_RIGHT;
	
		case PAD_LEFT:
#ifdef __SWITCH__
		case LSTICK_LEFT:
#endif
		return SDLK_LEFT;

		case PAD_START:
		return SDLK_LALT;

		case PAD_SELECT:
		return SDLK_LCTRL;

		case PAD_CROSS:
#ifdef __SWITCH__
		if (mainMenu_swapAB) {
			return SDLK_END;
		}
#endif
		return SDLK_PAGEDOWN;

		case PAD_CIRCLE:
#ifdef __SWITCH__
		if (mainMenu_swapAB) {
			return SDLK_PAGEDOWN;
		}
#endif
		return SDLK_END;

		case PAD_TRIANGLE:
		return SDLK_PAGEUP;

		case PAD_SQUARE:
		return SDLK_HOME;

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

	if(event != NULL && inside_menu) {

		switch (event->type) {
			
			case SDL_JOYAXISMOTION:
			break;
			 
			case SDL_JOYBUTTONDOWN:
				if (event->jbutton.which==0) // Only Joystick 0 controls the menu
				{
					event->type = SDL_KEYDOWN;
					event->key.keysym.sym = getKey(event->jbutton.button);
				}
			break;
			 
			 case SDL_JOYBUTTONUP:
			 	if (event->jbutton.which==0)
				{
					event->type = SDL_KEYUP;
					event->key.keysym.sym = getKey(event->jbutton.button);
				}
			 break;
			 
			 default:
			 break;
		 }
	 }
	 
	 return ret;
}
