#include <stdc.h>
#include <X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <utf.h>
#include <locale.h>

static struct {
    Window root;
    Display* display;
    Visual* visual;
    Colormap colormap;
    unsigned depth;
    int screen;
    /* assume a single window for now. these are it's attributes */
    Window window;
    XftDraw* xft;
    Pixmap pixmap;
    int width;
    int height;
    XIC xic;
    XIM xim;
    GC gc;
} X;
static XConfig* Config;

static void xftcolor(XftColor* xc, uint32_t c) {
    xc->color.alpha = 0xFF | ((c & 0xFF000000) >> 16);
    xc->color.red   = 0xFF | ((c & 0x00FF0000) >> 8);
    xc->color.green = 0xFF | ((c & 0x0000FF00));
    xc->color.blue  = 0xFF | ((c & 0x000000FF) << 8);
    XftColorAllocValue(X.display, X.visual, X.colormap, &(xc->color), xc);
}

void x11_deinit(void) {
    XCloseDisplay(X.display);
}

void x11_init(XConfig* cfg) {
    atexit(x11_deinit);
    signal(SIGPIPE, SIG_IGN); // Ignore the SIGPIPE signal
    setlocale(LC_CTYPE, "");
    XSetLocaleModifiers("");
    /* open the X display and get basic attributes */
    Config = cfg;
    if (!(X.display = XOpenDisplay(0)))
        assert(false);
    X.root = DefaultRootWindow(X.display);
    XWindowAttributes wa;
    XGetWindowAttributes(X.display, X.root, &wa);
    X.visual   = wa.visual;
    X.colormap = wa.colormap;
    X.screen   = DefaultScreen(X.display);
    X.depth    = DefaultDepth(X.display, X.screen);
}

void x11_window(char* name, int width, int height) {
    /* create the main window */
    X.width  = width ;
    X.height = height;
    XWindowAttributes wa;
    XGetWindowAttributes(X.display, X.root, &wa);
    X.window = XCreateSimpleWindow(X.display, X.root,
        (wa.width  - X.width) / 2,
        (wa.height - X.height) /2,
        X.width,
        X.height,
        0, X.depth,
        Config->palette[0]);
    XSetWindowAttributes swa;
    swa.backing_store = WhenMapped;
    swa.bit_gravity = NorthWestGravity;
    XChangeWindowAttributes(X.display, X.window, CWBackingStore|CWBitGravity, &swa);
    XStoreName(X.display, X.window, name);
    XSelectInput(X.display, X.window,
          StructureNotifyMask
        | ButtonPressMask
        | ButtonReleaseMask
        | ButtonMotionMask
        | KeyPressMask
        | ExposureMask
        | FocusChangeMask);
    /* set input methods */
    if ((X.xim = XOpenIM(X.display, 0, 0, 0)))
        X.xic = XCreateIC(X.xim, XNInputStyle, XIMPreeditNothing|XIMStatusNothing, XNClientWindow, X.window, XNFocusWindow, X.window, NULL);
    /* initialize pixmap and drawing context */
    X.pixmap = XCreatePixmap(X.display, X.window, width, height, X.depth);
    X.xft    = XftDrawCreate(X.display, X.pixmap, X.visual, X.colormap);

    /* initialize the graphics context */
    XGCValues gcv;
    gcv.foreground = WhitePixel(X.display, X.screen);
    gcv.graphics_exposures = False;
    X.gc = XCreateGC(X.display, X.window, GCForeground|GCGraphicsExposures, &gcv);
}

