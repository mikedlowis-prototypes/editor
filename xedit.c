#include <stdc.h>
#include <x11.h>
#include <utf.h>
#include <edit.h>
#include <ctype.h>

static void redraw(int width, int height);
static void mouse_handler(MouseAct act, MouseBtn btn, int x, int y);
static void key_handler(Rune key);

/* Global Data
 *****************************************************************************/
//Buf Buffer;
//Buf TagBuffer;
//unsigned TargetCol = 0;
//unsigned SelBeg = 0;
//unsigned SelEnd = 0;
//static unsigned ScrRows = 0;
//static unsigned ScrCols = 0;
//static XFont Fonts;

static bool TagWinExpanded = false;
static View TagView;
static View BufView;
static View* Focused = &BufView;
static XFont Font;
static XConfig Config = {
    .redraw       = redraw,
    .handle_key   = key_handler,
    .handle_mouse = mouse_handler,
    .palette      = COLOR_PALETTE
};

/* UI Callbacks
 *****************************************************************************/
static void cursor_up(void) {
    view_byline(Focused, -1);
}

static void cursor_dn(void) {
    view_byline(Focused, +1);
}

static void cursor_left(void) {
    view_byrune(Focused, -1);
}

static void cursor_right(void) {
    view_byrune(Focused, +1);
}

static void change_focus(void) {
    if (Focused == &TagView) {
        if (TagWinExpanded)
            TagWinExpanded = false;
        Focused = &BufView;
    } else {
        Focused = &TagView;
        TagWinExpanded = true;
    }
}

/* UI Callbacks
 *****************************************************************************/
static void mouse_handler(MouseAct act, MouseBtn btn, int x, int y) {

}

/* Keyboard Bindings
 *****************************************************************************/
typedef struct {
    Rune key;
    void (*action)(void);
} KeyBinding_T;

static KeyBinding_T Insert[] = {
    { KEY_UP,        cursor_up     },
    { KEY_DOWN,      cursor_dn     },
    { KEY_LEFT,      cursor_left   },
    { KEY_RIGHT,     cursor_right  },
    //{ KEY_CTRL_Q,    quit          },
    //{ KEY_CTRL_S,    write         },
    { KEY_CTRL_T,    change_focus    },
    //{ KEY_CTRL_Z,    undo          },
    //{ KEY_CTRL_Y,    redo          },
    //{ KEY_CTRL_X,    cut           },
    //{ KEY_CTRL_C,    copy          },
    //{ KEY_CTRL_V,    paste         },
    //{ KEY_HOME,      cursor_bol    },
    //{ KEY_END,       cursor_eol    },
    //{ KEY_DELETE,    delete        },
    //{ KEY_BACKSPACE, backspace     },
    { 0,             NULL          }
};

static void process_table(KeyBinding_T* bindings, Rune key) {
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
        view_insert(Focused, key);
}

static void key_handler(Rune key) {
    /* ignore invalid keys */
    if (key == RUNE_ERR) return;
    /* handle the proper line endings */
    if (key == '\r') key = '\n';
    if (key == '\n' && Focused->buffer.crlf) key = RUNE_CRLF;
    /* handle the key */
    process_table(Insert, key);
}

/* Redisplay Functions
 *****************************************************************************/
static void draw_runes(unsigned x, unsigned y, int fg, int bg, UGlyph* glyphs, size_t rlen) {
    XGlyphSpec specs[rlen];
    while (rlen) {
        size_t nspecs = x11_font_getglyphs(specs, (XGlyph*)glyphs, rlen, Font, x, y);
        x11_draw_glyphs(fg, bg, specs, nspecs);
        rlen -= nspecs;
    }
}

