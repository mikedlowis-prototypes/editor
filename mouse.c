#include "edit.h"
#include <ctype.h>

void unused(MouseEvent* mevnt) {
    (void)mevnt;
}

void move_cursor(MouseEvent* mevnt) {
    if (mevnt->y == 0) return;
    SelBeg = SelEnd = screen_getoff(&Buffer, SelEnd, mevnt->y-1, mevnt->x);
    TargetCol = buf_getcol(&Buffer, SelEnd);
}

void bigword(MouseEvent* mevnt) {
    (void)mevnt;
    unsigned mbeg = SelEnd, mend = SelEnd;
    for (; !isblank(buf_get(&Buffer, mbeg-1)); mbeg--);
    for (; !isblank(buf_get(&Buffer, mend)); mend++);
    SelBeg = mbeg, SelEnd = mend-1;
}

void selection(MouseEvent* mevnt) {
    (void)mevnt;
    unsigned bol = buf_bol(&Buffer, SelEnd);
    Rune r = buf_get(&Buffer, SelEnd);
    if (SelEnd == bol || r == '\n' || r == RUNE_CRLF) {
        SelBeg = bol;
        SelEnd = buf_eol(&Buffer, SelEnd);
    } else if (isword(r)) {
        SelBeg = buf_bow(&Buffer, SelEnd);
        SelEnd = buf_eow(&Buffer, SelEnd);
        if (Buffer.insert_mode) SelEnd++;
    } else if (r == '(' || r == ')') {
        SelBeg = buf_lscan(&Buffer, SelEnd, '(');
        SelEnd = buf_rscan(&Buffer, SelEnd, ')');
        if (Buffer.insert_mode) SelEnd++;
    } else if (r == '[' || r == ']') {
        SelBeg = buf_lscan(&Buffer, SelEnd, '[');
        SelEnd = buf_rscan(&Buffer, SelEnd, ']');
        if (Buffer.insert_mode) SelEnd++;
    } else if (r == '{' || r == '}') {
        SelBeg = buf_lscan(&Buffer, SelEnd, '{');
        SelEnd = buf_rscan(&Buffer, SelEnd, '}');
        if (Buffer.insert_mode) SelEnd++;
    } else {
        bigword(mevnt);
    }
}

void search(MouseEvent* mevnt) {
    unsigned clickpos = screen_getoff(&Buffer, SelEnd, mevnt->y-1, mevnt->x);
    if (clickpos < SelBeg || clickpos > SelEnd) {
        move_cursor(mevnt);
        selection(mevnt);
    } else {
        buf_find(&Buffer, &SelBeg, &SelEnd);
    }
    unsigned x, y;
    screen_update(&Buffer, SelEnd, &x, &y);
    extern void move_pointer(unsigned x, unsigned y);
    move_pointer(x, y);

}

void scrollup(MouseEvent* mevnt) {
    (void)mevnt;
    SelBeg = SelEnd = buf_byline(&Buffer, SelEnd, -ScrollLines);
}

void scrolldn(MouseEvent* mevnt) {
    (void)mevnt;
    SelBeg = SelEnd = buf_byline(&Buffer, SelEnd, ScrollLines);
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
        [DOUBLE_CLICK] = selection,
        [TRIPLE_CLICK] = bigword,
    },
    [MOUSE_MIDDLE] = {
        [SINGLE_CLICK] = unused,
        [DOUBLE_CLICK] = unused,
        [TRIPLE_CLICK] = unused,
    },
    [MOUSE_RIGHT] = {
        [SINGLE_CLICK] = search,
        [DOUBLE_CLICK] = search,
        [TRIPLE_CLICK] = search,
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
    SelEnd = screen_getoff(&Buffer, SelEnd, mevnt->y-1, mevnt->x);
    TargetCol = buf_getcol(&Buffer, SelEnd);
}

void handle_mouse(MouseEvent* mevnt) {
    if (mevnt->type == MouseDown) {
        handle_click(mevnt);
    } else if (mevnt->type == MouseMove) {
        handle_drag(mevnt);
    }
}
