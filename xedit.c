#include <stdc.h>
#include <x11.h>
#include <utf.h>
#include <edit.h>
#include <ctype.h>

enum RegionId {
    STATUS   = 0,
    TAGS     = 1,
    EDIT     = 2,
    NREGIONS = 3
};

// Input Handlers
static void mouse_handler(MouseAct act, MouseBtn btn, int x, int y);
static void tag_handler(char* cmd, char* arg);
static void key_handler(Rune key);

// Drawing Routines
static void draw_runes(size_t x, size_t y, int fg, int bg, UGlyph* glyphs, size_t rlen);
static void draw_glyphs(size_t x, size_t y, UGlyph* glyphs, size_t rlen, size_t ncols);
static void draw_status(int fg, size_t ncols);
static void draw_region(enum RegionId id);
static void layout(int width, int height);
static void redraw(int width, int height);

// UI Callbacks
static void delete(void);
static void cursor_up(void);
static void cursor_dn(void);
static void cursor_left(void);
static void cursor_right(void);
static void cursor_bol(void);
static void cursor_eol(void);
static void page_up(void);
static void page_dn(void);
static void select_prev(void);
static void change_focus(void);
static void quit(void);
static void save(void);
static void undo(void);
static void redo(void);
static void cut(void);
static void copy(void);
static void paste(void);
static void search(void);
static void find(char* arg);

// Mouse Handling
static void mouse_left(enum RegionId id, size_t count, size_t row, size_t col);
static void mouse_middle(enum RegionId id, size_t count, size_t row, size_t col);
static void mouse_right(enum RegionId id, size_t count, size_t row, size_t col);
static void mouse_wheelup(enum RegionId id, size_t count, size_t row, size_t col);
static void mouse_wheeldn(enum RegionId id, size_t count, size_t row, size_t col);

// Region Utils
static View* getview(enum RegionId id);
static Buf* getbuf(enum RegionId id);
static View* currview(void);
static Buf* currbuf(void);
static enum RegionId getregion(size_t x, size_t y);

/* Global Data
 *****************************************************************************/
static enum RegionId Focused = EDIT;
static Region Regions[NREGIONS] = { 0 };
static ButtonState MouseBtns[MOUSE_BTN_COUNT] = { 0 };
static XFont Font;
static XConfig Config = {
    .redraw       = redraw,
    .handle_key   = key_handler,
    .handle_mouse = mouse_handler,
    .palette      = COLOR_PALETTE
};

Tag Builtins[] = {
    { .tag = "Quit",  .action.noarg = quit  },
    { .tag = "Save",  .action.noarg = save  },
    { .tag = "Cut",   .action.noarg = cut   },
    { .tag = "Copy",  .action.noarg = copy  },
    { .tag = "Paste", .action.noarg = paste },
    { .tag = "Undo",  .action.noarg = undo  },
    { .tag = "Redo",  .action.noarg = redo  },
    { .tag = "Find",  .action.arg   = find  },
    { .tag = NULL,    .action.noarg = NULL  }
};

void (*MouseActs[MOUSE_BTN_COUNT])(enum RegionId id, size_t count, size_t row, size_t col) = {
    [MOUSE_BTN_LEFT]      = mouse_left,
    [MOUSE_BTN_MIDDLE]    = mouse_middle,
    [MOUSE_BTN_RIGHT]     = mouse_right,
    [MOUSE_BTN_WHEELUP]   = mouse_wheelup,
    [MOUSE_BTN_WHEELDOWN] = mouse_wheeldn,
};

static KeyBinding Insert[] = {
    { KEY_DELETE,    delete       },
    { KEY_UP,        cursor_up    },
    { KEY_DOWN,      cursor_dn    },
    { KEY_LEFT,      cursor_left  },
    { KEY_RIGHT,     cursor_right },
    { KEY_HOME,      cursor_bol   },
    { KEY_END,       cursor_eol   },
    { KEY_PGUP,      page_up      },
    { KEY_PGDN,      page_dn      },
    { KEY_ESCAPE,    select_prev  },
    { KEY_CTRL_T,    change_focus },
    { KEY_CTRL_Q,    quit         },
    { KEY_CTRL_S,    save         },
    { KEY_CTRL_Z,    undo         },
    { KEY_CTRL_Y,    redo         },
    { KEY_CTRL_X,    cut          },
    { KEY_CTRL_C,    copy         },
    { KEY_CTRL_V,    paste        },
    { KEY_CTRL_F,    search       },
    { 0,             NULL         }
};

