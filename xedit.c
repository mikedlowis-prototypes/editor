#define _GNU_SOURCE
#include <time.h>
#include <signal.h>
#include <locale.h>
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
    unsigned fheight;
    unsigned fwidth;
    unsigned fascent;
    unsigned fdescent;
    int width;
    int height;
    int rows;
    int cols;
    XIC xic;
    XIM xim;
} X;

/*****************************************************************************/

struct XFont {
    struct XFont* next;
    XftFont* font;
    FcPattern* pattern;
}* FontList = NULL;

struct XFont* fontbyname(char* fontname) {
    struct XFont* xfont = (struct XFont*)calloc(1, sizeof(struct XFont));
    if (!(xfont->font = XftFontOpenName(X.display, X.screen, fontname)))
        die("cannot open default font");
    if (!(xfont->pattern = FcNameParse((FcChar8 *)FONTNAME)))
        die("cannot convert fontname to font pattern");
    return xfont;
}

struct XFont* fontbypatt(FcPattern* pattern) {
    struct XFont* xfont = (struct XFont*)calloc(1, sizeof(struct XFont));
    if (!(xfont->font = XftFontOpenPattern(X.display, pattern)))
        die("cannot get font by pattern");
    return xfont;
}

struct XFont* getfont(Rune r) {
    /* check to see if the font is already loaded */
    struct XFont* xfont = FontList;
    while (xfont) {
        printf("does font have rune? %d\n", XftCharExists(X.display, xfont->font, r));
        if (XftCharExists(X.display, xfont->font, r)) {
            //puts("found font in font list");
            return xfont;
        }
        xfont = xfont->next;
    }
    /* search for a new font that has the rune */
    XftResult result;
    FcCharSet* fccharset = FcCharSetCreate();
    FcCharSetAddChar(fccharset, r);
    FcPattern* fcpattern = FcPatternDuplicate(FontList->pattern);
    FcPatternAddCharSet(fcpattern, FC_CHARSET, fccharset);
    FcPatternAddBool(fcpattern, FC_SCALABLE, FcTrue);
    FcConfigSubstitute(NULL, fcpattern, FcMatchPattern);
    FcDefaultSubstitute(fcpattern);
    FcPattern* match = XftFontMatch(X.display, X.screen, fcpattern, &result);
    FcCharSetDestroy(fccharset);
    FcPatternDestroy(fcpattern);
    if (match) {
        //puts("found new font, adding it to the list");
        struct XFont* newfont  = fontbypatt(match);
        struct XFont* lastfont = FontList;
        printf("newfont: %p, %d\n", newfont->font, XftCharExists(X.display, newfont->font, r));
        if (XftCharExists(X.display, newfont->font, r)) {
            for (; lastfont->next; lastfont = lastfont->next)
                ; /* NOP */
            lastfont->next = newfont;
            return newfont;
        } else {
            XftFontClose(X.display, newfont->font);
            free(newfont);
        }
    }
    puts("failed to find a font with the given rune");
    return FontList;
}

void fontinit(char* fontstr)
{
    double fontval;
    float ceilf(float);
    /* initialize fontconfig */
    if (!FcInit())
        die("Could not init fontconfig");
    /* get pattern from font name */
    FcPattern *pattern;
    if (fontstr[0] == '-') {
        pattern = XftXlfdParse(fontstr, False, False);
    } else {
        pattern = FcNameParse((FcChar8 *)fontstr);
    }
    if (!pattern)
        die("failed to open default font");
    /* load the font by pattern */
    if (!(FontList = fontbypatt(pattern)))
        die("failed to load the font");
    FcPatternDestroy(pattern);
}


/*****************************************************************************/

void die(char* m) {
    fprintf(stderr, "dying, %s\n", m);
    exit(1);
}

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
    //fontinit(FONTNAME);
    FontList   = fontbyname(FONTNAME);
    X.fheight  = FontList->font->height;
    X.fwidth   = FontList->font->max_advance_width;
    X.fascent  = FontList->font->ascent;
    X.fdescent = FontList->font->descent;

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
        event.x      = e->xmotion.x / X.fwidth;
        event.y      = e->xmotion.y / (X.fascent + X.fdescent);
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
        event.x = e->xbutton.x / X.fwidth;
        event.y = e->xbutton.y / (X.fascent + X.fdescent);
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
                    X.height / X.fheight,
                    X.width  / X.fwidth);
            }
            break;
    }
}

#include <assert.h>

static void draw_string(XftDraw* xft, XftColor* clr, unsigned x, unsigned y, Rune* r, unsigned len) {
    struct XFont* curfont = NULL;
    for (unsigned i = 0, n = 0; i < len; i += n, n = 0) {
        //printf("i: %d, n: %d\n", i, n);
        Rune* str = r+i;
        struct XFont* newfont = NULL;
        curfont = getfont(str[n]);
        while (curfont == (newfont = getfont(str[n]))) {
            n++;
        }
        if (n) {
            //printf("printing: %p %u\n", curfont, n);
            //if (!curfont) curfont = FontList;
            XftDrawString32(xft, clr, (curfont ? curfont->font : FontList->font), x, y, str, n);
        }
    }
    //for (unsigned i = 0; i < len;) {
    //    struct XFont* newfont = NULL;
    //    Rune* curr = r+i;
    //    unsigned n = 0;
    //    while (curfont == (newfont = getfont(curr[n])))
    //        n++;
    //    if (!curfont) curfont = FontList;
    //    XftDrawString32(xft, clr, curfont->font, x, y, curr, n);
    //    curfont = newfont;
    //    i += n;
    //}
}

static void redraw(void) {
    /* Allocate the colors */
    XftColor bkgclr = xftcolor(CLR_BASE03);
    XftColor gtrclr = xftcolor(CLR_BASE02);
    XftColor csrclr = xftcolor(CLR_BASE3);
    XftColor txtclr = xftcolor(CLR_BASE0);

    /* draw the background colors */
    XftDrawRect(X.xft, &bkgclr, 0, 0, X.width, X.height);
    XftDrawRect(X.xft, &gtrclr, 79 * X.fwidth, 0, X.fwidth, X.height);
    XftDrawRect(X.xft, &txtclr, 0, 0, X.width, X.fheight + X.fdescent);

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
        draw_string(X.xft, (y == 0 ? &bkgclr : &txtclr), 0, (y+1) * X.fheight, row->cols, row->len);
    }

    /* Place cursor on screen */
    Rune csrrune = screen_getcell(csry,csrx);
    if (Buffer.insert_mode) {
        XftDrawRect(X.xft, &csrclr, csrx * X.fwidth, csry * X.fheight + X.fdescent, 2, X.fheight);
    } else {
        unsigned width = ('\t' == buf_get(&Buffer, CursorPos) ? (TabWidth - (csrx % TabWidth)) : 1);
        XftDrawRect(X.xft, &csrclr, csrx * X.fwidth, csry * X.fheight + X.fdescent, width * X.fwidth, X.fheight);
        draw_string(X.xft, &bkgclr, csrx * X.fwidth, (csry+1) * X.fheight, (FcChar32*)&csrrune, 1);
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
