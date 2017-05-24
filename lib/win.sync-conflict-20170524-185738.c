#include <stdc.h>
#include <utf.h>
#include <edit.h>
#include <x11.h>
#include <win.h>
#include <ctype.h>

static void onredraw(int height, int width);
static void oninput(int mods, Rune key);
static void onmousedrag(int state, int x, int y);
static void onmousebtn(int btn, bool pressed, int x, int y);
static void onwheelup(WinRegion id, bool pressed, size_t row, size_t col);
static void onwheeldn(WinRegion id, bool pressed, size_t row, size_t col);
static void draw_glyphs(size_t x, size_t y, UGlyph* glyphs, size_t rlen, size_t ncols);
static WinRegion getregion(size_t x, size_t y);

static size_t Ruler = 0;
static double ScrollOffset = 0.0;
static double ScrollVisible = 1.0;
static XFont Font;
static XConfig Config = {
    .redraw       = onredraw,
    .handle_key   = oninput,
    .shutdown     = onshutdown,
    .set_focus    = onfocus,
    .mouse_drag   = onmousedrag,
    .mouse_btn    = onmousebtn,
    .palette      = COLOR_PALETTE
};
static WinRegion Focused = EDIT;
static Region Regions[NREGIONS] = {0};
KeyBinding* Keys = NULL;

static void win_init(void (*errfn)(char*)) {
    for (int i = 0; i < SCROLL; i++)
        view_init(&(Regions[i].view), NULL, errfn);
    x11_init(&Config);
    Font = x11_font_load(FONTNAME);
}

void win_window(char* name, void (*errfn)(char*)) {
    win_init(errfn);
    x11_window(name, Width, Height);
}

void win_dialog(char* name, void (*errfn)(char*)) {
    win_init(errfn);
    x11_dialog(name, Width, Height);
}

static bool update_focus(void) {
    static int prev_x = 0, prev_y = 0;
    int ptr_x, ptr_y;
    bool changed = false;
    /* dont change focus if any mouse buttons are pressed */
    if ((x11_keybtnstate() & 0x1f00) == 0) {
        x11_mouse_get(&ptr_x, &ptr_y);
        if (prev_x != ptr_x || prev_y != ptr_y)
            changed = win_setregion(getregion(ptr_x, ptr_y));
        prev_x = ptr_x, prev_y = ptr_y;
    }
    return changed;
}

void win_loop(void) {
    x11_show();
    x11_flip();
    while (x11_running()) {
        bool pending = x11_events_await(EventTimeout);
        int nevents  = x11_events_queued();
        if (update_focus() || pending || nevents) {
            x11_events_take();
            if (x11_running())
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

bool win_btnpressed(int btn) {
    int btnmask = (1 << (btn + 7));
    return ((x11_keybtnstate() & btnmask) == btnmask);
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

    /* Place the tag region relative to status */
    Regions[TAGS].y = 5 + Regions[STATUS].y + Regions[STATUS].height;
    size_t maxtagrows = ((height - Regions[TAGS].y - 5) / 4) / fheight;
    size_t tagcols    = Regions[TAGS].width / fwidth;
    size_t tagrows    = view_limitrows(tagview, maxtagrows, tagcols);
    Regions[TAGS].height = tagrows * fheight;
    view_resize(tagview, tagrows, tagcols);

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
}

static void onredraw(int width, int height) {
    size_t fheight = x11_font_height(Font);
    size_t fwidth  = x11_font_width(Font);

    layout(width, height);
    onupdate(); // Let the user program update the status and other content
    view_update(win_view(STATUS), &(Regions[STATUS].csrx), &(Regions[STATUS].csry));
    view_update(win_view(TAGS), &(Regions[TAGS].csrx), &(Regions[TAGS].csry));
    view_update(win_view(EDIT), &(Regions[EDIT].csrx), &(Regions[EDIT].csry));
    onlayout(); // Let the user program update the scroll bar

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
    if (key < 0xE000 || key > 0xF8FF) {
        if (key == '\n' && win_view(FOCUSED)->buffer.crlf)
            key = RUNE_CRLF;
        view_insert(win_view(FOCUSED), true, key);
    }
}

static void scroll_actions(int btn, bool pressed, int x, int y) {
    size_t row = (y-Regions[SCROLL].y) / x11_font_height(Font);
    size_t col = (x-Regions[SCROLL].x) / x11_font_width(Font);
    switch (btn) {
        case MouseLeft:
            if (pressed)
                view_scroll(win_view(EDIT), -row);
            break;
        case MouseMiddle:
            if (pressed)
                onscroll((double)(y - Regions[SCROLL].y) /
                         (double)(Regions[SCROLL].height - Regions[SCROLL].y));
            break;
        case MouseRight:
            if (pressed)
                view_scroll(win_view(EDIT), +row);
            break;
        case MouseWheelUp:
            view_scroll(win_view(EDIT), -ScrollLines);
            break;
        case MouseWheelDn:
            view_scroll(win_view(EDIT), +ScrollLines);
            break;
    }
}

static void onmousedrag(int state, int x, int y) {
    WinRegion id = getregion(x, y);
    size_t row = (y-Regions[id].y) / x11_font_height(Font);
    size_t col = (x-Regions[id].x) / x11_font_width(Font);
    if (id == Focused && win_btnpressed(MouseLeft))
        view_selext(win_view(id), row, col);
}

static void onmousebtn(int btn, bool pressed, int x, int y) {
    WinRegion id = getregion(x, y);
    size_t row = (y-Regions[id].y) / x11_font_height(Font);
    size_t col = (x-Regions[id].x) / x11_font_width(Font);

    if (id == SCROLL) {
        scroll_actions(btn, pressed, x, y);
    } else {
        switch(btn) {
            case MouseLeft:    onmouseleft(id, pressed, row, col);   break;
            case MouseMiddle:  onmousemiddle(id, pressed, row, col); break;
            case MouseRight:   onmouseright(id, pressed, row, col);  break;
            case MouseWheelUp: onwheelup(id, pressed, row, col);     break;
            case MouseWheelDn: onwheeldn(id, pressed, row, col);     break;
        }
    }
}

static void onwheelup(WinRegion id, bool pressed, size_t row, size_t col) {
    if (!pressed) return;
    view_scroll(win_view(id), -ScrollLines);
}

static void onwheeldn(WinRegion id, bool pressed, size_t row, size_t col) {
    if (!pressed) return;
    view_scroll(win_view(id), +ScrollLines);
}

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
