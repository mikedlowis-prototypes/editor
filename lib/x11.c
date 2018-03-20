#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <stdc.h>
#include <x11.h>
#include <utf.h>
#include <edit.h>
#include <locale.h>
#include <sys/time.h>
#include <sys/types.h>

enum { FontCacheSize = 16 };

static struct XSel* selfetch(Atom atom);
static void selclear(XEvent* evnt);
static void selnotify(XEvent* evnt);
static void selrequest(XEvent* evnt);
static void propnotify(XEvent* evnt);
static char* readprop(Display* disp, Window win, Atom prop);

struct XFont {
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
    } cache[FontCacheSize];
    int ncached;
};

static bool Running = true;
static struct {
    Window root;
    Display* display;
    Visual* visual;
    Colormap colormap;
    unsigned depth;
    int screen;
    /* assume a single window for now. these are its attributes */
    Window self;
    XftDraw* xft;
    Pixmap pixmap;
    int width;
    int height;
    XIC xic;
    XIM xim;
    GC gc;
} X;
static XConfig* Config;
static int KeyBtnState;
static Atom SelTarget;
static struct XSel {
    char* name;
    Atom atom;
    char* text;
    void (*callback)(char*);
} Selections[] = {
    { .name = "PRIMARY" },
    { .name = "CLIPBOARD" },
};

static void xftcolor(XftColor* xc, int id) {
    #define COLOR(c) ((c) | ((c) >> 8))
    uint32_t c = config_get_int(id + Color00);
    xc->color.alpha = COLOR((c & 0xFF000000) >> 16);
    xc->color.red   = COLOR((c & 0x00FF0000) >> 8);
    xc->color.green = COLOR((c & 0x0000FF00));
    xc->color.blue  = COLOR((c & 0x000000FF) << 8);
    XftColorAllocValue(X.display, X.visual, X.colormap, &(xc->color), xc);
}

void x11_deinit(void) {
    Running = false;
}

void x11_init(XConfig* cfg) {
    atexit(x11_deinit);
    signal(SIGPIPE, SIG_IGN); // Ignore the SIGPIPE signal
    setlocale(LC_CTYPE, "");
    XSetLocaleModifiers("");
    /* open the X display and get basic attributes */
    Config = cfg;
    if (!(X.display = XOpenDisplay(0)))
        die("could not open display");
    X.root = DefaultRootWindow(X.display);
    XWindowAttributes wa;
    XGetWindowAttributes(X.display, X.root, &wa);
    X.visual   = wa.visual;
    X.colormap = wa.colormap;
    X.screen   = DefaultScreen(X.display);
    X.depth    = DefaultDepth(X.display, X.screen);

    /* initialize selection atoms */
    for (int i = 0; i < (sizeof(Selections) / sizeof(Selections[0])); i++)
        Selections[i].atom = XInternAtom(X.display, Selections[i].name, 0);
    SelTarget = XInternAtom(X.display, "UTF8_STRING", 0);
    if (SelTarget == None)
        SelTarget = XInternAtom(X.display, "STRING", 0);
    config_init(X.display);
}

int x11_connfd(void) {
    return ConnectionNumber(X.display);
}

int x11_keybtnstate(void) {
    return KeyBtnState;
}

bool x11_keymodsset(int mask) {
    return ((KeyBtnState & mask) == mask);
}

