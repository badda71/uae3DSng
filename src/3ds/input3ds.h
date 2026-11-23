/*
 * uae3DSng - 3DS input glue for the uae4all2 core
 *
 * Replaces what psp2/psp2_input.h provides on Vita/Switch:
 *  - PAD_* button constants (here: libctru HID bit masks, not SDL joystick indices)
 *  - N3DS_PollEvent(): SDL_PollEvent() + key events synthesized from the pad
 *    while the emulator menu is open (equivalent of PSP2_PollEvent)
 *
 * Face buttons follow the *physical position* convention used by the Switch port:
 *   Vita name   position   3DS button
 *   CIRCLE      right      A
 *   CROSS       bottom     B
 *   TRIANGLE    top        X
 *   SQUARE      left       Y
 */
#ifndef UAE3DSNG_INPUT3DS_H
#define UAE3DSNG_INPUT3DS_H

#include <3ds.h>
#include <SDL/SDL.h>

#define PAD_UP        KEY_DUP
#define PAD_DOWN      KEY_DDOWN
#define PAD_LEFT      KEY_DLEFT
#define PAD_RIGHT     KEY_DRIGHT
#define PAD_CIRCLE    KEY_A
#define PAD_CROSS     KEY_B
#define PAD_TRIANGLE  KEY_X
#define PAD_SQUARE    KEY_Y
#define PAD_L         KEY_L
#define PAD_R         KEY_R
#define PAD_L2        KEY_ZL      /* New 3DS only */
#define PAD_R2        KEY_ZR      /* New 3DS only */
#define PAD_SELECT    KEY_SELECT
#define PAD_START     KEY_START

#ifdef __cplusplus
extern "C" {
#endif

/* call once after SDL_Init(): stops the SDL driver from typing hid buttons into the Amiga */
void input3ds_init(void);
/* snapshot hid state (pumps SDL so hidScanInput() has run); call once per frame */
void input3ds_update(void);
unsigned int input3ds_held(void);          /* KEY_* bits, valid after input3ds_update() */
int input3ds_cpad_x(void);                 /* circle pad, -156..156 */
int input3ds_cpad_y(void);                 /* circle pad, -156..156, up positive */

int N3DS_PollEvent(SDL_Event *event);

/* SDL_JoystickGetButton()/GetAxis() look-alikes for the built-in pad ("joystick" 0 only).
 * button: one of the PAD_* masks; axis: 0=left X, 1=left Y (down positive), 2=right X, 3=right Y
 * (right stick = New 3DS C-stick, digital only); range is SDL-like +-32767. */
int input3ds_button(int joy, unsigned int mask);
int input3ds_axis(int joy, int axis);

#ifdef __cplusplus
}
#endif
#endif
