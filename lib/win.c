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
static void mouse_wheelup(WinRegion id, size_t count, size_t row, size_t col);
static void mouse_wheeldn(WinRegion id, size_t count, size_t row, size_t col);

static XFont Font;
static XConfig Config = {
    .redraw       = onredraw,
    .handle_key   = oninput,
    .handle_mouse = onmouse,
    .shutdown     = onshutdown,
    .palette      = COLOR_PALETTE
};

void (*MouseActs[MOUSE_BTN_COUNT])(WinRegion id, size_t count, size_t row, size_t col) = {
    [MOUSE_BTN_LEFT]      = mouse_left,
    [MOUSE_BTN_MIDDLE]    = mouse_middle,
    [MOUSE_BTN_RIGHT]     = mouse_right,
    [MOUSE_BTN_WHEELUP]   = mouse_wheelup,
    [MOUSE_BTN_WHEELDOWN] = mouse_wheeldn,
};

static WinRegion Focused = EDIT;
static Region Regions[NREGIONS] = {0};
static ButtonState MouseBtns[MOUSE_BTN_COUNT] = {0};
KeyBinding* Keys = NULL;

void win_init(char* name) {
    for (int i = 0; i < SCROLL; i++)
        view_init(&(Regions[i].view), NULL);
    x11_init(&Config);
    Font = x11_font_load(FONTNAME);
    x11_window(name, Width, Height);
}

void win_loop(void) {
    x11_show();
    x11_loop();
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

void win_setkeys(KeyBinding* bindings) {
    Keys = bindings;
}

bool win_btnpressed(MouseBtn btn) {
    return MouseBtns[btn].pressed;
}

WinRegion win_getregion(void) {
    return Focused;
}

void win_setregion(WinRegion id) {
    Focused = id;
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
    onupdate(); // Let the user program update the status and such
    /* layout and draw the three text regions */
    layout(width, height);
    for (int i = 0; i < SCROLL; i++) {
        View* view = win_view(i);
        x11_draw_rect((i == TAGS ? CLR_BASE02 : CLR_BASE03), 
            0, Regions[i].y - 3, width, Regions[i].height + 8);
        x11_draw_rect(CLR_BASE01, 0, Regions[i].y - 3, width, 1);
        for (size_t y = 0; y < view->nrows; y++) {
            Row* row = view_getrow(view, y);
            draw_glyphs(Regions[i].x, Regions[i].y + ((y+1) * fheight), row->cols, row->rlen, row->len);
        }
    }
    
    /* draw the scroll region */
    x11_draw_rect(CLR_BASE01, Regions[SCROLL].width, Regions[SCROLL].y-2, 1, Regions[SCROLL].height);
    
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
        x11_warp_mouse(x,y);
    }
}

static void oninput(int mods, Rune key) {
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
        } else if (id == TAGS || id == EDIT) {
            Focused = id;
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
            /* execute the action on button release */
            if (MouseActs[btn])
                MouseActs[btn](MouseBtns[btn].region, MouseBtns[btn].count, row, col);
        }
    }
}

static void onshutdown(void) {
    x11_deinit();
}

static void mouse_wheelup(WinRegion id, size_t count, size_t row, size_t col) {
    if (id == SCROLL) id = EDIT;
    view_scroll(win_view(id), -ScrollLines);
}

static void mouse_wheeldn(WinRegion id, size_t count, size_t row, size_t col) {
    if (id == SCROLL) id = EDIT;
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

