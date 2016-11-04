#include <X11/Xft/Xft.h>
#include <stdint.h>

typedef enum {
    MOUSE_ACT_UP,
    MOUSE_ACT_DOWN,
    MOUSE_ACT_MOVE
} MouseAct;

typedef enum {
        MOUSE_BTN_LEFT = 0,
        MOUSE_BTN_MIDDLE,
        MOUSE_BTN_RIGHT,
        MOUSE_BTN_WHEELUP,
        MOUSE_BTN_WHEELDOWN,
        MOUSE_BTN_NONE
} MouseBtn;

typedef struct {
    void (*redraw)(int width, int height);
    void (*handle_key)(uint32_t rune);
    void (*handle_mouse)(MouseAct act, MouseBtn btn, int x, int y);
    uint32_t palette[16];
} XConfig;

#ifndef MAXFONTS
#define MAXFONTS 16
#endif

typedef struct {
    struct {
        int height;
        int width;
        int ascent;
        int descent;
        XftFont* match;
        FcFontSet* set;
        FcPattern* pattern;
    } base;
    struct {
        XftFont* font;
        uint32_t unicodep;
    } cache[MAXFONTS];
    int ncached;
} XFont;

typedef struct {
    uint32_t attr; /* attributes  applied to this cell */
    uint32_t rune; /* rune value for the cell */
} XGlyph;

/* key definitions */
enum Keys {
    /* Define some runes in the private use area of unicode to represent
     * special keys */
    KEY_F1     = (0xE000+0),
    KEY_F2     = (0xE000+1),
    KEY_F3     = (0xE000+2),
    KEY_F4     = (0xE000+3),
    KEY_F5     = (0xE000+4),
    KEY_F6     = (0xE000+5),
    KEY_F7     = (0xE000+6),
    KEY_F8     = (0xE000+7),
    KEY_F9     = (0xE000+8),
    KEY_F10    = (0xE000+9),
    KEY_F11    = (0xE000+10),
    KEY_F12    = (0xE000+11),
    KEY_INSERT = (0xE000+12),
    KEY_DELETE = (0xE000+13),
    KEY_HOME   = (0xE000+14),
    KEY_END    = (0xE000+15),
    KEY_PGUP   = (0xE000+16),
    KEY_PGDN   = (0xE000+17),
    KEY_UP     = (0xE000+18),
    KEY_DOWN   = (0xE000+19),
    KEY_RIGHT  = (0xE000+20),
    KEY_LEFT   = (0xE000+21),

    /* ASCII Control Characters */
    KEY_CTRL_TILDE       = 0x00,
    KEY_CTRL_2           = 0x00,
    KEY_CTRL_A           = 0x01,
    KEY_CTRL_B           = 0x02,
    KEY_CTRL_C           = 0x03,
    KEY_CTRL_D           = 0x04,
    KEY_CTRL_E           = 0x05,
    KEY_CTRL_F           = 0x06,
    KEY_CTRL_G           = 0x07,
    KEY_BACKSPACE        = 0x08,
    KEY_CTRL_H           = 0x08,
    KEY_TAB              = 0x09,
    KEY_CTRL_I           = 0x09,
    KEY_CTRL_J           = 0x0A,
    KEY_CTRL_K           = 0x0B,
    KEY_CTRL_L           = 0x0C,
    KEY_ENTER            = 0x0D,
    KEY_CTRL_M           = 0x0D,
    KEY_CTRL_N           = 0x0E,
    KEY_CTRL_O           = 0x0F,
    KEY_CTRL_P           = 0x10,
    KEY_CTRL_Q           = 0x11,
    KEY_CTRL_R           = 0x12,
    KEY_CTRL_S           = 0x13,
    KEY_CTRL_T           = 0x14,
    KEY_CTRL_U           = 0x15,
    KEY_CTRL_V           = 0x16,
    KEY_CTRL_W           = 0x17,
    KEY_CTRL_X           = 0x18,
    KEY_CTRL_Y           = 0x19,
    KEY_CTRL_Z           = 0x1A,
    KEY_ESCAPE           = 0x1B,
    KEY_CTRL_LSQ_BRACKET = 0x1B,
    KEY_CTRL_3           = 0x1B,
    KEY_CTRL_4           = 0x1C,
    KEY_CTRL_BACKSLASH   = 0x1C,
    KEY_CTRL_5           = 0x1D,
    KEY_CTRL_RSQ_BRACKET = 0x1D,
    KEY_CTRL_6           = 0x1E,
    KEY_CTRL_7           = 0x1F,
    KEY_CTRL_SLASH       = 0x1F,
    KEY_CTRL_UNDERSCORE  = 0x1F,
};

void x11_init(XConfig* cfg);
void x11_deinit(void);
void x11_window(char* name, int width, int height);
void x11_dialog(char* name, int height, int width);
void x11_show(void);
void x11_loop(void);
void x11_draw_rect(int color, int x, int y, int width, int height);
void x11_draw_glyphs(int fg, int bg, XftGlyphFontSpec* glyphs, size_t nglyphs);
void x11_getsize(int* width, int* height);
void x11_warp_mouse(int x, int y);
void x11_font_load(XFont* font, char* name);
void x11_font_getglyph(XFont* font, XftGlyphFontSpec* spec, uint32_t rune);
size_t x11_font_getglyphs(XftGlyphFontSpec* specs, const XGlyph* glyphs, int len, XFont* font, int x, int y);
