#include "edit.h"

#ifndef __MACH__
#include <time.h>
#else
#include <sys/time.h>
#define CLOCK_MONOTONIC 0
// clock_gettime is not implemented on OSX
int clock_gettime(int id, struct timespec* t) {
    (void)id;
    struct timeval now;
    int rv = gettimeofday(&now, NULL);
    if (rv) return rv;
    t->tv_sec  = now.tv_sec;
    t->tv_nsec = now.tv_usec * 1000;
    return 0;
}
#endif

uint32_t getmillis(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return ((time.tv_sec * 1000) + (time.tv_nsec / 1000000));
}

/*****************************************************************************/

void unused(MouseEvent* mevnt) {
    (void)mevnt;
}

void move_cursor(MouseEvent* mevnt) {
    CursorPos = screen_getoff(&Buffer, CursorPos, mevnt->y, mevnt->x);
    TargetCol = buf_getcol(&Buffer, CursorPos);
}

void select_word(MouseEvent* mevnt) {
    (void)mevnt;
}

void select_line(MouseEvent* mevnt) {
    (void)mevnt;
}

void scrollup(MouseEvent* mevnt) {
    (void)mevnt;
    CursorPos = buf_byline(&Buffer, CursorPos, -ScrollLines);
}

void scrolldn(MouseEvent* mevnt) {
    (void)mevnt;
    CursorPos = buf_byline(&Buffer, CursorPos, ScrollLines);
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
        [DOUBLE_CLICK] = select_word,
        [TRIPLE_CLICK] = select_line,
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
    (void)mevnt;
}

void handle_mouse(MouseEvent* mevnt) {
    if (mevnt->type == MouseDown) {
        handle_click(mevnt);
    } else if (mevnt->type == MouseMove) {
        handle_drag(mevnt);
    }
}
