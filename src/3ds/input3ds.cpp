/*
 * uae3DSng - 3DS input glue for the uae4all2 core (see input3ds.h)
 */
#include <string.h>
#include <3ds.h>
#include <SDL/SDL.h>
#include "input3ds.h"

extern int inside_menu;   /* gp2x/menu/menu_main.cpp */

static u32 g_held = 0;
static circlePosition g_cpad = {0, 0};

extern "C" void input3ds_init(void)
{
	/* The SDL 3DS driver reports every hid bit as an SDL key (A->'a', START->RETURN, ...).
	 * uae4all2 polls the pad itself (gui_handle_events) and would otherwise see those as
	 * typed Amiga keys. Unbind all 32 bits; N3DS_PollEvent() synthesizes menu keys instead. */
	SDL_N3DSKeyBind(0xFFFFFFFFu, SDLK_UNKNOWN);
}

extern "C" void input3ds_update(void)
{
	SDL_PumpEvents();            /* runs hidScanInput() inside the SDL 3DS driver */
	g_held = hidKeysHeld();
	hidCircleRead(&g_cpad);
}

extern "C" unsigned int input3ds_held(void) { return g_held; }

extern "C" int input3ds_button(int joy, unsigned int mask)
{
	return (joy == 0 && (g_held & mask)) ? 1 : 0;
}

extern "C" int input3ds_axis(int joy, int axis)
{
	if (joy != 0) return 0;
	switch (axis) {
	case 0: return  g_cpad.dx * 210;               /* same scaling as the SDL 3DS joystick driver */
	case 1: return -g_cpad.dy * 210;               /* hid: up positive; SDL: down positive */
	case 2: return (g_held & KEY_CSTICK_RIGHT) ? 32767 : (g_held & KEY_CSTICK_LEFT) ? -32767 : 0;
	case 3: return (g_held & KEY_CSTICK_DOWN)  ? 32767 : (g_held & KEY_CSTICK_UP)   ? -32767 : 0;
	}
	return 0;
}
extern "C" int input3ds_cpad_x(void) { return g_cpad.dx; }
extern "C" int input3ds_cpad_y(void) { return g_cpad.dy; }

/* Same key convention as getKey() in psp2_input.c, which the gp2x menus were written for.
 * A = confirm, B = back (Nintendo layout; Vita default is the reverse). */
static SDLKey menu_key_for(u32 bit)
{
	switch (bit) {
	case KEY_DUP:    case KEY_CPAD_UP:    return SDLK_UP;
	case KEY_DDOWN:  case KEY_CPAD_DOWN:  return SDLK_DOWN;
	case KEY_DLEFT:  case KEY_CPAD_LEFT:  return SDLK_LEFT;
	case KEY_DRIGHT: case KEY_CPAD_RIGHT: return SDLK_RIGHT;
	case KEY_START:  return SDLK_LALT;
	case KEY_SELECT: return SDLK_LCTRL;
	case KEY_A:      return SDLK_PAGEDOWN;
	case KEY_B:      return SDLK_END;
	case KEY_X:      return SDLK_PAGEUP;
	case KEY_Y:      return SDLK_HOME;
	case KEY_L:      return SDLK_RSHIFT;
	case KEY_R:      return SDLK_RCTRL;
	default:         return SDLK_UNKNOWN;
	}
}

static const u32 MENU_BITS[] = {
	KEY_DUP, KEY_DDOWN, KEY_DLEFT, KEY_DRIGHT,
	KEY_CPAD_UP, KEY_CPAD_DOWN, KEY_CPAD_LEFT, KEY_CPAD_RIGHT,
	KEY_START, KEY_SELECT, KEY_A, KEY_B, KEY_X, KEY_Y, KEY_L, KEY_R
};
#define N_MENU_BITS (sizeof(MENU_BITS) / sizeof(MENU_BITS[0]))

extern "C" int N3DS_PollEvent(SDL_Event *event)
{
	static SDL_Event pending[2 * N_MENU_BITS];
	static unsigned head = 0, tail = 0;
	static u32 prev_held = 0;

	if (head < tail) {                       /* drain synthesized events first */
		if (event) *event = pending[head];
		++head;
		return 1;
	}
	head = tail = 0;

	int ret = SDL_PollEvent(event);          /* also runs hidScanInput() */
	if (ret) return ret;

	u32 held = hidKeysHeld();
	if (!inside_menu) {                      /* don't replay buttons held when the menu opens */
		prev_held = held;
		return 0;
	}

	u32 changed = held ^ prev_held;
	prev_held = held;
	for (unsigned i = 0; i < N_MENU_BITS; i++) {
		u32 bit = MENU_BITS[i];
		if (!(changed & bit)) continue;
		SDLKey k = menu_key_for(bit);
		if (k == SDLK_UNKNOWN) continue;
		SDL_Event e;
		memset(&e, 0, sizeof(e));
		e.type = (held & bit) ? SDL_KEYDOWN : SDL_KEYUP;
		e.key.state = (held & bit) ? SDL_PRESSED : SDL_RELEASED;
		e.key.keysym.sym = k;
		pending[tail++] = e;
	}
	if (head < tail) {
		if (event) *event = pending[head];
		++head;
		return 1;
	}
	return 0;
}
