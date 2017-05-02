#include <stdc.h>
#include <utf.h>
#include <edit.h>
#include <x11.h>
#include <win.h>
#include <ctype.h>

typedef struct {
    uint64_t time;
    uint8_t count;
    bool pressed;
    int region;
} ButtonState;

static void draw_glyphs(size_t x, size_t y, UGlyph* glyphs, size_t rlen, size_t ncols);
static WinRegion getregion(size_t x, size_t y);
static void onredraw(int height, int width);
static void oninput(int mods, Rune key);
static void onmouse(MouseAct act, MouseBtn btn, int x, int y);
static void onshutdown(void);
static void onwheelup(WinRegion id, size_t count, size_t row, size_t col);
static void onwheeldn(WinRegion id, size_t count, size_t row, size_t col);

static size_t Ruler = 0;
static double ScrollOffset = 0.0;
static double ScrollVisible = 1.0;
static XFont Font;
static XConfig Config = {
    .redraw       = onredraw,
    .handle_key   = oninput,
    .handle_mouse = onmouse,
    .shutdown     = onshutdown,
    .palette      = COLOR_PALETTE
};

void (*MouseActs[MOUSE_BTN_COUNT])(WinRegion id, size_t count, size_t row, size_t col) = {
    [MOUSE_BTN_LEFT]      = onmouseleft,
    [MOUSE_BTN_MIDDLE]    = onmousemiddle,
    [MOUSE_BTN_RIGHT]     = onmouseright,
    [MOUSE_BTN_WHEELUP]   = onwheelup,
    [MOUSE_BTN_WHEELDOWN] = onwheeldn,
};

static WinRegion Focused = EDIT;
static Region Regions[NREGIONS] = {0};
static ButtonState MouseBtns[MOUSE_BTN_COUNT] = {0};
KeyBinding* Keys = NULL;

static void win_init(void) {
    for (int i = 0; i < SCROLL; i++)
        view_init(&(Regions[i].view), NULL);
    x11_init(&Config);
    Font = x11_font_load(FONTNAME);
}

void win_window(char* name) {
    win_init();
    x11_window(name, Width, Height);
}

void win_dialog(char* name) {
    win_init();
    x11_dialog(name, Width, Height);
}

static bool update_focus(void) {
    static int prev_x = 0, prev_y = 0;
    int ptr_x, ptr_y;
    bool changed = true;
    x11_mouse_get(&ptr_x, &ptr_y);
    if (prev_x != ptr_x || prev_y != ptr_y)
        changed = win_setregion(getregion(ptr_x, ptr_y));
    prev_x = ptr_x, prev_y = ptr_y;
    return changed;
}

void win_loop(void) {
    x11_show();
    x11_flip();
    while (x11_running()) {
        bool pending = x11_events_await(200 /* ms */);
        if (update_focus() || pending) {
            x11_events_take();
            x11_flip();
        }
        x11_flush();
    }
    x11_finish();
}

void win_settext(WinRegion id, char* text) {
    View* view = win_view(id);
    view->buffer.gapstart = view->buffer.bufstart;
    view->buffer.gapend   = view->buffer.bufend;
    view->selection = (Sel){0,0,0};
    view_putstr(view, text);
    view_selprev(view); // clear the selection
    buf_logclear(&(view->buffer));
}

void win_setruler(size_t ruler) {
    Ruler = ruler;
}

void win_setkeys(KeyBinding* bindings) {
    Keys = bindings;
}

bool win_btnpressed(MouseBtn btn) {
    return MouseBtns[btn].pressed;
}

WinRegion win_getregion(void) {
    return Focused;
}

bool win_setregion(WinRegion id) {
    bool changed = true;
    if (Focused != id && (id == TAGS || id == EDIT))
        changed = true, Focused = id;
    return changed;
}

void win_warpptr(WinRegion id) {
    Regions[id].warp_ptr = true;
}

View* win_view(WinRegion id) {
    if (id == FOCUSED) id = Focused;
    return &(Regions[id].view);
}

Buf* win_buf(WinRegion id) {
    if (id == FOCUSED) id = Focused;
    return &(Regions[id].view.buffer);
}

Sel* win_sel(WinRegion id) {
    if (id == FOCUSED) id = Focused;
    return &(Regions[id].view.selection);
}

void win_setscroll(double offset, double visible) {
    ScrollOffset  = offset;
    ScrollVisible = visible;
}

