#include "edit.h"

void unused(MouseEvent* mevnt) {
    (void)mevnt;
}

void move_cursor(MouseEvent* mevnt) {
    if (mevnt->y == 0) return;
    DotEnd = screen_getoff(&Buffer, DotEnd, mevnt->y-1, mevnt->x);
    TargetCol = buf_getcol(&Buffer, DotEnd);
    DotBeg = DotEnd;
}

void select(MouseEvent* mevnt) {
    (void)mevnt;
    unsigned bol = buf_bol(&Buffer, DotEnd);
    Rune r = buf_get(&Buffer, DotEnd);
    if (DotEnd == bol || r == '\n' || r == RUNE_CRLF) {
        DotBeg = bol;
        DotEnd = buf_eol(&Buffer, DotEnd);
    } else if (isword(r)) {
        DotBeg = buf_bow(&Buffer, DotEnd);
        DotEnd = buf_eow(&Buffer, DotEnd);
        if (Buffer.insert_mode) DotEnd++;
    } else if (r == '(' || r == ')') {
        DotBeg = buf_lscan(&Buffer, DotEnd, '(');
        DotEnd = buf_rscan(&Buffer, DotEnd, ')');
        if (Buffer.insert_mode) DotEnd++;
    } else if (r == '[' || r == ']') {
        DotBeg = buf_lscan(&Buffer, DotEnd, '[');
        DotEnd = buf_rscan(&Buffer, DotEnd, ']');
        if (Buffer.insert_mode) DotEnd++;
    } else if (r == '{' || r == '}') {
        DotBeg = buf_lscan(&Buffer, DotEnd, '{');
        DotEnd = buf_rscan(&Buffer, DotEnd, '}');
        if (Buffer.insert_mode) DotEnd++;
    } else {
        /* scan for big word */
    }
}

void scrollup(MouseEvent* mevnt) {
    (void)mevnt;
    DotBeg = DotEnd = buf_byline(&Buffer, DotEnd, -ScrollLines);
}

void scrolldn(MouseEvent* mevnt) {
    (void)mevnt;
    DotBeg = DotEnd = buf_byline(&Buffer, DotEnd, ScrollLines);
}

/*****************************************************************************/

enum {
    SINGLE_CLICK = 0,
    DOUBLE_CLICK,
    TRIPLE_CLICK
};

struct {
    uint32_t time;
    uint32_t count;
} Buttons[5] = { {0,0}, {0,0}, {0,0} };

void (*Actions[5][3])(MouseEvent* mevnt) = {
    [MOUSE_LEFT] = {
        [SINGLE_CLICK] = move_cursor,
        [DOUBLE_CLICK] = select,
        [TRIPLE_CLICK] = unused,
    },
    [MOUSE_MIDDLE] = {
        [SINGLE_CLICK] = unused,
        [DOUBLE_CLICK] = unused,
        [TRIPLE_CLICK] = unused,
    },
    [MOUSE_RIGHT] = {
        [SINGLE_CLICK] = unused,
        [DOUBLE_CLICK] = unused,
        [TRIPLE_CLICK] = unused,
    },
    [MOUSE_WHEELUP] = {
        [SINGLE_CLICK] = scrollup,
        [DOUBLE_CLICK] = scrollup,
        [TRIPLE_CLICK] = scrollup,
    },
    [MOUSE_WHEELDOWN] = {
        [SINGLE_CLICK] = scrolldn,
        [DOUBLE_CLICK] = scrolldn,
        [TRIPLE_CLICK] = scrolldn,
    },
};

static void handle_click(MouseEvent* mevnt) {
    if (mevnt->button >= 5) return;
    /* update the number of clicks */
    uint32_t now = getmillis();
    uint32_t elapsed = now - Buttons[mevnt->button].time;
    if (elapsed <= 250)
        Buttons[mevnt->button].count++;
    else
        Buttons[mevnt->button].count = 1;
    Buttons[mevnt->button].time = now;
    /* execute the click action */
    uint32_t nclicks = Buttons[mevnt->button].count;
    nclicks = (nclicks > 3 ? 1 : nclicks);
    Actions[mevnt->button][nclicks-1](mevnt);
}

static void handle_drag(MouseEvent* mevnt) {
    if (mevnt->y == 0 || mevnt->button != MOUSE_LEFT) return;
    DotEnd = screen_getoff(&Buffer, DotEnd, mevnt->y-1, mevnt->x);
    TargetCol = buf_getcol(&Buffer, DotEnd);
}

void handle_mouse(MouseEvent* mevnt) {
    if (mevnt->type == MouseDown) {
        handle_click(mevnt);
    } else if (mevnt->type == MouseMove) {
        handle_drag(mevnt);
    }
}