/* External Commands
 *****************************************************************************/
#ifdef __MACH__
static char* CopyCmd[]  = { "pbcopy", NULL };
static char* PasteCmd[] = { "pbpaste", NULL };
#else
static char* CopyCmd[]  = { "xsel", "-bi", NULL };
static char* PasteCmd[] = { "xsel", "-bo", NULL };
#endif

/* Main Routine
 *****************************************************************************/
int main(int argc, char** argv) {
    /* load the buffer views */
    view_init(getview(TAGS), NULL);
    view_putstr(getview(TAGS), DEFAULT_TAGS);
    view_init(getview(EDIT), (argc > 1 ? argv[1] : NULL));
    /* initialize the display engine */
    x11_init(&Config);
    x11_window("edit", Width, Height);
    x11_show();
    Font = x11_font_load(FONTNAME);
    x11_loop();
    return 0;
}

static void mouse_handler(MouseAct act, MouseBtn btn, int x, int y) {
    enum RegionId id = getregion(x, y);
    if (id != TAGS && id != EDIT) return;
    if (Focused != id) Focused = id;
    size_t row = (y-Regions[id].y) / x11_font_height(Font);
    size_t col = (x-Regions[id].x) / x11_font_width(Font);
    if (act == MOUSE_ACT_MOVE) {
        if (MouseBtns[MOUSE_BTN_LEFT].pressed) {
            view_setcursor(getview(id), row, col);
            MouseBtns[MOUSE_BTN_LEFT].pressed = false;
            MouseBtns[MOUSE_BTN_LEFT].count = 0;
        } else if (MouseBtns[MOUSE_BTN_LEFT].region < id) {
            //view_scroll(getview(MouseBtns[MOUSE_BTN_LEFT].region), +1);
        } else if (MouseBtns[MOUSE_BTN_LEFT].region > id) {
            //view_scroll(getview(MouseBtns[MOUSE_BTN_LEFT].region), -1);
        } else {
            view_selext(getview(id), row, col);
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
            MouseActs[btn](id, MouseBtns[btn].count, row, col);
        }
    }
}

static void tag_handler(char* tag, char* arg) {
    Tag* tags = Builtins;
    while (tags->tag) {
        if (!strcmp(tags->tag, tag)) {
            Focused = EDIT;
            tags->action.arg(arg);
            break;
        }
        tags++;
    }
}

static void key_handler(Rune key) {
    /* ignore invalid keys */
    if (key == RUNE_ERR) return;
    /* handle the proper line endings */
    if (key == '\r') key = '\n';
    if (key == '\n' && currview()->buffer.crlf) key = RUNE_CRLF;
    /* handle the key */
    KeyBinding* bindings = Insert;
    while (bindings->key) {
        if (key == bindings->key) {
            bindings->action();
            return;
        }
        bindings++;
    }
    /* fallback to just inserting the rune if it doesnt fall in the private use area.
     * the private use area is used to encode special keys */
    if (key < 0xE000 || key > 0xF8FF)
        view_insert(currview(), key);
}

/* Drawing Routines
 *****************************************************************************/
