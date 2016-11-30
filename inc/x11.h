typedef enum {
    MOUSE_ACT_UP,
    MOUSE_ACT_DOWN,
    MOUSE_ACT_MOVE
} MouseAct;

typedef enum {
    MOUSE_BTN_LEFT      = 0,
    MOUSE_BTN_MIDDLE    = 1,
    MOUSE_BTN_RIGHT     = 2,
    MOUSE_BTN_WHEELUP   = 3,
    MOUSE_BTN_WHEELDOWN = 4,
    MOUSE_BTN_NONE      = 5,
    MOUSE_BTN_COUNT     = 5
} MouseBtn;

typedef struct {
    void (*redraw)(int width, int height);
    void (*handle_key)(int mods, uint32_t rune);
    void (*handle_mouse)(MouseAct act, MouseBtn btn, int x, int y);
    uint32_t palette[16];
} XConfig;

typedef void* XFont;

typedef struct {
    uint32_t attr; /* attributes  applied to this cell */
    uint32_t rune; /* rune value for the cell */
} XGlyph;

typedef struct {
    void* font;
    uint32_t glyph;
    short x;
    short y;
} XGlyphSpec;

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

/* Key modifier masks */
enum {
    ModNone       = 0,
    ModShift      = (1 << 0),
    ModCapsLock   = (1 << 1),
    ModCtrl       = (1 << 2),
    ModAlt        = (1 << 3),
    ModNumLock    = (1 << 4),
    ModScrollLock = (1 << 5),
    ModWindows    = (1 << 6),
    ModAny        = ModWindows-1
};

void x11_init(XConfig* cfg);
void x11_deinit(void);
int x11_keymods(void);
bool x11_keymodsset(int mask);
void x11_window(char* name, int width, int height);
void x11_dialog(char* name, int height, int width);
void x11_show(void);
void x11_loop(void);
XFont x11_font_load(char* name);
size_t x11_font_height(XFont fnt);
size_t x11_font_width(XFont fnt);
size_t x11_font_descent(XFont fnt);
void x11_draw_rect(int color, int x, int y, int width, int height);
void x11_getsize(int* width, int* height);
void x11_warp_mouse(int x, int y);
void x11_draw_utf8(XFont font, int fg, int bg, int x, int y, char* str);
void x11_font_getglyph(XFont font, XGlyphSpec* spec, uint32_t rune);
size_t x11_font_getglyphs(XGlyphSpec* specs, const XGlyph* glyphs, int len, XFont font, int x, int y);
void x11_draw_glyphs(int fg, int bg, XGlyphSpec* glyphs, size_t nglyphs);
void x11_draw_utf8(XFont font, int fg, int bg, int x, int y, char* str);
