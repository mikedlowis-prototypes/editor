#include <stdc.h>
#include <X.h>
#include <utf.h>
#include <edit.h>

static void redraw(int width, int height);
static void mouse_handler(MouseAct act, MouseBtn btn, int x, int y);

Buf Buffer;
unsigned TargetCol = 0;
unsigned SelBeg = 0;
unsigned SelEnd = 0;

static XFont Fonts;
static XConfig Config = {
    .redraw       = redraw,
    .handle_key   = handle_key,
    .handle_mouse = mouse_handler,
    .palette    = {
        /* ARGB color values */
        0xff002b36,
        0xff073642,
        0xff586e75,
        0xff657b83,
        0xff839496,
        0xff93a1a1,
        0xffeee8d5,
        0xfffdf6e3,
        0xffb58900,
        0xffcb4b16,
        0xffdc322f,
        0xffd33682,
        0xff6c71c4,
        0xff268bd2,
        0xff2aa198,
        0xff859900
    }
};

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

static void draw_cursor(unsigned csrx, unsigned csry) {
    unsigned rwidth;
    UGlyph* csrrune = screen_getglyph(csry, csrx, &rwidth);
    csrrune->attr = (CLR_BASE3 << 8 | CLR_BASE03);
    if (buf_locked(&Buffer)) {
        x11_draw_rect(CLR_BASE3, csrx * Fonts.base.width, (csry+1) * Fonts.base.height, rwidth * Fonts.base.width, Fonts.base.height);
        draw_glyphs(csrx * Fonts.base.width, (csry+2) * Fonts.base.height, csrrune, 1, rwidth);
    } else {
        x11_draw_rect(CLR_BASE3, csrx * Fonts.base.width, (csry+1) * Fonts.base.height, 1, Fonts.base.height);
    }
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
    draw_cursor(csrx, csry);
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
