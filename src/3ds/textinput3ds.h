/*
 * uae3DSng - text input with the 3DS system keyboard (the software keyboard applet).
 */
#ifndef TEXTINPUT3DS_H
#define TEXTINPUT3DS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Asks for a line of text. hint is shown while the field is empty, initial (may be NULL) is the text to start with.
   Returns 1 when the user confirmed with OK (text in out, may be empty), 0 when cancelled or on a failure (out unchanged).
   The applet takes over both screens until it is closed, so call this from the menu only. */
int textinput3ds(const char *hint, const char *initial, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif
