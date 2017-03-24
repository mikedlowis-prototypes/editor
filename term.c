#include <stdc.h>
#include <x11.h>
#include <utf.h>
#include <edit.h>
#include <ctype.h>
#include <win.h>

/* Mouse Handling
 *****************************************************************************/
void onmouseleft(WinRegion id, size_t count, size_t row, size_t col) {

}

void onmousemiddle(WinRegion id, size_t count, size_t row, size_t col) {

}

void onmouseright(WinRegion id, size_t count, size_t row, size_t col) {

}

//MouseConfig* MouseHandlers[NREGIONS] = {
//    [EDIT] = &(MouseConfig){
//        .left   = mouse_left,
//        .middle = mouse_middle,
//        .right  = mouse_right
//    }
//};

void onscroll(double percent) {

}

/* Keyboard Handling
 *****************************************************************************/

/* Main Routine
 *****************************************************************************/
 
void onupdate(void) {
}

#ifndef TEST
int main(int argc, char** argv) {
    win_init("term");
    //win_setkeys(&Bindings);
    //win_setmouse(&MouseHandlers);
    win_loop();
    return 0;
}
#endif















//static void redraw(int width, int height) {
#if 0
    x11_draw_rect(CLR_BASE03, 0, 0, width, height);
    size_t fheight = x11_font_height(Font);
    size_t fwidth  = x11_font_width(Font);
    /* if the window is too small, don't bother updating. */
    if (width < fwidth || height < (4 * fheight))
        return;
    layout(width, height);
    x11_draw_rect(CLR_BASE03, 0, 0, width, height);
    x11_draw_rect(CLR_BASE02, 0, Regions[EDIT].y-2, fwidth + 4, height - Regions[EDIT].y + 2);

    draw_status(CLR_BASE1, (width - 4) / x11_font_width(Font));
    draw_region(TAGS);
    draw_region(EDIT);
    cmdreap(); // cleanup any zombie child processes
#endif
//}

#if 0
#ifdef TEST
#define exit mockexit
#endif

enum RegionId {
    STATUS   = 0,
    TAGS     = 1,
    EDIT     = 2,
    SCROLL   = 3,
    NREGIONS = 4
};

// Input Handlers
static void mouse_handler(MouseAct act, MouseBtn btn, int x, int y);
static void key_handler(int mods, Rune key);

// Drawing Routines
static void draw_runes(size_t x, size_t y, int fg, int bg, UGlyph* glyphs, size_t rlen);
static void draw_glyphs(size_t x, size_t y, UGlyph* glyphs, size_t rlen, size_t ncols);
static void draw_status(int fg, size_t ncols);
static void draw_region(enum RegionId id);
static void layout(int width, int height);
static void redraw(int width, int height);

// UI Callbacks
static void delete(void);
static void del_to_bol(void);
static void del_to_eol(void);
static void del_to_bow(void);
static void backspace(void);
static void cursor_bol(void);
static void cursor_eol(void);
static void cursor_home(void);
static void cursor_end(void);
static void cursor_up(void);
static void cursor_dn(void);
static void cursor_left(void);
static void cursor_right(void);
static void page_up(void);
static void page_dn(void);
static void change_focus(void);
static void quit(void);
static void undo(void);
static void redo(void);
static void tag_undo(void);
static void tag_redo(void);
static void cut(void);
static void copy(void);
static void paste(void);

// Tag/Cmd Execution
static Tag* tag_lookup(char* cmd);
static void tag_exec(Tag* tag, char* arg);
static void cmd_exec(char* cmd);
static void exec(char* cmd);

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
static Sel* getsel(enum RegionId id);
static Sel* currsel(void);

/* Global Data
 *****************************************************************************/
static int SearchDir = DOWN;
static enum RegionId Focused = EDIT;
static Region Regions[NREGIONS] = { 0 };
static ButtonState MouseBtns[MOUSE_BTN_COUNT] = { 0 };
static XFont Font;
static XConfig Config = {
    .redraw       = redraw,
    .handle_key   = key_handler,
    .handle_mouse = mouse_handler,
    .shutdown     = quit,
    .palette      = COLOR_PALETTE
};

Tag Builtins[] = {
    { .tag = "Quit",   .action.noarg = quit     },
    { .tag = "Cut",    .action.noarg = cut      },
    { .tag = "Copy",   .action.noarg = copy     },
    { .tag = "Paste",  .action.noarg = paste    },
    { .tag = "Undo",   .action.noarg = tag_undo },
    { .tag = "Redo",   .action.noarg = tag_redo },
    { .tag = NULL,     .action.noarg = NULL     }
};

void (*MouseActs[MOUSE_BTN_COUNT])(enum RegionId id, size_t count, size_t row, size_t col) = {
    [MOUSE_BTN_LEFT]      = mouse_left,
    [MOUSE_BTN_MIDDLE]    = mouse_middle,
    [MOUSE_BTN_RIGHT]     = mouse_right,
    [MOUSE_BTN_WHEELUP]   = mouse_wheelup,
    [MOUSE_BTN_WHEELDOWN] = mouse_wheeldn,
};

static KeyBinding Bindings[] = {
    /* Cursor Movements */
    { ModAny, KEY_HOME,  cursor_home  },
    { ModAny, KEY_END,   cursor_end   },
    { ModAny, KEY_UP,    cursor_up    },
    { ModAny, KEY_DOWN,  cursor_dn    },
    { ModAny, KEY_LEFT,  cursor_left  },
    { ModAny, KEY_RIGHT, cursor_right },

    /* Standard Unix Shortcuts */
    { ModCtrl, 'u', del_to_bol  },
    { ModCtrl, 'k', del_to_eol  },
    { ModCtrl, 'w', del_to_bow  },
    { ModCtrl, 'h', backspace   },
    { ModCtrl, 'a', cursor_bol  },
    { ModCtrl, 'e', cursor_eol  },

    /* Standard Text Editing Shortcuts */
    { ModCtrl, 'z', undo  },
    { ModCtrl, 'y', redo  },
    { ModCtrl, 'x', cut   },
    { ModCtrl, 'c', copy  },
    { ModCtrl, 'v', paste },

    /* Common Special Keys */
    { ModNone, KEY_PGUP,      page_up   },
    { ModNone, KEY_PGDN,      page_dn   },
    { ModAny,  KEY_DELETE,    delete    },
    { ModAny,  KEY_BACKSPACE, backspace },

    /* Implementation Specific */
    { ModCtrl, 't', change_focus },
    { ModCtrl, 'q', quit         },
};

/* External Commands
 *****************************************************************************/
/* The shell: Filled in with $SHELL. Used to execute commands */
static char* ShellCmd[] = { NULL, "-c", NULL, NULL };
static char* SedCmd[] = { "sed", "-e", NULL, NULL };

/* Main Routine
 *****************************************************************************/
#ifndef TEST
int main(int argc, char** argv) {
    /* setup the shell */
    ShellCmd[0] = getenv("SHELL");
    if (!ShellCmd[0]) ShellCmd[0] = "/bin/sh";
    /* load the buffer views */
    char* tags = getenv("EDITTAGS");
    view_init(getview(TAGS), NULL);
    view_putstr(getview(TAGS), (tags ? tags : DEFAULT_TAGS));
    view_selprev(getview(TAGS)); // clear the selection
    buf_logclear(getbuf(TAGS));
    view_init(getview(EDIT), (argc > 1 ? argv[1] : NULL));
    /* initialize the display engine */
    x11_init(&Config);
    x11_window("edit", Width, Height);
    x11_show();
    Font = x11_font_load(FONTNAME);
    x11_loop();
    return 0;
}
#endif