void x11_window(char* name, int width, int height) {
    /* create the main window */
    X.width  = width ;
    X.height = height;
    XWindowAttributes wa;
    XGetWindowAttributes(X.display, X.root, &wa);
    X.self = XCreateSimpleWindow(X.display, X.root,
        (wa.width  - X.width) / 2,
        (wa.height - X.height) /2,
        X.width,
        X.height,
        0, X.depth,
        config_get_int(Color00));

    /* register interest in the delete window message */
    Atom wmDeleteMessage = XInternAtom(X.display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(X.display, X.self, &wmDeleteMessage, 1);

    /* setup window attributes and events */
    XSetWindowAttributes swa;
    swa.backing_store = WhenMapped;
    swa.bit_gravity = NorthWestGravity;
    XChangeWindowAttributes(X.display, X.self, CWBackingStore|CWBitGravity, &swa);
    XStoreName(X.display, X.self, name);
    XSelectInput(X.display, X.root, PropertyChangeMask);
    XSelectInput(X.display, X.self,
          StructureNotifyMask
        | ButtonPressMask
        | ButtonReleaseMask
        | ButtonMotionMask
        | KeyPressMask
        | FocusChangeMask
        | PropertyChangeMask
    );

    /* set input methods */
    if ((X.xim = XOpenIM(X.display, 0, 0, 0)))
        X.xic = XCreateIC(X.xim, XNInputStyle, XIMPreeditNothing|XIMStatusNothing, XNClientWindow, X.self, XNFocusWindow, X.self, NULL);

    /* initialize pixmap and drawing context */
    X.pixmap = XCreatePixmap(X.display, X.self, width, height, X.depth);
    X.xft    = XftDrawCreate(X.display, X.pixmap, X.visual, X.colormap);

    /* initialize the graphics context */
    XGCValues gcv;
    gcv.foreground = WhitePixel(X.display, X.screen);
    gcv.graphics_exposures = False;
    X.gc = XCreateGC(X.display, X.self, GCForeground|GCGraphicsExposures, &gcv);

    /* Create and updtate tide-specific properties */
    Atom prop = XInternAtom(X.display, "TIDE_WINDOWS", False);
    XChangeProperty(X.display, X.root, prop, XA_WINDOW, 32, PropModeAppend, (unsigned char*)&X.self, 1);
    prop = XInternAtom(X.display, "TIDE_COMM", False);
    XChangeProperty(X.display, X.self, prop, XA_STRING, 8, PropModeReplace, (unsigned char*)"", 0);
}

void x11_dialog(char* name, int height, int width) {
    x11_window(name, height, width);
    Atom WindowType = XInternAtom(X.display, "_NET_WM_WINDOW_TYPE", False);
    Atom DialogType = XInternAtom(X.display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    XChangeProperty(X.display, X.self, WindowType, XA_ATOM, 32, PropModeReplace, (unsigned char*)&DialogType, 1);
}

void x11_show(void) {
    /* simulate an initial resize and map the window */
    XConfigureEvent ce;
    ce.type   = ConfigureNotify;
    ce.width  = X.width;
    ce.height = X.height;
    XSendEvent(X.display, X.self, False, StructureNotifyMask, (XEvent *)&ce);
    XMapWindow(X.display, X.self);
}

bool x11_running(void) {
    return Running;
}

void x11_flip(void) {
    Config->redraw(X.width, X.height);
    XCopyArea(X.display, X.pixmap, X.self, X.gc, 0, 0, X.width, X.height, 0, 0);
    x11_flush();
}

void x11_flush(void) {
    XFlush(X.display);
}

void x11_finish(void) {
    XCloseDisplay(X.display);
    /* we're exiting now. If we own the clipboard, make sure it persists */
    if (Selections[CLIPBOARD].text) {
        char* text = Selections[CLIPBOARD].text;
        size_t len = strlen(text);
        exec_job((char*[]){ "xcpd", NULL }, text, len, NULL);
        while (event_poll(100));
    }
}

/******************************************************************************/

static uint32_t special_keys(uint32_t key) {
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
        case XK_Escape:    return KEY_ESCAPE;
        case XK_BackSpace: return '\b';
        case XK_Tab:       return '\t';
        case XK_Return:    return '\r';
        case XK_Linefeed:  return '\n';

        /* modifiers should not trigger key presses */
        case XK_Scroll_Lock:
        case XK_Shift_L:
        case XK_Shift_R:
        case XK_Control_L:
        case XK_Control_R:
        case XK_Caps_Lock:
        case XK_Shift_Lock:
        case XK_Meta_L:
        case XK_Meta_R:
        case XK_Alt_L:
        case XK_Alt_R:
        case XK_Super_L:
        case XK_Super_R:
        case XK_Hyper_L:
        case XK_Hyper_R:
        case XK_Menu:
            return RUNE_ERR;

        /* if it ain't special, don't touch it */
        default:
            return key;
    }
}

static uint32_t getkey(XEvent* e) {
    int32_t rune = RUNE_ERR;
    size_t len = 0;
    char buf[8];
    KeySym key;
    Status status;
    /* Read the key string */
    if (X.xic)
        len = Xutf8LookupString(X.xic, &(e->xkey), buf, sizeof(buf), &key, &status);
    else
        len = XLookupString(&(e->xkey), buf, sizeof(buf), &key, 0);
    /* if it's ascii, just return it */
    if (key >= 0x20 && key <= 0x7F)
        return (uint32_t)key;
    /* decode it */
    if (len > 0) {
        len = 0;
        for (int i = 0; i < 8 && !utf8decode(&rune, &len, buf[i]); i++);
    }
    /* translate special key codes into unicode codepoints */
    rune = special_keys(key);
    return rune;
}

static void handle_key(XEvent* event) {
    uint32_t key = getkey(event);
    KeyBtnState = event->xkey.state;
    if (key == RUNE_ERR) return;
    Config->handle_key(KeyBtnState, key);
}

static void handle_mouse(XEvent* e) {
    KeyBtnState = e->xbutton.state;
    int x = e->xbutton.x;
    int y = e->xbutton.y;

    if (e->type == MotionNotify) {
        Config->mouse_drag(KeyBtnState, x, y);
    } else {
        if (e->type == ButtonRelease)
            KeyBtnState &= ~(1 << (e->xbutton.button + 7));
        else
            KeyBtnState |= (1 << (e->xbutton.button + 7));
        Config->mouse_btn(e->xbutton.button, (e->type == ButtonPress), x, y);
    }
}

static void set_focus(bool focused) {
    if (X.xic)
        (focused ? XSetICFocus : XUnsetICFocus)(X.xic);
    Config->set_focus(focused);
}

void x11_handle_event(XEvent* e) {
    Atom wmDeleteMessage = XInternAtom(X.display, "WM_DELETE_WINDOW", False);
    switch (e->type) {
        case FocusIn:          set_focus(true);  break;
        case FocusOut:         set_focus(false); break;
        case KeyPress:         handle_key(e);    break;
        case ButtonRelease:    handle_mouse(e);  break;
        case ButtonPress:      handle_mouse(e);  break;
        case MotionNotify:     handle_mouse(e);  break;
        case SelectionClear:   selclear(e);      break;
        case SelectionNotify:  selnotify(e);     break;
        case SelectionRequest: selrequest(e);    break;
        case PropertyNotify:   propnotify(e);    break;
        case ClientMessage:
            if (e->xclient.data.l[0] == wmDeleteMessage)
                Config->shutdown();
            break;
        case ConfigureNotify: // Resize the window
            if (e->xconfigure.width != X.width || e->xconfigure.height != X.height) {
                X.width  = e->xconfigure.width;
                X.height = e->xconfigure.height;
                X.pixmap = XCreatePixmap(X.display, X.self, X.width, X.height, X.depth);
                X.xft    = XftDrawCreate(X.display, X.pixmap, X.visual, X.colormap);
            }
            break;
    }
}

int x11_events_queued(void) {
    return XEventsQueued(X.display, QueuedAfterFlush);
}

void x11_events_take(void) {
    XEvent e;
    int nevents;
    XGetMotionEvents(X.display, X.self, CurrentTime, CurrentTime, &nevents);
    while (XPending(X.display)) {
        XNextEvent(X.display, &e);
        if (!XFilterEvent(&e, None))
            x11_handle_event(&e);
    }
}

void x11_mouse_set(int x, int y) {
    XWarpPointer(X.display, X.self, X.self, 0, 0, X.width, X.height, x, y);
}

void x11_mouse_get(int* ptrx, int* ptry) {
    Window xw; int x; unsigned int ux;
    XQueryPointer(X.display, X.self, &xw, &xw, &x, &x, ptrx, ptry, &ux);
}

XFont x11_font_load(char* name) {
    struct XFont* font = calloc(1, sizeof(struct XFont));
    /* init the library and the base font pattern */
    if (!FcInit())
        die("Could not init fontconfig.\n");
    FcPattern* pattern = FcNameParse((FcChar8 *)name);
    if (!pattern)
        die("can't open font %s\n", name);

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
    return font;
}

size_t x11_font_height(XFont fnt) {
    struct XFont* font = fnt;
    return font->base.height;
}

size_t x11_font_width(XFont fnt) {
    struct XFont* font = fnt;
    return font->base.width;
}

size_t x11_font_descent(XFont fnt) {
    struct XFont* font = fnt;
    return font->base.descent;
}

void x11_draw_rect(int color, int x, int y, int width, int height) {
    XftColor clr;
    xftcolor(&clr, color);
    XftDrawRect(X.xft, &clr, x, y, width, height);
    XftColorFree(X.display, X.visual, X.colormap, &clr);
}

void x11_font_getglyph(XFont fnt, XGlyphSpec* spec, uint32_t rune) {
    struct XFont* font = fnt;
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
        /* Found a suitable font or found a default font */
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
    if (font->ncached >= FontCacheSize) {
        font->ncached = FontCacheSize - 1;
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

size_t x11_font_getglyphs(XGlyphSpec* specs, const XGlyph* glyphs, int len, XFont fnt, int x, int y) {
    struct XFont* font = fnt;
    int winx = x, winy = y;
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

void x11_draw_glyphs(int fg, int bg, XGlyphSpec* specs, size_t nspecs, bool eol) {
    if (!nspecs) return;
    XftFont* font = specs[0].font;
    XftColor fgc, bgc;

    if (bg > 0) {
        XGlyphInfo extent;
        XftTextExtentsUtf8(X.display, font, (const FcChar8*)"0", 1, &extent);
        int w = extent.xOff;
        int h = (font->height - font->descent) + config_get_int(LineSpacing);
        xftcolor(&bgc, bg);
        size_t width = specs[nspecs-1].x - specs[0].x + w;
        if (eol) width = X.width - specs[0].x;
        x11_draw_rect(bg, specs[0].x, specs[0].y - h, width, font->height + config_get_int(LineSpacing));
        XftColorFree(X.display, X.visual, X.colormap, &bgc);
    }
    xftcolor(&fgc, fg);
    XftDrawGlyphFontSpec(X.xft, &fgc, (XftGlyphFontSpec*)specs, nspecs);
    XftColorFree(X.display, X.visual, X.colormap, &fgc);
}

/* Selection Handling
 *****************************************************************************/

static struct XSel* selfetch(Atom atom) {
    for (int i = 0; i < (sizeof(Selections) / sizeof(Selections[0])); i++)
        if (atom == Selections[i].atom)
            return &Selections[i];
    return NULL;
}

static void selclear(XEvent* evnt) {
    struct XSel* sel = selfetch(evnt->xselectionrequest.selection);
    if (!sel) return;
    free(sel->text);
    sel->text = NULL;
}

static void selnotify(XEvent* evnt) {
    /* bail if the selection cannot be converted */
    if (evnt->xselection.property == None)
        return;
    struct XSel* sel = selfetch( evnt->xselection.selection );
    char* propdata = readprop(X.display, X.self, sel->atom);
    if (evnt->xselection.target == SelTarget) {
        void(*cbfn)(char*) = sel->callback;
        sel->callback = NULL;
        cbfn(propdata);
    }
    /* cleanup */
    if (propdata) XFree(propdata);
}

static void selrequest(XEvent* evnt) {
    XEvent s;
    struct XSel* sel = selfetch( evnt->xselectionrequest.selection );
    s.xselection.type      = SelectionNotify;
    s.xselection.property  = evnt->xselectionrequest.property;
    s.xselection.requestor = evnt->xselectionrequest.requestor;
    s.xselection.selection = evnt->xselectionrequest.selection;
    s.xselection.target    = evnt->xselectionrequest.target;
    s.xselection.time      = evnt->xselectionrequest.time;

    Atom target    = evnt->xselectionrequest.target;
    Atom xatargets = XInternAtom(X.display, "TARGETS", 0);
    Atom xastring  = XInternAtom(X.display, "STRING", 0);
    if (target == xatargets) {
        /* respond with the supported type */
        XChangeProperty(
            X.display,
            s.xselection.requestor,
            s.xselection.property,
            XA_ATOM, 32, PropModeReplace,
            (unsigned char*)&SelTarget, 1);
    } else if (target == SelTarget || target == xastring) {
        XChangeProperty(
            X.display,
            s.xselection.requestor,
            s.xselection.property,
            SelTarget, 8, PropModeReplace,
            (unsigned char*)sel->text, strlen(sel->text));
    }
    XSendEvent(X.display, s.xselection.requestor, True, 0, &s);
}

bool x11_sel_get(int selid, void(*cbfn)(char*)) {
    struct XSel* sel = &(Selections[selid]);
    if (sel->callback) return false;
    Window owner = XGetSelectionOwner(X.display, sel->atom);
    if (owner == X.self) {
        cbfn(sel->text);
    } else if (owner != None){
        sel->callback = cbfn;
        XConvertSelection(X.display, sel->atom, SelTarget, sel->atom, X.self, CurrentTime);
    }
    return true;
}

bool x11_sel_set(int selid, char* str) {
    struct XSel* sel = &(Selections[selid]);
    if (!sel || !str || !*str) {
        free(str);
        return false;
    } else {
        sel->text = str;
        XSetSelectionOwner(X.display, sel->atom, X.self, CurrentTime);
        return true;
    }
}

/* Tide Server Communication and Property Handling
 *****************************************************************************/
static void propnotify(XEvent* evnt) {
    XPropertyEvent* ev = (XPropertyEvent*)evnt;
    if (ev->atom == XInternAtom(X.display, "TIDE_COMM", False))
        Config->cmd_received(x11_prop_get("TIDE_COMM"));
}

static char* readprop(Display* disp, Window win, Atom prop) {
    Atom rtype;
    unsigned long format = 0, nitems = 0, nleft = 0, nread = 0;
    unsigned char* data = NULL;
    XGetWindowProperty(X.display, win, prop, 0, -1, False, AnyPropertyType, &rtype,
                       (int*)&format, &nitems, &nleft, &data);
    return (char*)data;
}

void x11_prop_set(char* name, char* val) {
    Atom prop = XInternAtom(X.display, name, False);
    XChangeProperty(X.display, X.self, prop, XA_STRING, 8, PropModeReplace, (unsigned char*)val, strlen(val)+1);
}

char* x11_prop_get(char* name) {
    return readprop(X.display, X.self, XInternAtom(X.display, name, False));
}
