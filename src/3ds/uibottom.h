/*
  * UAE3DS - Amiga 500 emulator for Nintendo 3DS
  *
  * uibottom.h - functions for handling 3DS bottom screen
  *
  * Copyright 2020 Sebastian Weber
  */


#ifndef UAE_UIBOTTOM_H
#define UAE_UIBOTTOM_H
#include <SDL/SDL.h>

// exposed definitions
enum uib_action {
	UIB_NO = 0,
	UIB_REPAINT = 1,
	UIB_RECALC_KEYPRESS = 2,
	UIB_RECALC_STATUSBAR = 4
};

typedef struct {
	int x,y,w,h,key,sticky,flags;
	const char *name;
} uikbd_key;

// exposed functions
void uib_update(void);
int uib_handle_event(SDL_Event *);
void uib_init();
int uib_handle_tap_processing(SDL_Event *e);

// exposed variables
extern uikbd_key uikbd_keypos[];
extern volatile enum uib_action uib_must_redraw;
#endif