static void mouse_handler(MouseAct act, MouseBtn btn, int x, int y) {
    enum RegionId id = getregion(x, y);
    if (id != TAGS && id != EDIT) return;
    if (Focused != id) Focused = id;
    size_t row = (y-Regions[id].y) / x11_font_height(Font);
    size_t col = (x-Regions[id].x) / x11_font_width(Font);
    if (act == MOUSE_ACT_MOVE) {
        if (MouseBtns[MOUSE_BTN_LEFT].pressed) {
            if (MouseBtns[MOUSE_BTN_LEFT].count == 1) {
                view_setcursor(getview(id), row, col);
                MouseBtns[MOUSE_BTN_LEFT].count = 0;
            } else {
                view_selext(getview(id), row, col);
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
            /* execute the action on button release */
            if (MouseActs[btn])
                MouseActs[btn](id, MouseBtns[btn].count, row, col);
        }
    }
}

static void key_handler(int mods, Rune key) {
    /* handle the proper line endings */
    if (key == '\r') key = '\n';
    if (key == '\n' && currview()->buffer.crlf) key = RUNE_CRLF;
    /* search for a key binding entry */
    uint32_t mkey = tolower(key);
    for (int i = 0; i < nelem(Bindings); i++) {
        int keymods = Bindings[i].mods;
        if ((mkey == Bindings[i].key) && (keymods == ModAny || keymods == mods)) {
            Bindings[i].action();
            return;
        }
    }
    /* fallback to just inserting the rune if it doesn't fall in the private use area.
     * the private use area is used to encode special keys */
    if (key < 0xE000 || key > 0xF8FF)
        view_insert(currview(), true, key);
}

/* Drawing Routines
 *****************************************************************************/
#ifndef TEST
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
        draw_glyphs(Regions[id].x, Regions[id].y + ((y+1) * fheight), row->cols, row->rlen, row->len);
    }
    /* Place cursor on screen */
    if (id == Focused && csrx != SIZE_MAX && csry != SIZE_MAX)
        x11_draw_rect(CLR_BASE3, Regions[id].x + csrx * fwidth, Regions[id].y + (csry * fheight), 1, fheight);

    if (Regions[id].warp_ptr) {
        Regions[id].warp_ptr = false;
        size_t x = Regions[id].x + (csrx * fwidth)  - (fwidth/2);
        size_t y = Regions[id].y + (csry * fheight) + (fheight/2);
        x11_warp_mouse(x,y);
    }
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
    Regions[EDIT].x = 50;
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
    x11_draw_rect(CLR_BASE02, 0, Regions[EDIT].y-2, fwidth + 4, height - Regions[EDIT].y + 2);

    draw_status(CLR_BASE1, (width - 4) / x11_font_width(Font));
    draw_region(TAGS);
    draw_region(EDIT);
    cmdreap(); // cleanup any zombie child processes
}
#endif

/* UI Callbacks
 *****************************************************************************/
static void delete(void) {
    bool byword = x11_keymodsset(ModCtrl);
    view_delete(currview(), RIGHT, byword);
}

static void del_to_bol(void) {
    view_bol(currview(), true);
    if (view_selsize(currview()) > 0)
        delete();
}

static void del_to_eol(void) {
    view_eol(currview(), true);
    if (view_selsize(currview()) > 0)
        delete();
}

static void del_to_bow(void) {
    view_byword(currview(), LEFT, true);
    if (view_selsize(currview()) > 0)
        delete();
}

static void backspace(void) {
    bool byword = x11_keymodsset(ModCtrl);
    view_delete(currview(), LEFT, byword);
}

static void cursor_bol(void) {
    view_bol(currview(), false);
}

static void cursor_eol(void) {
    view_eol(currview(), false);
}

static void cursor_home(void) {
    bool extsel = x11_keymodsset(ModShift);
    if (x11_keymodsset(ModCtrl))
        view_bof(currview(), extsel);
    else
        view_bol(currview(), extsel);
}

static void cursor_end(void) {
    bool extsel = x11_keymodsset(ModShift);
    if (x11_keymodsset(ModCtrl))
        view_eof(currview(), extsel);
    else
        view_eol(currview(), extsel);
}

static void cursor_up(void) {
    bool extsel = x11_keymodsset(ModShift);
    view_byline(currview(), UP, extsel);
}

static void cursor_dn(void) {
    bool extsel = x11_keymodsset(ModShift);
    view_byline(currview(), DOWN, extsel);
}

static void cursor_left(void) {
    bool extsel = x11_keymodsset(ModShift);
    if (x11_keymodsset(ModCtrl))
        view_byword(currview(), LEFT, extsel);
    else
        view_byrune(currview(), LEFT, extsel);
}

static void cursor_right(void) {
    bool extsel = x11_keymodsset(ModShift);
    if (x11_keymodsset(ModCtrl))
        view_byword(currview(), RIGHT, extsel);
    else
        view_byrune(currview(), RIGHT, extsel);
}

static void page_up(void) {
    view_scrollpage(currview(), UP);
}

static void page_dn(void) {
    view_scrollpage(currview(), DOWN);
}

static void change_focus(void) {
    Focused = (Focused == TAGS ? EDIT : TAGS);
}

static void quit(void) {
    x11_deinit();
}

static void undo(void) {
    view_undo(currview());
}

static void redo(void) {
    view_redo(currview());
}

static void tag_undo(void) {
    view_undo(getview(EDIT));
}

static void tag_redo(void) {
    view_redo(getview(EDIT));
}

