#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* UTF-8 Handling
 *****************************************************************************/
enum {
    UTF_MAX   = 6u,        /* maximum number of bytes that make up a rune */
    RUNE_SELF = 0x80,      /* byte values larger than this are *not* ascii */
    RUNE_ERR  = 0xFFFD,    /* rune value representing an error */
    RUNE_MAX  = 0x10FFFF,  /* Maximum decodable rune value */
    RUNE_EOF  = UINT32_MAX /* ruen value representing end of file */
};

/* Represents a unicode code point */
typedef uint32_t Rune;

size_t utf8encode(char str[UTF_MAX], Rune rune);
bool utf8decode(Rune* rune, size_t* length, int byte);
Rune fgetrune(FILE* f);
void fputrune(Rune rune, FILE* f);

/* Input Handling
 *****************************************************************************/
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

/* Define the mouse buttons used for input */
typedef struct {
    enum {
        MouseUp,
        MouseDown,
        MouseMove
    } type;
    enum {
        MOUSE_LEFT = 0,
        MOUSE_MIDDLE,
        MOUSE_RIGHT,
        MOUSE_WHEELUP,
        MOUSE_WHEELDOWN,
        MOUSE_NONE
    } button;
    int x;
    int y;
} MouseEvent;

void handle_key(Rune key);
void handle_mouse(MouseEvent* mevnt);

/* Buffer management functions
 *****************************************************************************/
typedef struct buf {
    bool insert_mode; /* tracks current mode */
    size_t bufsize;   /* size of the buffer in runes */
    Rune* bufstart;   /* start of the data buffer */
    Rune* bufend;     /* end of the data buffer */
    Rune* gapstart;   /* start of the gap */
    Rune* gapend;     /* end of the gap */
} Buf;

void buf_load(Buf* buf, char* path);
void buf_initsz(Buf* buf, size_t sz);
void buf_init(Buf* buf);
void buf_clr(Buf* buf);
void buf_del(Buf* buf, unsigned pos);
void buf_ins(Buf* buf, unsigned pos, Rune);
Rune buf_get(Buf* buf, unsigned pos);
unsigned buf_bol(Buf* buf, unsigned pos);
unsigned buf_eol(Buf* buf, unsigned pos);
unsigned buf_end(Buf* buf);
unsigned buf_byrune(Buf* buf, unsigned pos, int count);
unsigned buf_byline(Buf* buf, unsigned pos, int count);

/* Screen management functions
 *****************************************************************************/
typedef struct {
    unsigned off;  /* offset of the first rune in the row */
    unsigned rlen; /* number of runes displayed in the row */
    unsigned len;  /* number of screen columns taken up by row */
    Rune cols[];   /* row data */
} Row;

void screen_reflow(Buf* buf);
void screen_update(Buf* buf, unsigned crsr, unsigned* csrx, unsigned* csry);
unsigned screen_getoff(Buf* buf, unsigned pos, unsigned row, unsigned col);
void screen_setsize(Buf* buf, unsigned nrows, unsigned ncols);
void screen_getsize(unsigned* nrows, unsigned* ncols);
void screen_clear(void);
Row* screen_getrow(unsigned row);
void screen_clearrow(unsigned row);
void screen_setrowoff(unsigned row, unsigned off);
unsigned screen_setcell(unsigned row, unsigned col, Rune r);
Rune screen_getcell(unsigned row, unsigned col);

/* Miscellaneous Functions
 *****************************************************************************/
void die(char* msg);

/* Color Scheme Handling
 *****************************************************************************/

/* color indexes for the colorscheme */
enum ColorId {
    CLR_BASE03 = 0,
    CLR_BASE02,
    CLR_BASE01,
    CLR_BASE00,
    CLR_BASE0,
    CLR_BASE1,
    CLR_BASE2,
    CLR_BASE3,
    CLR_YELLOW,
    CLR_ORANGE,
    CLR_RED,
    CLR_MAGENTA,
    CLR_VIOLET,
    CLR_BLUE,
    CLR_CYAN,
    CLR_GREEN,
};

/* Represents an ARGB color value */
typedef uint32_t Color;

/* two colorscheme variants are supported, a light version and a dark version */
enum ColorScheme {
    DARK = 0,
    LIGHT = 1
};

/* variable for holding the currently selected color scheme */
enum ColorScheme ColorBase;

/* Configuration
 *****************************************************************************/
enum {
    Width       = 640,  /* default window width */
    Height      = 480,  /* default window height */
    TabWidth    = 4,    /* maximum number of spaces used to represent a tab */
    ScrollLines = 1,    /* number of lines to scroll by for mouse wheel scrolling */
    BufSize     = 8192, /* default buffer size */
};

static const Color Palette[][2] = {
/*   Color Name   =   Dark      Light    */
    [CLR_BASE03]  = { 0x002b36, 0xfdf6e3 },
    [CLR_BASE02]  = { 0x073642, 0xeee8d5 },
    [CLR_BASE01]  = { 0x586e75, 0x93a1a1 },
    [CLR_BASE00]  = { 0x657b83, 0x839496 },
    [CLR_BASE0]   = { 0x839496, 0x657b83 },
    [CLR_BASE1]   = { 0x93a1a1, 0x586e75 },
    [CLR_BASE2]   = { 0xeee8d5, 0x073642 },
    [CLR_BASE3]   = { 0xfdf6e3, 0x002b36 },
    [CLR_YELLOW]  = { 0xb58900, 0xb58900 },
    [CLR_ORANGE]  = { 0xcb4b16, 0xcb4b16 },
    [CLR_RED]     = { 0xdc322f, 0xdc322f },
    [CLR_MAGENTA] = { 0xd33682, 0xd33682 },
    [CLR_VIOLET]  = { 0x6c71c4, 0x6c71c4 },
    [CLR_BLUE]    = { 0x268bd2, 0x268bd2 },
    [CLR_CYAN]    = { 0x2aa198, 0x2aa198 },
    [CLR_GREEN]   = { 0x859900, 0x859900 },
};

/* choose the font to  use for xft */
#ifdef __MACH__
#define FONTNAME "Inconsolata:pixelsize=14:antialias=true:autohint=true"
#else
#define FONTNAME "Liberation Mono:pixelsize=14:antialias=true:autohint=true"
#endif

#define DEFAULT_COLORSCHEME DARK