static void draw_runes(size_t x, size_t y, int fg, int bg, UGlyph* glyphs, size_t rlen) {
    XGlyphSpec specs[rlen];
    while (rlen) {
        size_t nspecs = x11_font_getglyphs(specs, (XGlyph*)glyphs, rlen, Font, x, y);
        x11_draw_glyphs(fg, bg, specs, nspecs);
        rlen -= nspecs;
    }
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

static void draw_status(int fg, size_t ncols) {
    Buf* buf = getbuf(EDIT);
    UGlyph glyphs[ncols], *status = glyphs;
    (status++)->rune = (buf->charset == BINARY ? 'B' : 'U');
    (status++)->rune = (buf->crlf ? 'C' : 'N');
    (status++)->rune = (buf->modified ? '*' : ' ');
    (status++)->rune = ' ';
    char* path = (buf->path ? buf->path : "*scratch*");
    size_t len = strlen(path);
    if (len > ncols-4) {
        (status++)->rune = '.';
        (status++)->rune = '.';
        (status++)->rune = '.';
        path += (len - ncols) + 6;
    }
    while (*path)
        (status++)->rune = *path++;
    draw_runes(2, 2, fg, 0, glyphs, status - glyphs);
}

static void draw_region(enum RegionId id) {
    size_t fheight = x11_font_height(Font);
    size_t fwidth  = x11_font_width(Font);
    /* update the screen buffer and retrieve cursor coordinates */
    View* view = getview(id);
    size_t csrx = SIZE_MAX, csry = SIZE_MAX;
    view_update(view, &csrx, &csry);
    /* draw the region to the frame buffer */
    if (id == TAGS)
        x11_draw_rect(CLR_BASE02, Regions[id].x-2, Regions[id].y-2, Regions[id].width+4, Regions[id].height+4);
    x11_draw_rect(CLR_BASE01, 0, Regions[id].y - 3, Regions[id].width + 4, 1);
    for (size_t y = 0; y < view->nrows; y++) {
        Row* row = view_getrow(view, y);
        draw_glyphs(2, Regions[id].y + ((y+1) * fheight), row->cols, row->rlen, row->len);
    }
    /* Place cursor on screen */
    if (id == Focused && csrx != SIZE_MAX && csry != SIZE_MAX)
        x11_draw_rect(CLR_BASE3, 2 + csrx * fwidth, Regions[id].y + (csry * fheight), 1, fheight);
}

static void layout(int width, int height) {
    /* initialize all of the regions to overlap the status region */
    size_t fheight = x11_font_height(Font);
    size_t fwidth  = x11_font_width(Font);
    for (int i = 0; i < NREGIONS; i++) {
        Regions[i].x      = 2;
        Regions[i].y      = 2;
        Regions[i].width  = (width - 4);
        Regions[i].height = fheight;
    }
    /* Place the tag region relative to status */
    Regions[TAGS].y = 5 + Regions[STATUS].y + Regions[STATUS].height;
    size_t maxtagrows = ((height - Regions[TAGS].y - 5) / 4) / fheight;
    size_t tagcols    = Regions[TAGS].width / fwidth;
    size_t tagrows    = view_limitrows(getview(TAGS), maxtagrows, tagcols);
    Regions[TAGS].height = tagrows * fheight;
    view_resize(getview(TAGS), tagrows, tagcols);
    /* Place the edit region relative to status */
    Regions[EDIT].y      = 5 + Regions[TAGS].y + Regions[TAGS].height;
    Regions[EDIT].height = (height - Regions[EDIT].y - 5);
    view_resize(getview(EDIT), Regions[EDIT].height / fheight, Regions[EDIT].width / fwidth);
}

static void redraw(int width, int height) {
    size_t fheight = x11_font_height(Font);
    size_t fwidth  = x11_font_width(Font);
    /* if the window is too small, don't bother updating. */
    if (width < fwidth || height < (4 * fheight))
        return;
    layout(width, height);
    x11_draw_rect(CLR_BASE03, 0, 0, width, height);
    x11_draw_rect(CLR_BASE02, (79 * fwidth) + 2, Regions[EDIT].y-2, fwidth, height - Regions[EDIT].y + 2);
    draw_status(CLR_BASE1, (width - 4) / x11_font_width(Font));
    draw_region(TAGS);
    draw_region(EDIT);
}

/* UI Callbacks
 *****************************************************************************/
static void delete(void) {
    view_delete(currview());
}

static void cursor_up(void) {
    view_byline(currview(), -1);
}

static void cursor_dn(void) {
    view_byline(currview(), +1);
}

static void cursor_left(void) {
    view_byrune(currview(), -1);
}

static void cursor_right(void) {
    view_byrune(currview(), +1);
}

static void cursor_bol(void) {
    view_bol(currview());
}

static void cursor_eol(void) {
    view_eol(currview());
}

static void page_up(void) {
    view_scrollpage(currview(), -1);
}

static void page_dn(void) {
    view_scrollpage(currview(), +1);
}

static void select_prev(void) {
    view_selprev(currview());
}

static void change_focus(void) {
    Focused = (Focused == TAGS ? EDIT : TAGS);
}

static void quit(void) {
    static uint32_t num_clicks = 0;
    static uint32_t prevtime = 0;
    uint32_t now = getmillis();
    num_clicks = (now - prevtime < 250 ? num_clicks+1 : 1);
    prevtime = now;
    if (!getbuf(EDIT)->modified || num_clicks >= 2)
        exit(0);
}

static void save(void) {
    buf_save(getbuf(EDIT));
}

static void undo(void) {
    view_undo(currview());
}

static void redo(void) {
    view_redo(currview());
}

static void cut(void) {
    char* str = view_getstr(currview(), NULL);
    if (str && *str) {
        cmdwrite(CopyCmd, str);
        delete();
    }
    free(str);
}

static void copy(void) {
    char* str = view_getstr(currview(), NULL);
    if (str && *str)
        cmdwrite(CopyCmd, str);
    free(str);
}

static void paste(void) {
    char* str = cmdread(PasteCmd);
    if (str && *str)
        view_putstr(currview(), str);
    free(str);
}

static void search(void) {
    char* str = view_getstr(currview(), NULL);
    view_findstr(getview(EDIT), str);
    free(str);
    Focused = EDIT;
}

static void find(char* arg) {
    view_findstr(getview(EDIT), arg);
}

/* Mouse Handling
 *****************************************************************************/
static void mouse_left(enum RegionId id, size_t count, size_t row, size_t col) {
    if (count == 1)
        view_setcursor(getview(id), row, col);
    else if (count == 2)
        view_select(getview(id), row, col);
    else if (count == 3)
        view_selword(getview(id), row, col);
}

static void mouse_middle(enum RegionId id, size_t count, size_t row, size_t col) {
    if (MouseBtns[MOUSE_BTN_LEFT].pressed) {
        cut();
    } else {
        char* str = view_fetch(getview(id), row, col);
        char* tag = str;
        if (!str) { return; }
        for (; *tag && isspace(*tag); tag++);
        /* first check if the arg is in the same selection as the tag/cmd */
        char* arg = tag;
        while (*arg && !isspace(*arg++));
        if (*arg) {
            char* temp = stringdup(arg);
            *(arg-1) = '\0', arg = temp;
        } else {
            /* if it isn't then check the tags buffer selection */
            arg = view_getstr(getview(TAGS), NULL);
        }
        /* if we still haven't found it, check the edit buffer selection */
        if (!arg) arg = view_getstr(getview(EDIT), NULL);
        tag_handler(tag, arg);
        free(str);
        free(arg);
    }
}

static void mouse_right(enum RegionId id, size_t count, size_t row, size_t col) {
    if (MouseBtns[MOUSE_BTN_LEFT].pressed)
        paste();
    else
        view_find(getview(id), row, col);
}

static void mouse_wheelup(enum RegionId id, size_t count, size_t row, size_t col) {
    view_scroll(getview(id), -ScrollLines);
}

static void mouse_wheeldn(enum RegionId id, size_t count, size_t row, size_t col) {
    view_scroll(getview(id), +ScrollLines);
}

/* Region Utils
 *****************************************************************************/
static View* getview(enum RegionId id) {
    return &(Regions[id].view);
}

static Buf* getbuf(enum RegionId id) {
    return &(getview(id)->buffer);
}

static View* currview(void) {
    return getview(Focused);
}

static Buf* currbuf(void) {
    return getbuf(Focused);
}

static enum RegionId getregion(size_t x, size_t y) {
    for (int i = 0; i < NREGIONS; i++) {
        size_t startx = Regions[i].x, endx = startx + Regions[i].width;
        size_t starty = Regions[i].y, endy = starty + Regions[i].height;
        if ((startx <= x && x <= endx) && (starty <= y && y <= endy))
            return (enum RegionId)i;
    }
    return NREGIONS;
}

#if 0
void move_pointer(size_t x, size_t y) {
    x = (x * Fonts.base.width)  + (Fonts.base.width / 2);
    y = ((y+1) * Fonts.base.height) + (Fonts.base.height / 2);
    x11_warp_mouse(x,y);
}
#endif