static void cut(void) {
    char* str = view_getstr(currview(), NULL);
    x11_setsel(CLIPBOARD, str);
    if (str && *str) delete();
}

static void copy(void) {
    char* str = view_getstr(currview(), NULL);
    x11_setsel(CLIPBOARD, str);
}

static void onpaste(char* text) {
    view_putstr(currview(), text);
}

static void paste(void) {
    assert(x11_getsel(CLIPBOARD, onpaste));
}

/* Tag/Cmd Execution
 *****************************************************************************/
static Tag* tag_lookup(char* cmd) {
    size_t len = 0;
    Tag* tags = Builtins;
    for (char* tag = cmd; *tag && !isspace(*tag); tag++, len++);
    while (tags->tag) {
        if (!strncmp(tags->tag, cmd, len))
            return tags;
        tags++;
    }
    return NULL;
}

static void tag_exec(Tag* tag, char* arg) {
    /* if we didnt get an arg, find one in the selection */
    if (!arg) arg = view_getstr(getview(TAGS), NULL);
    if (!arg) arg = view_getstr(getview(EDIT), NULL);
    /* execute the tag handler */
    tag->action.arg(arg);
    free(arg);
}

static void cmd_exec(char* cmd) {
    char op = '\0';
    if (*cmd == ':' || *cmd == '!' || *cmd == '<' || *cmd == '|' || *cmd == '>')
        op = *cmd, cmd++;
    ShellCmd[2] = cmd;
    /* execute the command */
    char *input = NULL, *output = NULL, *error = NULL;
    enum RegionId dest = EDIT;
    if (op && op != '<' && op != '!' && 0 == view_selsize(getview(EDIT)))
        getview(EDIT)->selection = (Sel){ .beg = 0, .end = buf_end(getbuf(EDIT)) };
    input = view_getstr(getview(EDIT), NULL);

    if (op == '!') {
        cmdrun(ShellCmd, NULL);
    } else if (op == '>') {
        dest = TAGS;
        output = cmdwriteread(ShellCmd, input, &error);
    } else if (op == '|') {
        output = cmdwriteread(ShellCmd, input, &error);
    } else if (op == ':') {
        SedCmd[2] = cmd;
        output = cmdwriteread(SedCmd, input, &error);
    } else {
        if (op != '<') dest = Focused;
        output = cmdread(ShellCmd, &error);
    }

    if (error)
        view_append(getview(TAGS), chomp(error));

    if (output) {
        if (op == '>')
            view_append(getview(dest), chomp(output));
        else
            view_putstr(getview(dest), output);
        Focused = dest;
    }
    /* cleanup */
    free(input);
    free(output);
    free(error);
}

static void exec(char* cmd) {
    /* skip leading space */
    for (; *cmd && isspace(*cmd); cmd++);
    if (!*cmd) return;
    /* see if it matches a builtin tag */
    Tag* tag = tag_lookup(cmd);
    if (tag) {
        while (*cmd && !isspace(*cmd++));
        tag_exec(tag, (*cmd ? stringdup(cmd) : NULL));
    } else {
        cmd_exec(cmd);
    }
}

/* Mouse Handling
 *****************************************************************************/
static void mouse_left(enum RegionId id, size_t count, size_t row, size_t col) {
    if (count == 1) {
        if (x11_keymodsset(ModShift))
            view_selext(getview(id), row, col);
        else
            view_setcursor(getview(id), row, col);
    } else if (count == 2) {
        view_select(getview(id), row, col);
    } else if (count == 3) {
        view_selword(getview(id), row, col);
    }
}

static void mouse_middle(enum RegionId id, size_t count, size_t row, size_t col) {
    if (MouseBtns[MOUSE_BTN_LEFT].pressed) {
        cut();
    } else {
        char* str = view_fetchcmd(getview(id), row, col);
        if (str) exec(str);
        free(str);
    }
}

static void mouse_right(enum RegionId id, size_t count, size_t row, size_t col) {
    if (MouseBtns[MOUSE_BTN_LEFT].pressed) {
        paste();
    } else {
        SearchDir *= (x11_keymodsset(ModShift) ? -1 : +1);
        view_find(getview(id), SearchDir, row, col);
        Regions[id].warp_ptr = true;
    }
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

static Sel* getsel(enum RegionId id) {
    return &(getview(id)->selection);
}

static Sel* currsel(void) {
    return getsel(Focused);
}
#endif
