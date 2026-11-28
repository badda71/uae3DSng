/*
 * uae3DSng - text input with the 3DS system keyboard. See textinput3ds.h.
 */
#include "textinput3ds.h"

#include <3ds.h>
#include <stdlib.h>
#include <string.h>

extern "C" int textinput3ds(const char *hint, const char *initial, char *out, size_t out_size)
{
    if (!out || out_size < 2)
        return 0;
    int maxlen = (int)out_size - 1;
    /* the limit counts characters (UTF-16 units), the result is UTF-8: up to 3 bytes each, plus the 0 */
    size_t bufsize = (size_t)maxlen * 3 + 1;
    char *buf = (char *)calloc(1, bufsize);
    if (!buf)
        return 0;

    SwkbdState swkbd;
    swkbdInit(&swkbd, SWKBD_TYPE_WESTERN, 2, maxlen);
    swkbdSetValidation(&swkbd, SWKBD_ANYTHING, 0, 0);       /* empty is allowed: it clears the text */
    swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Cancel", false);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "OK", true);
    if (hint && hint[0])
        swkbdSetHintText(&swkbd, hint);
    if (initial && initial[0])
        swkbdSetInitialText(&swkbd, initial);

    SwkbdButton button = swkbdInputText(&swkbd, buf, bufsize);
    int ok = button == SWKBD_BUTTON_RIGHT;
    if (ok) {
        strncpy(out, buf, out_size - 1);
        out[out_size - 1] = '\0';
    }
    free(buf);
    return ok;
}
