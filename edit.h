#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>


/* Definitons
 *****************************************************************************/
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

typedef uint32_t Color;
typedef uint32_t Rune;

/* Buffer management functions
 *****************************************************************************/
typedef struct buf {
    size_t bufsize;
    Rune* bufstart;
    Rune* bufend;
    Rune* gapstart;
    Rune* gapend;
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
    unsigned off;
    unsigned rlen;
    unsigned len;
    Rune cols[];
} Row;

void screen_setsize(unsigned nrows, unsigned ncols);
void screen_getsize(unsigned* nrows, unsigned* ncols);
void screen_clear(void);
Row* screen_getrow(unsigned row);
void screen_clearrow(unsigned row);
void screen_setrowoff(unsigned row, unsigned off);
unsigned screen_setcell(unsigned row, unsigned col, Rune r);
Rune screen_getcell(unsigned row, unsigned col);
void screen_update(Buf* buf, unsigned crsr, unsigned* csrx, unsigned* csry);

/* Miscellaneous Functions
 *****************************************************************************/
void die(char* msg);

/* UTF-8 Handling
 *****************************************************************************/
#define UTF_MAX   6u
#define RUNE_SELF ((Rune)0x80)
#define RUNE_ERR  ((Rune)0xFFFD)
#define RUNE_MAX  ((Rune)0x10FFFF)
#define RUNE_EOF  ((Rune)EOF)

size_t utf8encode(char str[UTF_MAX], Rune rune);
bool utf8decode(Rune* rune, size_t* length, int byte);

/* Configuration
 *****************************************************************************/
enum {
    Width       = 640,
    Height      = 480,
    TabWidth    = 4,
    ScrollLines = 1,
    BufSize     = 8192,
};

static enum { DARK = 0, LIGHT = 1 } ColorBase = DARK;

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

#ifdef __MACH__
#define FONTNAME "Inconsolata:pixelsize=14:antialias=true:autohint=true"
#else
#define FONTNAME "Liberation Mono:pixelsize=14:antialias=true:autohint=true"
#endif
