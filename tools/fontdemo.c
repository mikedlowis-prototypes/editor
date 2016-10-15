#define _GNU_SOURCE
#include <time.h>
#include <signal.h>
#include <locale.h>
#include <wchar.h>
#include <stdint.h>
#include <stdbool.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

uint32_t Msg1[] = {
    0x5916, 0x56fd, 0x8a9e, 0x306e, 0x5b66, 0x7fd2, 0x3068, 0x6559, 0x6388
};

uint32_t Msg2[] = {
    0x4c, 0x61, 0x6e, 0x67, 0x75, 0x61, 0x67, 0x65, 0x20, 0x4c,
    0x65, 0x61, 0x72, 0x6e, 0x69, 0x6e, 0x67, 0x20, 0x61, 0x6e,
    0x64, 0x20, 0x54, 0x65, 0x61, 0x63, 0x68, 0x69, 0x6e, 0x67
};

struct {
    Display* display;
    Visual* visual;
    Colormap colormap;
    unsigned depth;
    int screen;
    GC gc;
    Window window;
    Pixmap pixmap;
    XftDraw* xft;
    int width;
    int height;
    int rows;
    int cols;
    XIC xic;
    XIM xim;
} X;

static void die(const char* msgfmt, ...) {
    va_list args;
    va_start(args, msgfmt);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, msgfmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(EXIT_FAILURE);
}

static XftColor xftcolor(uint32_t c) {
    XftColor xc;
    xc.color.red   = ((c & 0x00FF0000) >> 8);
    xc.color.green = ((c & 0x0000FF00));
    xc.color.blue  = ((c & 0x000000FF) << 8);
    xc.color.alpha = UINT16_MAX;
    XftColorAllocValue(X.display, X.visual, X.colormap, &xc.color, &xc);
    return xc;
}

static int init(void) {
    signal(SIGPIPE, SIG_IGN); // Ignore the SIGPIPE signal
    /* open the X display and get basic attributes */
    if (!(X.display = XOpenDisplay(0)))
        die("cannot open display");
    Window root = DefaultRootWindow(X.display);
    XWindowAttributes wa;
    XGetWindowAttributes(X.display, root, &wa);
    X.visual   = wa.visual;
    X.colormap = wa.colormap;
    X.screen   = DefaultScreen(X.display);
    X.depth    = DefaultDepth(X.display, X.screen);

    /* create the main window */
    X.window = XCreateSimpleWindow(X.display, root, 0, 0, 640, 480, 0, 0, WhitePixel(X.display, X.screen));
    XSetWindowAttributes swa;
    swa.backing_store = WhenMapped;
    swa.bit_gravity = NorthWestGravity;
    XChangeWindowAttributes(X.display, X.window, CWBackingStore|CWBitGravity, &swa);
    XStoreName(X.display, X.window, "edit");
    XSelectInput(X.display, X.window, StructureNotifyMask|ExposureMask|FocusChangeMask);

    /* simulate an initial resize and map the window */
    XConfigureEvent ce;
    ce.type   = ConfigureNotify;
    ce.width  = 640;
    ce.height = 480;
    XSendEvent(X.display, X.window, False, StructureNotifyMask, (XEvent *)&ce);
    XMapWindow(X.display, X.window);

    /* set input methods */
    if((X.xim = XOpenIM(X.display, 0, 0, 0)))
        X.xic = XCreateIC(X.xim, XNInputStyle, XIMPreeditNothing|XIMStatusNothing, XNClientWindow, X.window, XNFocusWindow, X.window, NULL);

    /* initialize the graphics context */
    XGCValues gcv;
    gcv.foreground = WhitePixel(X.display, X.screen);
    gcv.graphics_exposures = False;
    X.gc = XCreateGC(X.display, X.window, GCForeground|GCGraphicsExposures, &gcv);

    /* initialize pixmap and drawing context */
    X.pixmap = XCreatePixmap(X.display, X.window, 640, 480, X.depth);
    X.xft = XftDrawCreate(X.display, X.pixmap, X.visual, X.colormap);

    return XConnectionNumber(X.display);
}