void x11_dialog(char* name, int height, int width) {
    x11_window(name, height, width);
    Atom WindowType = XInternAtom(X.display, "_NET_WM_WINDOW_TYPE", False);
    Atom DialogType = XInternAtom(X.display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    XChangeProperty(X.display, X.window, WindowType, XA_ATOM, 32, PropModeReplace, (unsigned char*)&DialogType, 1);
}

void x11_show(void) {
    /* simulate an initial resize and map the window */
    XConfigureEvent ce;
    ce.type   = ConfigureNotify;
    ce.width  = X.width;
    ce.height = X.height;
    XSendEvent(X.display, X.window, False, StructureNotifyMask, (XEvent *)&ce);
    XMapWindow(X.display, X.window);
}

static uint32_t getkey(XEvent* e) {
    uint32_t rune = 0xFFFD;
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

static void handle_mouse(XEvent* e) {
    MouseAct action;
    MouseBtn button;
    int x = 0, y = 0;
    if (e->type == MotionNotify) {
        action = MOUSE_ACT_MOVE;
        button = MOUSE_BTN_LEFT;
        x      = e->xmotion.x;
        y      = e->xmotion.y;
    } else {
        action = (e->type = ButtonPress ? MOUSE_ACT_DOWN : MOUSE_ACT_UP);
        /* set the button id */
        switch (e->xbutton.button) {
            case Button1: button = MOUSE_BTN_LEFT;      break;
            case Button2: button = MOUSE_BTN_MIDDLE;    break;
            case Button3: button = MOUSE_BTN_RIGHT;     break;
            case Button4: button = MOUSE_BTN_WHEELUP;   break;
            case Button5: button = MOUSE_BTN_WHEELDOWN; break;
            default:      button = MOUSE_BTN_NONE;      break;
        }
        x = e->xbutton.x;
        y = e->xbutton.y;
    }
    /* pass the data to the app */
    Config->handle_mouse(action, button, x, y);
}

void x11_loop(void) {
    XEvent e;
    while (true) {
        XPeekEvent(X.display,&e);
        while (XPending(X.display)) {
            XNextEvent(X.display, &e);
            if (!XFilterEvent(&e, None))
                switch (e.type) {
                    case FocusIn:      if (X.xic) XSetICFocus(X.xic);   break;
                    case FocusOut:     if (X.xic) XUnsetICFocus(X.xic); break;
                    case KeyPress:     Config->handle_key(getkey(&e));  break;
                    case ButtonPress:  handle_mouse(&e);                break;
                    case MotionNotify: handle_mouse(&e);                break;
                    case ConfigureNotify: // Resize the window
                        if (e.xconfigure.width != X.width || e.xconfigure.height != X.height) {
                            X.width  = e.xconfigure.width;
                            X.height = e.xconfigure.height;
                            X.pixmap = XCreatePixmap(X.display, X.window, X.width, X.height, X.depth);
                            X.xft    = XftDrawCreate(X.display, X.pixmap, X.visual, X.colormap);
                        }
                        break;
                }
        }
        /* redraw the window */
        Config->redraw(X.width, X.height);
        XCopyArea(X.display, X.pixmap, X.window, X.gc, 0, 0, X.width, X.height, 0, 0);
        XFlush(X.display);
    }
}

void x11_draw_rect(int color, int x, int y, int width, int height) {
    XftColor clr;
    xftcolor(&clr, Config->palette[color]);
    XftDrawRect(X.xft, &clr, x, y, width, height);
    XftColorFree(X.display, X.visual, X.colormap, &clr);
}

void x11_getsize(int* width, int* height) {
    *width  = X.width;
    *height = X.height;
}

void x11_font_load(XFont* font, char* name) {
    /* init the library and the base font pattern */
    if (!FcInit())
        die("Could not init fontconfig.\n");
    FcPattern* pattern = FcNameParse((FcChar8 *)name);
    if (!pattern)
        die("st: can't open font %s\n", name);

    /* load the base font */
    FcResult result;
    FcPattern* match = XftFontMatch(X.display, X.screen, pattern, &result);
    if (!match || !(font->base.match = XftFontOpenPattern(X.display, match)))
        die("could not load default font: %s", name);

    /* get base font extents */
    XGlyphInfo extents;
    const FcChar8 ascii[] =
        " !\"#$%&'()*+,-./0123456789:;<=>?"
        "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
        "`abcdefghijklmnopqrstuvwxyz{|}~";
    XftTextExtentsUtf8(X.display, font->base.match, ascii, sizeof(ascii), &extents);
    font->base.set     = NULL;
    font->base.pattern = FcPatternDuplicate(pattern);
    font->base.ascent  = font->base.match->ascent;
    font->base.descent = font->base.match->descent;
    font->base.height  = font->base.ascent + font->base.descent;
    font->base.width   = ((extents.xOff + (sizeof(ascii) - 1)) / sizeof(ascii));
    FcPatternDestroy(pattern);
}

void x11_font_getglyph(XFont* font, XftGlyphFontSpec* spec, uint32_t rune) {
    /* if the rune is in the base font, set it and return */
    FT_UInt glyphidx = XftCharIndex(X.display, font->base.match, rune);
    if (glyphidx) {
        spec->font  = font->base.match;
        spec->glyph = glyphidx;
        return;
    }
    /* Otherwise check the cache */
    for (int f = 0; f < font->ncached; f++) {
        glyphidx = XftCharIndex(X.display, font->cache[f].font, rune);
        /* Fond a suitable font or found a default font */
        if (glyphidx || (!glyphidx && font->cache[f].unicodep == rune)) {
            spec->font  = font->cache[f].font;
            spec->glyph = glyphidx;
            return;
        }
    }
    /* if all other options fail, ask fontconfig for a suitable font */
    FcResult fcres;
    if (!font->base.set)
        font->base.set = FcFontSort(0, font->base.pattern, 1, 0, &fcres);
    FcFontSet* fcsets[]  = { font->base.set };
    FcPattern* fcpattern = FcPatternDuplicate(font->base.pattern);
    FcCharSet* fccharset = FcCharSetCreate();
    FcCharSetAddChar(fccharset, rune);
    FcPatternAddCharSet(fcpattern, FC_CHARSET, fccharset);
    FcPatternAddBool(fcpattern, FC_SCALABLE, 1);
    FcConfigSubstitute(0, fcpattern, FcMatchPattern);
    FcDefaultSubstitute(fcpattern);
    FcPattern* fontmatch = FcFontSetMatch(0, fcsets, 1, fcpattern, &fcres);
    /* add the font to the cache and use it */
    if (font->ncached >= MAXFONTS) {
        font->ncached = MAXFONTS - 1;
        XftFontClose(X.display, font->cache[font->ncached].font);
    }
    font->cache[font->ncached].font = XftFontOpenPattern(X.display, fontmatch);
    font->cache[font->ncached].unicodep = rune;
    spec->glyph = XftCharIndex(X.display, font->cache[font->ncached].font, rune);
    spec->font  = font->cache[font->ncached].font;
    font->ncached++;
    FcPatternDestroy(fcpattern);
    FcCharSetDestroy(fccharset);
}

void x11_draw_glyphs(int fg, int bg, XftGlyphFontSpec* specs, size_t nspecs) {
    if (!nspecs) return;
    XftFont* font = specs[0].font;
    XftColor fgc, bgc;
    if (bg > 0) {
        XGlyphInfo extent;
        XftTextExtentsUtf8(X.display, font, (const FcChar8*)"0", 1, &extent);
        int w = extent.xOff;
        int h = (font->height - font->descent);
        xftcolor(&bgc, Config->palette[bg]);
        x11_draw_rect(bg, specs[0].x, specs[0].y - h, nspecs * w, font->height);
        XftColorFree(X.display, X.visual, X.colormap, &bgc);
    }
    xftcolor(&fgc, Config->palette[fg]);
    XftDrawGlyphFontSpec(X.xft, &fgc, specs, nspecs);
    XftColorFree(X.display, X.visual, X.colormap, &fgc);
}

size_t x11_font_getglyphs(XftGlyphFontSpec* specs, const XGlyph* glyphs, int len, XFont* font, int x, int y) {
    int winx = x * font->base.width, winy = y * font->base.ascent;
    size_t numspecs = 0;
    for (int i = 0, xp = winx, yp = winy + font->base.ascent; i < len;) {
        x11_font_getglyph(font, &(specs[numspecs]), glyphs[i].rune);
        specs[numspecs].x = xp;
        specs[numspecs].y = yp;
        xp += font->base.width;
        numspecs++;
        i++;
        /* skip over null chars which mark multi column runes */
        for (; i < len && !glyphs[i].rune; i++)
            xp += font->base.width;
    }
    return numspecs;
}

void x11_warp_mouse(int x, int y) {
    XWarpPointer(X.display, X.window, X.window, 0, 0, X.width, X.height, x, y);
}
