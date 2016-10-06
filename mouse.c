#include "edit.h"

extern Buf Buffer;
extern unsigned CursorPos;

void handle_mouse(MouseEvent* mevnt) {
    switch (mevnt->button) {
        case MOUSE_LEFT:
            CursorPos = screen_getoff(&Buffer, CursorPos, mevnt->y, mevnt->x);
            break;
        case MOUSE_WHEELUP:
            CursorPos = buf_byline(&Buffer, CursorPos, -ScrollLines);
            break;
        case MOUSE_WHEELDOWN:
            CursorPos = buf_byline(&Buffer, CursorPos, ScrollLines);
            break;
        default:
            break;
    }
}