static void handle_event(XEvent* e) {
    switch (e->type) {
        case FocusIn:     if (X.xic) XSetICFocus(X.xic);   break;
        case FocusOut:    if (X.xic) XUnsetICFocus(X.xic); break;
        case ConfigureNotify: // Resize the window
            if (e->xconfigure.width != X.width || e->xconfigure.height != X.height) {
                X.width  = e->xconfigure.width;
                X.height = e->xconfigure.height;
                X.pixmap = XCreatePixmap(X.display, X.window, X.width, X.height, X.depth);
                X.xft    = XftDrawCreate(X.display, X.pixmap, X.visual, X.colormap);
            }
            break;
    }
}

/*****************************************************************************/

typedef uint32_t Rune;

static char FontName[] = "Liberation Mono:size=14:antialias=true:autohint=true";

/* globals */
#define MaxFonts 16
struct {
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
        Rune unicodep;
    } cache[MaxFonts];
    int ncached;
} Fonts;

void font_init(void);
void font_find(XftGlyphFontSpec* spec, Rune rune);
int font_makespecs(XftGlyphFontSpec* specs, const Rune* runes, int len, int x, int y);
void font_drawspecs(const XftGlyphFontSpec* specs, int len, XftColor* fg, XftColor* bg, int x, int y);

void draw_runes(unsigned x, unsigned y, XftColor* fg, XftColor* bg, Rune* runes, size_t rlen);
static void redraw(void);

int main(int argc, char** argv) {
    setlocale(LC_CTYPE, "");
    XSetLocaleModifiers("");
    init();
    font_init();
    XEvent e;
    while (true) {
        XPeekEvent(X.display,&e);
        while (XPending(X.display)) {
            XNextEvent(X.display, &e);
            if (!XFilterEvent(&e, None))
                handle_event(&e);
        }
        redraw();
    }
    return 0;
}

static void redraw(void) {
    /* Allocate the colors */
    XftColor bkgclr = xftcolor(0x002b36);
    XftColor txtclr = xftcolor(0x839496);
    /* draw the background colors */
    XftDrawRect(X.xft, &bkgclr, 0, 0, X.width, X.height);
    /* Draw text */
    draw_runes(0, 0, &txtclr, &bkgclr, (FcChar32*)(Msg1), (sizeof(Msg1)/sizeof(Msg1[0])));
    draw_runes(0, 1, &txtclr, &bkgclr, (FcChar32*)(Msg1), (sizeof(Msg1)/sizeof(Msg1[0])));
    draw_runes(0, 2, &txtclr, &bkgclr, (FcChar32*)(Msg2), (sizeof(Msg2)/sizeof(Msg2[0])));
    /* flush pixels to the screen */
    XCopyArea(X.display, X.pixmap, X.window, X.gc, 0, 0, X.width, X.height, 0, 0);
    XFlush(X.display);
}

/*****************************************************************************/

void draw_runes(unsigned x, unsigned y, XftColor* fg, XftColor* bg, Rune* runes, size_t rlen) {
    XftGlyphFontSpec specs[rlen];
    size_t nspecs = font_makespecs(specs, runes, rlen, x, y);
    font_drawspecs(specs, nspecs, fg, bg, x, y);
}

void font_init(void) {
    /* init the library and the base font pattern */
    if (!FcInit())
        die("Could not init fontconfig.\n");
    FcPattern* pattern = FcNameParse((FcChar8 *)FontName);
    if (!pattern)
        die("st: can't open font %s\n", FontName);

    /* load the base font */
    FcResult result;
    FcPattern* match = XftFontMatch(X.display, X.screen, pattern, &result);
    if (!match || !(Fonts.base.match = XftFontOpenPattern(X.display, match)))
        die("could not load default font: %s", FontName);

    /* get base font extents */
    XGlyphInfo extents;
    const FcChar8 ascii[] =
        " !\"#$%&'()*+,-./0123456789:;<=>?"
        "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
        "`abcdefghijklmnopqrstuvwxyz{|}~";
    XftTextExtentsUtf8(X.display, Fonts.base.match, ascii, sizeof(ascii), &extents);
    Fonts.base.set     = NULL;
    Fonts.base.pattern = FcPatternDuplicate(pattern);
    Fonts.base.ascent  = Fonts.base.match->ascent;
    Fonts.base.descent = Fonts.base.match->descent;
    Fonts.base.height  = Fonts.base.ascent + Fonts.base.descent;
    Fonts.base.width   = ((extents.xOff + (sizeof(ascii) - 1)) / sizeof(ascii));
    FcPatternDestroy(pattern);
}