static void layout(int width, int height) {
    size_t fheight = x11_font_height(Font);
    size_t fwidth  = x11_font_width(Font);
    View* statview = win_view(STATUS);
    View* tagview  = win_view(TAGS);
    View* editview = win_view(EDIT);
    
    /* update the text views and region positions and sizes */
    for (int i = 0; i < SCROLL; i++) {
        Regions[i].x      = 2;
        Regions[i].y      = 2;
        Regions[i].csrx   = SIZE_MAX;
        Regions[i].csry   = SIZE_MAX;
        Regions[i].width  = (width - 4);
        Regions[i].height = fheight;
    }
    
    /* place the status region */
    view_resize(statview, 1, Regions[STATUS].width / fwidth);
    view_update(statview, &(Regions[STATUS].csrx), &(Regions[STATUS].csry));
    
    /* Place the tag region relative to status */
    Regions[TAGS].y = 5 + Regions[STATUS].y + Regions[STATUS].height;
    size_t maxtagrows = ((height - Regions[TAGS].y - 5) / 4) / fheight;
    size_t tagcols    = Regions[TAGS].width / fwidth;
    size_t tagrows    = view_limitrows(tagview, maxtagrows, tagcols);
    Regions[TAGS].height = tagrows * fheight;
    view_resize(tagview, tagrows, tagcols);
    view_update(tagview, &(Regions[TAGS].csrx), &(Regions[TAGS].csry));
    
    /* Place the scroll region relative to tags */
    Regions[SCROLL].x      = 0;
    Regions[SCROLL].y      = 5 + Regions[TAGS].y + Regions[TAGS].height;
    Regions[SCROLL].height = (height - Regions[EDIT].y - 5);
    Regions[SCROLL].width  = 5 + fwidth;
    
    /* Place the edit region relative to tags */
    Regions[EDIT].x      = 3 + Regions[SCROLL].width;
    Regions[EDIT].y      = 5 + Regions[TAGS].y + Regions[TAGS].height;
    Regions[EDIT].height = (height - Regions[EDIT].y - 5);
    Regions[EDIT].width  = width - Regions[SCROLL].width - 5; 
    view_resize(editview, Regions[EDIT].height / fheight, Regions[EDIT].width / fwidth);
    view_update(editview, &(Regions[EDIT].csrx), &(Regions[EDIT].csry));
}

static void onredraw(int width, int height) {
    size_t fheight = x11_font_height(Font);
    size_t fwidth  = x11_font_width(Font);
    
    /* layout and draw the three text regions */
    onupdate(); // Let the user program update the status and such
    layout(width, height);
    onupdate(); // Let the user program update the status and such
    
    for (int i = 0; i < SCROLL; i++) {
        View* view = win_view(i);
        x11_draw_rect((i == TAGS ? CLR_BASE02 : CLR_BASE03), 
            0, Regions[i].y - 3, width, Regions[i].height + 8);
        x11_draw_rect(CLR_BASE01, 0, Regions[i].y - 3, width, 1);
        if ((i == EDIT) && (Ruler != 0))
            x11_draw_rect(CLR_BASE02, (Ruler+2) * fwidth, Regions[i].y-2, 1, Regions[i].height+7);
        for (size_t y = 0; y < view->nrows; y++) {
            Row* row = view_getrow(view, y);
            draw_glyphs(Regions[i].x, Regions[i].y + ((y+1) * fheight), row->cols, row->rlen, row->len);
        }
    }
    
    /* draw the scroll region */
    size_t thumbreg = (Regions[SCROLL].height - Regions[SCROLL].y + 9);
    size_t thumboff = (size_t)((thumbreg * ScrollOffset) + (Regions[SCROLL].y - 2));
    size_t thumbsz  = (size_t)(thumbreg * ScrollVisible);
    if (thumbsz < 5) thumbsz = 5;
    x11_draw_rect(CLR_BASE01, Regions[SCROLL].width, Regions[SCROLL].y - 2, 1, Regions[SCROLL].height);
    x11_draw_rect(CLR_BASE00, 0, Regions[SCROLL].y - 2, Regions[SCROLL].width, thumbreg);
    x11_draw_rect(CLR_BASE03, 0, thumboff, Regions[SCROLL].width, thumbsz);
    
    /* place the cursor on screen */
    if (Regions[Focused].csrx != SIZE_MAX && Regions[Focused].csry != SIZE_MAX) {
        x11_draw_rect(CLR_BASE3, 
            Regions[Focused].x + (Regions[Focused].csrx * fwidth), 
            Regions[Focused].y + (Regions[Focused].csry * fheight), 
            1, fheight);
    }
    
    /* adjust the mouse location */
    if (Regions[Focused].warp_ptr) {
        Regions[Focused].warp_ptr = false;
        size_t x = Regions[Focused].x + (Regions[Focused].csrx * fwidth)  - (fwidth/2);
        size_t y = Regions[Focused].y + (Regions[Focused].csry * fheight) + (fheight/2);
        x11_mouse_set(x, y);
    }
}