static void draw_glyphs(unsigned x, unsigned y, UGlyph* glyphs, size_t rlen, size_t ncols) {
    XGlyphSpec specs[rlen];
    size_t i = 0;
    while (rlen) {
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

static void draw_status(int fg, unsigned ncols) {
    UGlyph glyphs[ncols], *status = glyphs;
    (status++)->rune = (BufView.buffer.charset == BINARY ? 'B' : 'U');
    (status++)->rune = (BufView.buffer.crlf ? 'C' : 'N');
    (status++)->rune = (BufView.buffer.modified ? '*' : ' ');
    (status++)->rune = ' ';
    char* path = (BufView.buffer.path ? BufView.buffer.path : "*scratch*");
    size_t len = strlen(path);
    if (len > ncols-4) {
        (status++)->rune = '.';
        (status++)->rune = '.';
        (status++)->rune = '.';
        path += (len - ncols) + 6;
    }
    while(*path)
        (status++)->rune = *path++;
    draw_runes(0, 0, fg, 0, glyphs, status - glyphs);
}

static size_t stackvert(size_t off, size_t rows) {
    return (off + 4 + (rows * x11_font_height(Font)));
}

static size_t draw_view(size_t off, View* view, size_t rows, size_t width) {
    size_t fheight = x11_font_height(Font);
    size_t fwidth  = x11_font_width(Font);
    size_t cols    = (width - 4) / fwidth;
    view_resize(view, rows, cols);
    /* update the screen buffer and retrieve cursor coordinates */
    size_t csrx, csry;
    view_update(view, &csrx, &csry);
    /* flush the screen buffer */
    for (size_t y = 0; y < rows; y++) {
        Row* row = view_getrow(view, y);
        draw_glyphs(2, off + ((y+1) * fheight), row->cols, row->rlen, row->len);
    }
    /* Place cursor on screen */
    if (view == Focused || view == &BufView)
        x11_draw_rect(CLR_BASE3, 2 + csrx * fwidth, off + (csry * fheight), 1, fheight);
    return (off + 4 + (rows * x11_font_height(Font)));
}

static void redraw(int width, int height) {
    size_t vmargin = 2;
    size_t hmargin = 2;
    size_t fheight = x11_font_height(Font);
    size_t fwidth  = x11_font_width(Font);
    /* clear background and draw status */
    size_t vpos = 0;
    x11_draw_rect(CLR_BASE03, 0, vpos, width, height);
    draw_status(CLR_BASE3, width / fwidth);
    /* draw the tag buffer */
    vpos = stackvert(vpos, 1);
    x11_draw_rect(CLR_BASE01, 0, vpos++, width, 1);
    size_t totrows = (height - vpos - 9) / fheight;
    size_t tagrows = (TagWinExpanded ? (totrows / 4) : 1);
    size_t bufrows = totrows - tagrows;
    vpos = draw_view(3+vpos, &TagView, tagrows, width);
    /* draw thee dit buffer */
    x11_draw_rect(CLR_BASE01, 0, ++vpos, width, 1);
    vpos = draw_view(3+vpos, &BufView, bufrows, width);
}

/* Main Routine
 *****************************************************************************/
int main(int argc, char** argv) {
    /* load the buffer views */
    view_init(&TagView, NULL);
    view_init(&BufView, (argc > 1 ? argv[1] : NULL));
    buf_putstr(&(TagView.buffer), 0, 0, DEFAULT_TAGS);
    /* initialize the display engine */
    x11_init(&Config);
    x11_window("edit", Width, Height);
    x11_show();
    Font = x11_font_load(FONTNAME);
    x11_loop();
    return 0;
}

#if 0
#if 0
/* External Commands
 *****************************************************************************/
#ifdef __MACH__
static char* CopyCmd[]  = { "pbcopy", NULL };
static char* PasteCmd[] = { "pbpaste", NULL };
#else
static char* CopyCmd[]  = { "xsel", "-bi", NULL };
static char* PasteCmd[] = { "xsel", "-bo", NULL };
#endif

/* Keyboard Actions
 *****************************************************************************/
static void delete(void) {
    if (SelEnd == buf_end(&Buffer)) return;
    size_t n = SelEnd - SelBeg;
    for (size_t i = 0; i < n; i++)
        buf_del(&Buffer, SelBeg);
    SelEnd = SelBeg;
    TargetCol = buf_getcol(&Buffer, SelEnd);
}

static void backspace(void) {
    if (SelBeg > 0 && SelBeg == SelEnd) SelBeg--;
    while (SelBeg < SelEnd)
        buf_del(&Buffer, --SelEnd);
    TargetCol = buf_getcol(&Buffer, SelEnd);
}

static void quit(void) {
    static uint32_t num_clicks = 0;
    static uint32_t prevtime = 0;
    uint32_t now = getmillis();
    num_clicks = (now - prevtime < 250 ? num_clicks+1 : 1);
    prevtime = now;
    if (!Buffer.modified || num_clicks >= 2)
        exit(0);
}

static void write(void) {
    buf_save(&Buffer);
}

static void undo(void) {
    SelBeg = SelEnd = buf_undo(&Buffer, SelEnd);
    TargetCol = buf_getcol(&Buffer, SelEnd);
}

static void redo(void) {
    SelBeg = SelEnd = buf_redo(&Buffer, SelEnd);
    TargetCol = buf_getcol(&Buffer, SelEnd);
}

static void cut(void) {
    char* str = buf_getstr(&Buffer, SelBeg, SelEnd);
    cmdwrite(CopyCmd, str);
    free(str);
    delete();
}

static void copy(void) {
    char* str = buf_getstr(&Buffer, SelBeg, SelEnd);
    cmdwrite(CopyCmd, str);
    free(str);
}

static void paste(void) {
    char* str = cmdread(PasteCmd);
    buf_putstr(&Buffer, SelBeg, SelEnd, str);
    free(str);
}

static void cursor_bol(void) {
    SelBeg = SelEnd = buf_bol(&Buffer, SelEnd);
    TargetCol = 0;
}

static void cursor_eol(void) {
    SelBeg = SelEnd = buf_eol(&Buffer, SelEnd);
    TargetCol = (unsigned)-1;
}

static void tagwin(void) {
    TagWinExpanded = !TagWinExpanded;
}

#endif

/* Mouse Actions
 *****************************************************************************/
#if 0
void unused(int x, int y) {
}

void move_cursor(int x, int y) {
    if (y == 0) return;
    //SelBeg = SelEnd = screen_getoff(&Buffer, SelEnd, y-1, x);
    TargetCol = buf_getcol(&Buffer, SelEnd);
}

void bigword(int x, int y) {
    unsigned mbeg = SelEnd, mend = SelEnd;
    for (; !risblank(buf_get(&Buffer, mbeg-1)); mbeg--);
    for (; !risblank(buf_get(&Buffer, mend));   mend++);
    SelBeg = mbeg, SelEnd = mend-1;
}

void selection(int x, int y) {
    unsigned bol = buf_bol(&Buffer, SelEnd);
    Rune r = buf_get(&Buffer, SelEnd);
    if (SelEnd == bol || r == '\n' || r == RUNE_CRLF) {
        SelBeg = bol;
        SelEnd = buf_eol(&Buffer, SelEnd);
    } else if (risword(r)) {
        SelBeg = buf_bow(&Buffer, SelEnd);
        SelEnd = buf_eow(&Buffer, SelEnd++);
    } else if (r == '(' || r == ')') {
        SelBeg = buf_lscan(&Buffer, SelEnd,   '(');
        SelEnd = buf_rscan(&Buffer, SelEnd++, ')');
    } else if (r == '[' || r == ']') {
        SelBeg = buf_lscan(&Buffer, SelEnd,   '[');
        SelEnd = buf_rscan(&Buffer, SelEnd++, ']');
    } else if (r == '{' || r == '}') {
        SelBeg = buf_lscan(&Buffer, SelEnd,   '{');
        SelEnd = buf_rscan(&Buffer, SelEnd++, '}');
    } else {
        bigword(x,y);
    }
}

void search(int x, int y) {
    //unsigned clickpos = screen_getoff(&Buffer, SelEnd, y-1, x);
    //if (clickpos < SelBeg || clickpos > SelEnd) {
    //    move_cursor(x,y);
    //    selection(x,y);
    //} else {
    //    buf_find(&Buffer, &SelBeg, &SelEnd);
    //}
    //unsigned c, r;
    //screen_update(&Buffer, SelEnd, &c, &r);
    //extern void move_pointer(unsigned c, unsigned r);
    //move_pointer(c, r);
}

void scrollup(int x, int y) {
    SelBeg = SelEnd = buf_byline(&Buffer, SelEnd, -ScrollLines);
}

void scrolldn(int x, int y) {
    SelBeg = SelEnd = buf_byline(&Buffer, SelEnd, ScrollLines);
}
#endif
/* Mouse Input Handler
 *****************************************************************************/
struct {
    uint32_t time;
    uint32_t count;
} Buttons[5] = { 0 };

void (*Actions[5][3])(int x, int y) = { 0
                          /*  Single       Double     Triple    */
    //[MOUSE_BTN_LEFT]      = { move_cursor, selection, bigword,  },
    //[MOUSE_BTN_MIDDLE]    = { unused,      unused,    unused,   },
    //[MOUSE_BTN_RIGHT]     = { search,      search,    search,   },
    //[MOUSE_BTN_WHEELUP]   = { scrollup,    scrollup,  scrollup, },
    //[MOUSE_BTN_WHEELDOWN] = { scrolldn,    scrolldn,  scrolldn, },
};

static void mouse_handler(MouseAct act, MouseBtn btn, int x, int y) {
    //unsigned row  = y / Fonts.base.height;
    //unsigned col  = x / Fonts.base.width;
    //unsigned twsz = (TagWinExpanded ? ((ScrRows - 1) / 4) : 1);

    ////if (row == 0) {
    ////    puts("status");
    ////} else if (row >= 1 && row <= twsz) {
    ////    puts("tagwin");
    ////} else {
    ////    puts("editwin");
    ////}

    //if (act == MOUSE_ACT_DOWN) {
    //    //if (mevnt->button >= 5) return;
    //    ///* update the number of clicks */
    //    //uint32_t now = getmillis();
    //    //uint32_t elapsed = now - Buttons[mevnt->button].time;
    //    //if (elapsed <= 250)
    //    //    Buttons[mevnt->button].count++;
    //    //else
    //    //    Buttons[mevnt->button].count = 1;
    //    //Buttons[mevnt->button].time = now;
    //    ///* execute the click action */
    //    //uint32_t nclicks = Buttons[mevnt->button].count;
    //    //nclicks = (nclicks > 3 ? 1 : nclicks);
    //    //Actions[mevnt->button][nclicks-1](mevnt);
    //} else if (act == MOUSE_ACT_MOVE) {
    //    puts("move");
    //    //if (mevnt->y == 0 || mevnt->button != MOUSE_LEFT) return;
    //    //SelEnd = screen_getoff(&Buffer, SelEnd, mevnt->y-1, mevnt->x);
    //    //TargetCol = buf_getcol(&Buffer, SelEnd);
    //}
}

/* Screen Redraw
 *****************************************************************************/

static void draw_tagwin(unsigned off, unsigned rows, unsigned cols) {
    //screen_setsize(&TagBuffer, rows, cols);

}

static void draw_bufwin(unsigned off, unsigned rows, unsigned cols) {
    //screen_setsize(&Buffer, rows, cols);
    ///* update the screen buffer and retrieve cursor coordinates */
    //unsigned csrx, csry;
    //screen_update(&Buffer, SelEnd, &csrx, &csry);
    ///* flush the screen buffer */
    //unsigned nrows, ncols;
    //screen_getsize(&nrows, &ncols);
    //draw_status(CLR_BASE2, ncols);
    //for (unsigned y = 0; y < nrows; y++) {
    //    Row* row = screen_getrow(y);
    //    draw_glyphs(0, off + ((y+1) * Fonts.base.height), row->cols, row->rlen, row->len);
    //}
    ///* Place cursor on screen */
    //x11_draw_rect(CLR_BASE3, csrx * Fonts.base.width, off + (csry * Fonts.base.height), 1, Fonts.base.height);
}

static void redraw(int width, int height) {
    //unsigned fheight = Fonts.base.height;
    //unsigned fwidth  = Fonts.base.width;
    //ScrRows = height / Fonts.base.height;
    //ScrCols = width  / Fonts.base.width;
    ///* clear background and draw status */
    //x11_draw_rect(CLR_BASE03, 0, 0, width, height);
    //draw_status(CLR_BASE2, ScrCols);
    ///* draw the tag window */
    //unsigned twsz = (TagWinExpanded ? ((ScrRows - 1) / 4) : 1);
    //x11_draw_rect(CLR_BASE02, 0, fheight, width, twsz * fheight);
    //x11_draw_rect(CLR_BASE01, 0, fheight, width, 1);
    //draw_tagwin(fheight, twsz, ScrCols);
    ///* draw the file window */
    //x11_draw_rect(CLR_BASE01, 0, (twsz+1) * fheight, width, 1);
    //draw_bufwin((twsz+1) * fheight, ScrRows - (twsz), ScrCols);
}

int main(int argc, char** argv) {
    /* load the buffer views */
    view_init(&TagView, NULL);
    view_init(&BufView, (argc > 1 ? argv[1] : NULL));
    /* initialize the display engine */
    x11_init(&Config);
    x11_window("edit", Width, Height);
    x11_show();
    x11_font_load(&Fonts, FONTNAME);
    x11_loop();
    return 0;
}

void move_pointer(unsigned x, unsigned y) {
    x = (x * Fonts.base.width)  + (Fonts.base.width / 2);
    y = ((y+1) * Fonts.base.height) + (Fonts.base.height / 2);
    x11_warp_mouse(x,y);
}
#endif
