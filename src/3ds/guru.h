/*
 * uae3DSng - "Guru Meditation" banner on the emulated (top) screen.
 *
 * Shown when the emulator notices that the emulated Amiga has overwritten exec's low-memory structures
 * (vector table / ExecBase pointer) and can no longer run: like the real thing it is a black box with a
 * flashing red border and red text across the top of the screen, and a click of the left mouse button
 * reboots the Amiga.
 */
#ifndef UAE_GURU_H
#define UAE_GURU_H

extern void guru_show(unsigned code, unsigned task);  /* raise the alert (code = alert number, task = ThisTask address) */
extern void guru_hide(void);                          /* drop it (the Amiga is being reset) */
extern void guru_draw(void);                          /* once per frame, right before the frame is flipped to the screen */

#endif
