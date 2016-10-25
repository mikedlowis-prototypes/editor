#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

/* Unicode Handling
 *****************************************************************************/
enum {
    UTF_MAX   = 6u,          /* maximum number of bytes that make up a rune */
    RUNE_SELF = 0x80,        /* byte values larger than this are *not* ascii */
    RUNE_ERR  = 0xFFFD,      /* rune value representing an error */
    RUNE_MAX  = 0x10FFFF,    /* Maximum decodable rune value */
    RUNE_EOF  = UINT32_MAX,  /* rune value representing end of file */
    RUNE_CRLF = UINT32_MAX-1 /* rune value representing a \r\n sequence */
};

/* Represents a unicode code point */
typedef uint32_t Rune;

size_t utf8encode(char str[UTF_MAX], Rune rune);
bool utf8decode(Rune* rune, size_t* length, int byte);
Rune fgetrune(FILE* f);
void fputrune(Rune rune, FILE* f);
int runewidth(unsigned col, Rune r);

/* Utility Functions
 *****************************************************************************/
typedef struct {
    uint8_t* buf; /* memory mapped byte buffer */
    size_t len;   /* length of the buffer */
} FMap;

FMap fmap(char* path);
void funmap(FMap file);
void die(const char* fmt, ...);
uint32_t getmillis(void);
bool isword(Rune r);


/* Buffer management functions
 *****************************************************************************/
typedef struct buf {
    char* path;       /* the path to the open file */
    int charset;      /* the character set of the buffer */
    int crlf;         /* tracks whether the file uses dos style line endings */
    bool insert_mode; /* tracks current mode */
    bool modified;    /* tracks whether the buffer has been modified */
    size_t bufsize;   /* size of the buffer in runes */
    Rune* bufstart;   /* start of the data buffer */
    Rune* bufend;     /* end of the data buffer */
    Rune* gapstart;   /* start of the gap */
    Rune* gapend;     /* end of the gap */
} Buf;

void buf_load(Buf* buf, char* path);
void buf_save(Buf* buf);
void buf_init(Buf* buf);
void buf_clr(Buf* buf);
void buf_del(Buf* buf, unsigned pos);
void buf_ins(Buf* buf, unsigned pos, Rune);
Rune buf_get(Buf* buf, unsigned pos);
bool buf_iseol(Buf* buf, unsigned pos);
unsigned buf_bol(Buf* buf, unsigned pos);
unsigned buf_eol(Buf* buf, unsigned pos);
unsigned buf_bow(Buf* buf, unsigned pos);
unsigned buf_eow(Buf* buf, unsigned pos);
unsigned buf_lscan(Buf* buf, unsigned pos, Rune r);
unsigned buf_rscan(Buf* buf, unsigned pos, Rune r);
void buf_find(Buf* buf, unsigned* beg, unsigned* end);
unsigned buf_end(Buf* buf);
unsigned buf_byrune(Buf* buf, unsigned pos, int count);
unsigned buf_byline(Buf* buf, unsigned pos, int count);
unsigned buf_getcol(Buf* buf, unsigned pos);
unsigned buf_setcol(Buf* buf, unsigned pos, unsigned col);

/* Charset Handling
 *****************************************************************************/
enum {
    BINARY = 0, /* binary encoded file */
    UTF_8,      /* UTF-8 encoded file */
    UTF_16BE,   /* UTF-16 encoding, big-endian */
    UTF_16LE,   /* UTF-16 encoding, little-endian */
    UTF_32BE,   /* UTF-32 encoding, big-endian */
    UTF_32LE,   /* UTF-32 encoding, little-endian */
};

int charset(const uint8_t* buf, size_t len, int* crlf);
void utf8load(Buf* buf, FMap file);
void utf8save(Buf* buf, FILE* file);
void binload(Buf* buf, FMap file);
void binsave(Buf* buf, FILE* file);

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

/* Screen management functions
 *****************************************************************************/
typedef struct {
    uint32_t attr; /* attributes  applied to this cell */
    Rune rune;     /* rune value for the cell */
} UGlyph;

typedef struct {
    unsigned off;  /* offset of the first rune in the row */
    unsigned rlen; /* number of runes displayed in the row */
    unsigned len;  /* number of screen columns taken up by row */
    UGlyph cols[]; /* row data */
} Row;

void screen_update(Buf* buf, unsigned crsr, unsigned* csrx, unsigned* csry);
unsigned screen_getoff(Buf* buf, unsigned pos, unsigned row, unsigned col);
void screen_setsize(Buf* buf, unsigned nrows, unsigned ncols);
void screen_getsize(unsigned* nrows, unsigned* ncols);
Row* screen_getrow(unsigned row);
void screen_clearrow(unsigned row);
unsigned screen_setcell(unsigned row, unsigned col, uint32_t attr, Rune r);
UGlyph* screen_getglyph(unsigned row, unsigned col, unsigned* scrwidth);

/* Color Scheme Handling
 *****************************************************************************/
/* color indexes for the colorscheme */
enum ColorId {
    CLR_BASE03  = 0,
    CLR_BASE02  = 1,
    CLR_BASE01  = 2,
    CLR_BASE00  = 3,
    CLR_BASE0   = 4,
    CLR_BASE1   = 5,
    CLR_BASE2   = 6,
    CLR_BASE3   = 7,
    CLR_YELLOW  = 8,
    CLR_ORANGE  = 9,
    CLR_RED     = 10,
    CLR_MAGENTA = 11,
    CLR_VIOLET  = 12,
    CLR_BLUE    = 13,
    CLR_CYAN    = 14,
    CLR_GREEN   = 15,
    CLR_COUNT   = 16
};

/* Represents an ARGB color value */
typedef uint32_t Color;

/* two colorscheme variants are supported, a light version and a dark version */
enum ColorScheme {
    DARK = 0,
    LIGHT = 1
};

/* Global State
 *****************************************************************************/
/* variable for holding the currently selected color scheme */
extern enum ColorScheme ColorBase;
extern Buf Buffer;
extern unsigned TargetCol;
extern unsigned SelBeg;
extern unsigned SelEnd;

/* Configuration
 *****************************************************************************/
enum {
    Width       = 640,  /* default window width */
    Height      = 480,  /* default window height */
    TabWidth    = 4,    /* maximum number of spaces used to represent a tab */
    ScrollLines = 1,    /* number of lines to scroll by for mouse wheel scrolling */
    BufSize     = 8192, /* default buffer size */
    MaxFonts    = 16    /* maximum number of fonts to cache */
};

static const Color ColorScheme[CLR_COUNT][2] = {
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
#define FONTNAME "Monaco:size=10:antialias=true:autohint=true"
#else
#define FONTNAME "Liberation Mono:size=10.5:antialias=true:autohint=true"
#endif

#define DEFAULT_COLORSCHEME DARK
#define DEFAULT_CRLF 1
#define DEFAULT_CHARSET UTF_8

