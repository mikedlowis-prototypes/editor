#define _GNU_SOURCE
#include <time.h>
#include <signal.h>
#include <locale.h>
#include <wchar.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include "edit.h"

Buf Buffer;
unsigned CursorPos = 0;
unsigned TargetCol = 0;
enum ColorScheme ColorBase = DEFAULT_COLORSCHEME;
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
    XftFont* font;
    int width;
    int height;
    int rows;
    int cols;
    XIC xic;
    XIM xim;
} X;
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

/*****************************************************************************/

void font_init(void) {
    /* init the library and the base font pattern */
    if (!FcInit())
        die("Could not init fontconfig.\n");
    FcPattern* pattern = FcNameParse((FcChar8 *)FONTNAME);
    if (!pattern)
        die("st: can't open font %s\n", FONTNAME);

    /* load the base font */
    FcResult result;
    FcPattern* match = XftFontMatch(X.display, X.screen, pattern, &result);
    if (!match || !(Fonts.base.match = XftFontOpenPattern(X.display, match)))
        die("could not load default font: %s", FONTNAME);

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
        spec->font  = Fonts.base.match;
        spec->glyph = glyphidx;
        return;
    }
    /* Otherwise check the cache */
    for (int f = 0; f < Fonts.ncached; f++) {
        glyphidx = XftCharIndex(X.display, Fonts.cache[f].font, rune);
        /* Fond a suitable font or found a default font */
        if (glyphidx || (!glyphidx && Fonts.cache[f].unicodep == rune)) {
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
}

int font_makespecs(XftGlyphFontSpec* specs, const UGlyph* glyphs, int len, int x, int y) {
    int winx = x * Fonts.base.width, winy = y * Fonts.base.height;
    int numspecs = 0;
    for (int i = 0, xp = winx, yp = winy + Fonts.base.ascent; i < len; ++i) {
        if (!glyphs[i].rune) continue;
        font_find(&(specs[numspecs]), glyphs[i].rune);
        int runewidth = wcwidth(glyphs[i].rune) * Fonts.base.width;
        specs[numspecs].x = xp;
        specs[numspecs].y = yp;
        xp += runewidth;
        numspecs++;
    }
    return numspecs;
}

/*****************************************************************************/

static XftColor xftcolor(enum ColorId cid) {
    Color c = Palette[cid][ColorBase];
    XftColor xc;
    xc.color.red   = ((c & 0x00FF0000) >> 8);
    xc.color.green = ((c & 0x0000FF00));
    xc.color.blue  = ((c & 0x000000FF) << 8);
    xc.color.alpha = UINT16_MAX;
    XftColorAllocValue(X.display, X.visual, X.colormap, &xc.color, &xc);
    return xc;
}

static void deinit(void) {
    if (X.pixmap != None) {
        XftDrawDestroy(X.xft);
        XFreePixmap(X.display, X.pixmap);
    }
    XCloseDisplay(X.display);
}

static int init(void) {
    atexit(deinit);
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
    X.window = XCreateSimpleWindow(X.display, root, 0, 0, Width, Height, 0, 0, WhitePixel(X.display, X.screen));
    XSetWindowAttributes swa;
    swa.backing_store = WhenMapped;
    swa.bit_gravity = NorthWestGravity;
    XChangeWindowAttributes(X.display, X.window, CWBackingStore|CWBitGravity, &swa);
    XStoreName(X.display, X.window, "edit");
    XSelectInput(X.display, X.window, StructureNotifyMask|ButtonPressMask|ButtonReleaseMask|Button1MotionMask|KeyPressMask|ExposureMask|FocusChangeMask);

    /* simulate an initial resize and map the window */
    XConfigureEvent ce;
    ce.type   = ConfigureNotify;
    ce.width  = Width;
    ce.height = Height;
    XSendEvent(X.display, X.window, False, StructureNotifyMask, (XEvent *)&ce);
    XMapWindow(X.display, X.window);

    /* allocate font */
    font_init();

    /* set input methods */
    if ((X.xim = XOpenIM(X.display, 0, 0, 0)))
        X.xic = XCreateIC(X.xim, XNInputStyle, XIMPreeditNothing|XIMStatusNothing, XNClientWindow, X.window, XNFocusWindow, X.window, NULL);

    /* initialize the graphics context */
    XGCValues gcv;
    gcv.foreground = WhitePixel(X.display, X.screen);
    gcv.graphics_exposures = False;
    X.gc = XCreateGC(X.display, X.window, GCForeground|GCGraphicsExposures, &gcv);

    /* initialize pixmap and drawing context */
    X.pixmap = XCreatePixmap(X.display, X.window, Width, Height, X.depth);
    X.xft = XftDrawCreate(X.display, X.pixmap, X.visual, X.colormap);

    return XConnectionNumber(X.display);
}

static Rune getkey(XEvent* e) {
    Rune rune = RUNE_ERR;
    size_t len = 0;
    char buf[8];
    KeySym key;
    Status status;
    /* Read the key string */
    if (X.xic)
        len = Xutf8LookupString(X.xic, &e->xkey, buf, sizeof(buf), &key, &status);
    else
        len = XLookupString(&e->xkey, buf, sizeof(buf), &key, 0);
    /* decode it */
    if (len > 0) {
        len = 0;
        if (buf[0] == '\r') buf[0] = '\n';
        for(int i = 0; i < 8 && !utf8decode(&rune, &len, buf[i]); i++);
    }
    /* translate the key code into a unicode codepoint */
    switch (key) {
        case XK_F1:        return KEY_F1;
        case XK_F2:        return KEY_F2;
        case XK_F3:        return KEY_F3;
        case XK_F4:        return KEY_F4;
        case XK_F5:        return KEY_F5;
        case XK_F6:        return KEY_F6;
        case XK_F7:        return KEY_F7;
        case XK_F8:        return KEY_F8;
        case XK_F9:        return KEY_F9;
        case XK_F10:       return KEY_F10;
        case XK_F11:       return KEY_F11;
        case XK_F12:       return KEY_F12;
        case XK_Insert:    return KEY_INSERT;
        case XK_Delete:    return KEY_DELETE;
        case XK_Home:      return KEY_HOME;
        case XK_End:       return KEY_END;
        case XK_Page_Up:   return KEY_PGUP;
        case XK_Page_Down: return KEY_PGDN;
        case XK_Up:        return KEY_UP;
        case XK_Down:      return KEY_DOWN;
        case XK_Left:      return KEY_LEFT;
        case XK_Right:     return KEY_RIGHT;
        default:           return rune;
    }
}

static MouseEvent* getmouse(XEvent* e) {
    static MouseEvent event;
    if (e->type == MotionNotify) {
        event.type   = MouseMove;
        event.button = MOUSE_NONE;
        event.x      = e->xmotion.x / Fonts.base.width;
        event.y      = e->xmotion.y / (Fonts.base.ascent + Fonts.base.descent);
    } else {
        event.type = (e->type = ButtonPress ? MouseDown : MouseUp);
        /* set the button id */
        switch (e->xbutton.button) {
            case Button1: event.button = MOUSE_LEFT;      break;
            case Button2: event.button = MOUSE_MIDDLE;    break;
            case Button3: event.button = MOUSE_RIGHT;     break;
            case Button4: event.button = MOUSE_WHEELUP;   break;
            case Button5: event.button = MOUSE_WHEELDOWN; break;
            default:      event.button = MOUSE_NONE;      break;
        }
        event.x = e->xbutton.x / Fonts.base.width;
        event.y = e->xbutton.y / (Fonts.base.ascent + Fonts.base.descent);
    }
    return &event;
}

static void handle_event(XEvent* e) {
    switch (e->type) {
        case FocusIn:     if (X.xic) XSetICFocus(X.xic);   break;
        case FocusOut:    if (X.xic) XUnsetICFocus(X.xic); break;
        case KeyPress:    handle_key(getkey(e));           break;
        case ButtonPress: handle_mouse(getmouse(e));       break;
        case ConfigureNotify: // Resize the window
            if (e->xconfigure.width != X.width || e->xconfigure.height != X.height) {
                X.width  = e->xconfigure.width;
                X.height = e->xconfigure.height;
                X.pixmap = XCreatePixmap(X.display, X.window, X.width, X.height, X.depth);
                X.xft    = XftDrawCreate(X.display, X.pixmap, X.visual, X.colormap);
                screen_setsize(
                    &Buffer,
                    X.height / Fonts.base.height,
                    X.width  / Fonts.base.width);
            }
            break;
    }
}

void draw_runes(unsigned x, unsigned y, XftColor* fg, XftColor* bg, UGlyph* glyphs, size_t rlen) {
    (void)bg;
    XftGlyphFontSpec specs[rlen];
    size_t nspecs = font_makespecs(specs, glyphs, rlen, x, y);
    XftDrawGlyphFontSpec(X.xft, fg, specs, nspecs);
}

static void redraw(void) {
    /* Allocate the colors */
    XftColor bkgclr = xftcolor(CLR_BASE03);
    XftColor gtrclr = xftcolor(CLR_BASE02);
    XftColor csrclr = xftcolor(CLR_BASE3);
    XftColor txtclr = xftcolor(CLR_BASE0);

    /* draw the background colors */
    XftDrawRect(X.xft, &bkgclr, 0, 0, X.width, X.height);
    XftDrawRect(X.xft, &gtrclr, 79 * Fonts.base.width, 0, Fonts.base.width, X.height);
    XftDrawRect(X.xft, &txtclr, 0, 0, X.width, Fonts.base.height);

    /* update the screen buffer and retrieve cursor coordinates */
    unsigned csrx, csry;
    screen_update(&Buffer, CursorPos, &csrx, &csry);

    /* flush the screen buffer */
    unsigned nrows, ncols;
    screen_getsize(&nrows, &ncols);
    screen_status(" %s %c %s",
        (Buffer.charset == BINARY ? "BIN" : "UTF-8"),
        (Buffer.modified ? '*' : ' '),
        Buffer.path
    );
    for (unsigned y = 0; y < nrows; y++) {
        Row* row = screen_getrow(y);
        draw_runes(0, y, (y == 0 ? &bkgclr : &txtclr), NULL, row->cols, row->len);
    }

    /* Place cursor on screen */
    UGlyph* csrrune = screen_getcell(csry,csrx);
    if (Buffer.insert_mode) {
        XftDrawRect(X.xft, &csrclr, csrx * Fonts.base.width, csry * Fonts.base.height, 2, Fonts.base.height);
    } else {
        unsigned width = ('\t' == buf_get(&Buffer, CursorPos) ? (TabWidth - (csrx % TabWidth)) : 1);
        XftDrawRect(X.xft, &csrclr, csrx * Fonts.base.width, csry * Fonts.base.height, width * Fonts.base.width, Fonts.base.height);
        draw_runes(csrx, csry, &bkgclr, NULL, csrrune, 1);
    }

    /* flush pixels to the screen */
    XCopyArea(X.display, X.pixmap, X.window, X.gc, 0, 0, X.width, X.height, 0, 0);
    XFlush(X.display);
}

int main(int argc, char** argv) {
    /* load the buffer */
    buf_init(&Buffer);
    if (argc > 1)
        buf_load(&Buffer, argv[1]);
    /* main x11 event loop */
    setlocale(LC_CTYPE, "");
    XSetLocaleModifiers("");
    init();
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
