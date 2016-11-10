#include <stdc.h>
#include <X.h>
#include <utf.h>
#include <edit.h>
#include <ctype.h>

static void redraw(int width, int height);
static void mouse_handler(MouseAct act, MouseBtn btn, int x, int y);
static void key_handler(Rune key);

/* Global Data
 *****************************************************************************/
Buf Buffer;
unsigned TargetCol = 0;
unsigned SelBeg = 0;
unsigned SelEnd = 0;
static XFont Fonts;
static XConfig Config = {
    .redraw       = redraw,
    .handle_key   = key_handler,
    .handle_mouse = mouse_handler,
    .palette      = COLOR_PALETTE
};

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
    clipcopy(str);
    free(str);
    delete();
}

static void copy(void) {
    char* str = buf_getstr(&Buffer, SelBeg, SelEnd);
    clipcopy(str);
    free(str);
}

static void paste(void) {
    char* str = clippaste();
    buf_putstr(&Buffer, SelBeg, SelEnd, str);
    free(str);
}

static void cursor_up(void) {
    SelBeg = SelEnd = buf_byline(&Buffer, SelEnd, -1);
    SelBeg = SelEnd = buf_setcol(&Buffer, SelEnd, TargetCol);
}

static void cursor_dn(void) {
    SelBeg = SelEnd = buf_byline(&Buffer, SelEnd, 1);
    SelBeg = SelEnd = buf_setcol(&Buffer, SelEnd, TargetCol);
}

static void cursor_left(void) {
    SelBeg = SelEnd = buf_byrune(&Buffer, SelEnd, -1);
    TargetCol = buf_getcol(&Buffer, SelEnd);
}

static void cursor_right(void) {
    SelBeg = SelEnd = buf_byrune(&Buffer, SelEnd, 1);
    TargetCol = buf_getcol(&Buffer, SelEnd);
}

static void cursor_bol(void) {
    SelBeg = SelEnd = buf_bol(&Buffer, SelEnd);
    TargetCol = 0;
}

static void cursor_eol(void) {
    SelBeg = SelEnd = buf_eol(&Buffer, SelEnd);
    TargetCol = (unsigned)-1;
}

/* Keyboard Bindings
 *****************************************************************************/
typedef struct {
    Rune key;
    void (*action)(void);
} KeyBinding_T;

static KeyBinding_T Insert[] = {
    { KEY_CTRL_Q,    quit          },
    { KEY_CTRL_S,    write         },
    { KEY_CTRL_Z,    undo          },
    { KEY_CTRL_Y,    redo          },
    { KEY_CTRL_X,    cut           },
    { KEY_CTRL_C,    copy          },
    { KEY_CTRL_V,    paste         },
    { KEY_UP,        cursor_up     },
    { KEY_DOWN,      cursor_dn     },
    { KEY_LEFT,      cursor_left   },
    { KEY_RIGHT,     cursor_right  },
    { KEY_HOME,      cursor_bol    },
    { KEY_END,       cursor_eol    },
    { KEY_DELETE,    delete        },
    { KEY_BACKSPACE, backspace     },
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
    /* skip control and nonprintable characters */
    if ((!isspace(key) && key < 0x20) ||
        (key >= 0xE000 && key <= 0xF8FF))
        return;
    /* fallback to just inserting the rune */
    buf_ins(&Buffer, SelEnd++, key);
    SelBeg = SelEnd;
    TargetCol = buf_getcol(&Buffer, SelEnd);
}

static void key_handler(Rune key) {
    /* ignore invalid keys */
    if (key == RUNE_ERR) return;
    /* handle the proper line endings */
    if (key == '\r') key = '\n';
    if (key == '\n' && Buffer.crlf) key = RUNE_CRLF;
    /* handle the key */
    process_table(Insert, key);
}

/* Screen Redraw
 *****************************************************************************/
static void draw_runes(unsigned x, unsigned y, int fg, int bg, UGlyph* glyphs, size_t rlen) {
    XftGlyphFontSpec specs[rlen];
    while (rlen) {
        size_t nspecs = x11_font_getglyphs(specs, (XGlyph*)glyphs, rlen, &Fonts, x, y);
        x11_draw_glyphs(fg, bg, specs, nspecs);
        rlen -= nspecs;
    }
}

static void draw_glyphs(unsigned x, unsigned y, UGlyph* glyphs, size_t rlen, size_t ncols) {
    XftGlyphFontSpec specs[rlen];
    size_t i = 0;
    while (rlen) {
        int numspecs = 0;
        uint32_t attr = glyphs[i].attr;
        while (i < ncols && glyphs[i].attr == attr) {
            x11_font_getglyph(&Fonts, &(specs[numspecs]), glyphs[i].rune);
            specs[numspecs].x = x;
            specs[numspecs].y = y - Fonts.base.descent;
            x += Fonts.base.width;
            numspecs++;
            i++;
            /* skip over null chars which mark multi column runes */
            for (; i < ncols && !glyphs[i].rune; i++)
                x += Fonts.base.width;
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
    (status++)->rune = (Buffer.charset == BINARY ? 'B' : 'U');
    (status++)->rune = (Buffer.crlf ? 'C' : 'N');
    (status++)->rune = (Buffer.modified ? '*' : ' ');
    (status++)->rune = ' ';
    char* path = (Buffer.path ? Buffer.path : "*scratch*");
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

static void redraw(int width, int height) {
    /* update the screen size if needed */
    screen_setsize(&Buffer,
        height / Fonts.base.height - 1,
        width  / Fonts.base.width);

    /* draw the background colors */
    x11_draw_rect(CLR_BASE03, 0, 0, width, height);
    x11_draw_rect(CLR_BASE02, 79 * Fonts.base.width, 0, Fonts.base.width, height);
    x11_draw_rect(CLR_BASE02, 0, 0, width, Fonts.base.height);
    x11_draw_rect(CLR_BASE01, 0, Fonts.base.height, width, 1);

    /* update the screen buffer and retrieve cursor coordinates */
    unsigned csrx, csry;
    screen_update(&Buffer, SelEnd, &csrx, &csry);

    /* flush the screen buffer */
    unsigned nrows, ncols;
    screen_getsize(&nrows, &ncols);
    draw_status(CLR_BASE2, ncols);
    for (unsigned y = 0; y < nrows; y++) {
        Row* row = screen_getrow(y);
        draw_glyphs(0, (y+2) * Fonts.base.height, row->cols, row->rlen, row->len);
    }

    /* Place cursor on screen */
    x11_draw_rect(CLR_BASE3, csrx * Fonts.base.width, (csry+1) * Fonts.base.height, 1, Fonts.base.height);
}

static void mouse_handler(MouseAct act, MouseBtn btn, int x, int y) {
    MouseEvent evnt = {
        .type = act,
        .button = btn,
        .x = x / Fonts.base.width,
        .y = y / Fonts.base.height
    };
    handle_mouse(&evnt);
}

int main(int argc, char** argv) {
    /* load the buffer */
    buf_init(&Buffer);
    if (argc > 1)
        buf_load(&Buffer, argv[1]);
    buf_setlocked(&Buffer, false);
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
