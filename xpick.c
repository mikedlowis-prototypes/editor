#include <stdc.h>
#include <X.h>
#include <utf.h>
#include <edit.h>

static void redraw(int width, int height);
static void mouse_input(MouseAct act, MouseBtn btn, int x, int y);
static void keyboard_input(uint32_t key);

Buf Buffer;
static XFont Fonts;
static XConfig Config = {
    .redraw       = redraw,
    .handle_key   = keyboard_input,
    .handle_mouse = mouse_input,
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

static void redraw(int width, int height) {
    /* draw the background colors */
    x11_draw_rect(CLR_BASE03, 0, 0, width, height);
    x11_draw_rect(CLR_BASE02, 0, 0, width, Fonts.base.height);
    x11_draw_rect(CLR_BASE01, 0, Fonts.base.height, width, 1);
}

static void mouse_input(MouseAct act, MouseBtn btn, int x, int y) {
    (void)act;
    (void)btn;
    (void)x;
    (void)y;
}

static void keyboard_input(uint32_t key) {
    (void)key;
}

int main(int argc, char** argv) {
    /* load the buffer */
    buf_init(&Buffer);
    buf_setlocked(&Buffer, false);
    /* initialize the display engine */
    x11_init(&Config);
    x11_window("pick", Width, Height);
    x11_show();
    x11_font_load(&Fonts, FONTNAME);
    x11_loop();
    return 0;
}
