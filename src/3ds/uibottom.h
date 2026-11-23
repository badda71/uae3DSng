/*
 * uae3DSng - bottom screen UI (derived from uae3DS uibottom.h, Copyright 2020 Sebastian Weber)
 *
 * Touch keyboard + touchpad mouse + status LEDs on the bottom screen.
 * uib_init() once after the SDL video mode exists; uib_update() once per emulated frame.
 */
#ifndef UAE_UIBOTTOM_H
#define UAE_UIBOTTOM_H

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

extern void uib_init(void);
extern void uib_update(void);

extern uikbd_key uikbd_keypos[];
extern volatile int uib_must_redraw;   /* bit mask of enum uib_action */
extern int nowSuperThrottle;         /* "turbo" icon on the status bar while set */

#endif