static void oninput(int mods, Rune key) {
    /* mask of modifiers we don't care about */
    mods = mods & (ModCtrl|ModAlt|ModShift);
    /* handle the proper line endings */
    if (key == '\r') key = '\n';
    if (key == '\n' && win_view(FOCUSED)->buffer.crlf) key = RUNE_CRLF;
    /* search for a key binding entry */
    uint32_t mkey = tolower(key);
    for (KeyBinding* bind = Keys; bind && bind->key; bind++) {
        if ((mkey == bind->key) && (bind->mods == ModAny || bind->mods == mods)) {
            bind->action();
            return;
        }
    }
    
    /* fallback to just inserting the rune if it doesn't fall in the private use area.
     * the private use area is used to encode special keys */
    if (key < 0xE000 || key > 0xF8FF)
        view_insert(win_view(FOCUSED), true, key);
}

static void onclick(MouseAct act, MouseBtn btn, int x, int y) {
    WinRegion id = getregion(x, y);
    size_t row = (y-Regions[id].y) / x11_font_height(Font);
    size_t col = (x-Regions[id].x) / x11_font_width(Font);
    if (id == SCROLL) {
        id = EDIT;
        switch (btn) {
            case MOUSE_BTN_LEFT:
                view_scroll(win_view(EDIT), -row);
                break;
            case MOUSE_BTN_MIDDLE: 
                onscroll((double)(y - Regions[SCROLL].y) /
                         (double)(Regions[SCROLL].height - Regions[SCROLL].y));
                break;
            case MOUSE_BTN_RIGHT:
                view_scroll(win_view(EDIT), +row); 
                break;
            case MOUSE_BTN_WHEELUP:
                view_scroll(win_view(id), -ScrollLines);
                break;
            case MOUSE_BTN_WHEELDOWN:
                view_scroll(win_view(id), +ScrollLines);
                break;
            default:
                break;
        }
    } else if (MouseActs[btn]) {
        MouseActs[btn](MouseBtns[btn].region, MouseBtns[btn].count, row, col);
    }
}

static void onmouse(MouseAct act, MouseBtn btn, int x, int y) {
    WinRegion id = getregion(x, y);
    size_t row = (y-Regions[id].y) / x11_font_height(Font);
    size_t col = (x-Regions[id].x) / x11_font_width(Font);
    if (act == MOUSE_ACT_MOVE) {
        if (MouseBtns[MOUSE_BTN_LEFT].pressed) {
            WinRegion selid = MouseBtns[MOUSE_BTN_LEFT].region;
            if (MouseBtns[MOUSE_BTN_LEFT].count == 1) {
                view_setcursor(win_view(selid), row, col);
                MouseBtns[MOUSE_BTN_LEFT].count = 0;
            } else {
                view_selext(win_view(selid), row, col);
            }
        }
    } else {
        MouseBtns[btn].pressed = (act == MOUSE_ACT_DOWN);
        if (MouseBtns[btn].pressed) {
            /* update the number of clicks and click time */
            uint32_t now = getmillis();
            uint32_t elapsed = now - MouseBtns[btn].time;
            MouseBtns[btn].time = now;
            MouseBtns[btn].region = id;
            if (elapsed <= 250)
                MouseBtns[btn].count++;
            else
                MouseBtns[btn].count = 1;
        } else if (MouseBtns[btn].count > 0) {
            onclick(act, btn, x, y);
        }
    }
}

static void onshutdown(void) {
    x11_deinit();
}

static void onwheelup(WinRegion id, size_t count, size_t row, size_t col) {
    view_scroll(win_view(id), -ScrollLines);
}

static void onwheeldn(WinRegion id, size_t count, size_t row, size_t col) {
    view_scroll(win_view(id), +ScrollLines);
}

/*****************************************************************************/

static void draw_glyphs(size_t x, size_t y, UGlyph* glyphs, size_t rlen, size_t ncols) {
    XGlyphSpec specs[rlen];
    size_t i = 0;
    while (rlen && i < ncols) {
        int numspecs = 0;
        uint32_t attr = glyphs[i].attr;
        while (i < ncols && glyphs[i].attr == attr) {
            x11_font_getglyph(Font, &(specs[numspecs]), glyphs[i].rune);
            specs[numspecs].x = x;
            specs[numspecs].y = y - x11_font_descent(Font);
            x += x11_font_width(Font);
            numspecs++;
            i++;
            /* skip over null chars which mark multi column runes */
            for (; i < ncols && !glyphs[i].rune; i++)
                x += x11_font_width(Font);
        }
        /* Draw the glyphs with the proper colors */
        uint8_t bg = attr >> 8;
        uint8_t fg = attr & 0xFF;
        x11_draw_glyphs(fg, bg, specs, numspecs);
        rlen -= numspecs;
    }
}

static WinRegion getregion(size_t x, size_t y) {
    for (int i = 0; i < NREGIONS; i++) {
        size_t startx = Regions[i].x, endx = startx + Regions[i].width;
        size_t starty = Regions[i].y, endy = starty + Regions[i].height;
        if ((startx <= x && x <= endx) && (starty <= y && y <= endy))
            return (WinRegion)i;
    }
    return NREGIONS;
}