void font_find(XftGlyphFontSpec* spec, Rune rune) {
    /* if the rune is in the base font, set it and return */
    FT_UInt glyphidx = XftCharIndex(X.display, Fonts.base.match, rune);
    if (glyphidx) {
        puts("font: default");
        spec->font  = Fonts.base.match;
        spec->glyph = glyphidx;
        return;
    }
    /* Otherwise check the cache */
    for (int f = 0; f < Fonts.ncached; f++) {
        glyphidx = XftCharIndex(X.display, Fonts.cache[f].font, rune);
        /* Fond a suitable font or found a default font */
        if (glyphidx || (!glyphidx && Fonts.cache[f].unicodep == rune)) {
            puts("font: match");
            spec->font  = Fonts.cache[f].font;
            spec->glyph = glyphidx;
            return;
        }
    }
    /* if all other options fail, ask fontconfig for a suitable font */
    FcResult fcres;
    if (!Fonts.base.set)
        Fonts.base.set = FcFontSort(0, Fonts.base.pattern, 1, 0, &fcres);
    FcFontSet* fcsets[]  = { Fonts.base.set };
    FcPattern* fcpattern = FcPatternDuplicate(Fonts.base.pattern);
    FcCharSet* fccharset = FcCharSetCreate();
    FcCharSetAddChar(fccharset, rune);
    FcPatternAddCharSet(fcpattern, FC_CHARSET, fccharset);
    FcPatternAddBool(fcpattern, FC_SCALABLE, 1);
    FcConfigSubstitute(0, fcpattern, FcMatchPattern);
    FcDefaultSubstitute(fcpattern);
    FcPattern* fontmatch = FcFontSetMatch(0, fcsets, 1, fcpattern, &fcres);
    /* add the font to the cache and use it */
    if (Fonts.ncached >= MaxFonts) {
        Fonts.ncached = MaxFonts - 1;
        XftFontClose(X.display, Fonts.cache[Fonts.ncached].font);
    }
    Fonts.cache[Fonts.ncached].font = XftFontOpenPattern(X.display, fontmatch);
    Fonts.cache[Fonts.ncached].unicodep = rune;
    spec->glyph = XftCharIndex(X.display, Fonts.cache[Fonts.ncached].font, rune);
    spec->font  = Fonts.cache[Fonts.ncached].font;
    Fonts.ncached++;
    FcPatternDestroy(fcpattern);
    FcCharSetDestroy(fccharset);
    puts("font: fontconfig");
}

int font_makespecs(XftGlyphFontSpec* specs, const Rune* runes, int len, int x, int y) {
    int winx = x * Fonts.base.width, winy = y * Fonts.base.height, xp, yp;
    int numspecs = 0;
    for (int i = 0, xp = winx, yp = winy + Fonts.base.ascent; i < len; ++i) {
        if (!runes[i]) continue;
        font_find(&(specs[numspecs]), runes[i]);
        int runewidth = wcwidth(runes[i]) * Fonts.base.width;
        specs[numspecs].x = (short)xp;
        specs[numspecs].y = (short)yp;
        xp += runewidth;
        numspecs++;
    }
    return numspecs;
}

void font_drawspecs(const XftGlyphFontSpec* specs, int len, XftColor* fg, XftColor* bg, int x, int y) {
    //int charlen = len; // * ((base.mode & ATTR_WIDE) ? 2 : 1);
    //int winx = x * Fonts.base.width, winy = y * Fonts.base.height,
    //    width = charlen * Fonts.base.width;
    //XRectangle r;

    /* Render the glyphs. */
    XftDrawGlyphFontSpec(X.xft, fg, specs, len);
}


