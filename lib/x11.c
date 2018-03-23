#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE   700
#include <unistd.h>
#include <limits.h>
#include <stdc.h>
#include <x11.h>
#include <utf.h>
#include <edit.h>
#include <win.h>
#include <locale.h>
#include <sys/time.h>
#include <sys/types.h>
#include <ctype.h>

#define Region _Region
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#undef Region

enum { FontCacheSize = 16 };

static void onredraw(int height, int width);
static void oninput(int mods, Rune key);
static void onmousedrag(int state, int x, int y);
static void onmousebtn(int btn, bool pressed, int x, int y);
static void onwheelup(WinRegion id, bool pressed, size_t row, size_t col);
static void onwheeldn(WinRegion id, bool pressed, size_t row, size_t col);
static void oncommand(char* cmd);
static void draw_glyphs(size_t x, size_t y, UGlyph* glyphs, size_t rlen, size_t ncols);
static WinRegion getregion(size_t x, size_t y);

static struct XSel* selfetch(Atom atom);
static void selclear(XEvent* evnt);
static void selnotify(XEvent* evnt);
static void selrequest(XEvent* evnt);
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

static size_t Ruler = 0;
static double ScrollOffset = 0.0;
static double ScrollVisible = 1.0;
static XFont CurrFont;
static WinRegion Focused = EDIT;
static Region Regions[NREGIONS] = {0};
static Rune LastKey;
static KeyBinding* Keys = NULL;
static void (*InputFunc)(Rune);

static void xftcolor(XftColor* xc, int id) {
    #define COLOR(c) ((c) | ((c) >> 8))
    uint32_t c = Palette[id];
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
        Palette[0]);

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
    onredraw(X.width, X.height);
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
        job_start((char*[]){ "xcpd", NULL }, text, len, NULL);
        while (job_poll(-1, 100));
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
    oninput(KeyBtnState, key);
}

static void handle_mouse(XEvent* e) {
    KeyBtnState = e->xbutton.state;
    int x = e->xbutton.x;
    int y = e->xbutton.y;

    if (e->type == MotionNotify) {
        onmousedrag(KeyBtnState, x, y);
    } else {
        if (e->type == ButtonRelease)
            KeyBtnState &= ~(1 << (e->xbutton.button + 7));
        else
            KeyBtnState |= (1 << (e->xbutton.button + 7));
        onmousebtn(e->xbutton.button, (e->type == ButtonPress), x, y);
    }
}

static void set_focus(bool focused) {
    if (X.xic)
        (focused ? XSetICFocus : XUnsetICFocus)(X.xic);
    onfocus(focused);
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
        case ClientMessage:
            if (e->xclient.data.l[0] == wmDeleteMessage)
                onshutdown();
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
        int h = (font->height - font->descent) + LineSpacing;
        xftcolor(&bgc, bg);
        size_t width = specs[nspecs-1].x - specs[0].x + w;
        if (eol) width = X.width - specs[0].x;
        x11_draw_rect(bg, specs[0].x, specs[0].y - h, width, font->height + LineSpacing);
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

static char* readprop(Display* disp, Window win, Atom prop) {
    Atom rtype;
    unsigned long format = 0, nitems = 0, nleft = 0, nread = 0;
    unsigned char* data = NULL;
    XGetWindowProperty(X.display, win, prop, 0, -1, False, AnyPropertyType, &rtype,
                       (int*)&format, &nitems, &nleft, &data);
    return (char*)data;
}

/******************************************************************************/

static void win_init(void (*errfn)(char*)) {
    for (int i = 0; i < SCROLL; i++)
        view_init(&(Regions[i].view), NULL, errfn);
    x11_init(0);
    CurrFont = x11_font_load(FontString);
    Regions[SCROLL].clrnor = Colors[ClrScrollNor];
    Regions[TAGS].clrnor = Colors[ClrTagsNor];
    Regions[TAGS].clrsel = Colors[ClrTagsSel];
    Regions[TAGS].clrcsr = Colors[ClrTagsCsr];
    Regions[EDIT].clrnor = Colors[ClrEditNor];
    Regions[EDIT].clrsel = Colors[ClrEditSel];
    Regions[EDIT].clrcsr = Colors[ClrEditCsr];
}

void win_window(char* name, bool isdialog, void (*errfn)(char*)) {
    win_init(errfn);
    if (isdialog)
        x11_dialog(name, WinWidth, WinHeight);
    else
        x11_window(name, WinWidth, WinHeight);
}

static void win_update(int xfd, void* data) {
    if (xfd < 0) return;
    if (x11_events_queued())
        x11_events_take();
    x11_flush();
}

void win_load(char* path) {
    View* view = win_view(EDIT);
    view_init(view, path, view->buffer.errfn);
    path = view->buffer.path;
}

void win_save(char* path) {
    View* view = win_view(EDIT);
    if (!path) path = view->buffer.path;
    if (!path) return;
    path = stringdup(path);
    free(view->buffer.path);
    view->buffer.path = path;
    buf_save(&(view->buffer));
}

void win_loop(void) {
    x11_show();
    while (x11_running()) {
        bool pending = job_poll(ConnectionNumber(X.display), Timeout);
        int nevents = x11_events_queued();
        if (pending || nevents) {
            x11_events_take();
            if (x11_running())
                x11_flip();
        }
        x11_flush();
    }
    x11_finish();
}

void win_settext(WinRegion id, char* text) {
    View* view = win_view(id);
    view->buffer.gapstart = view->buffer.bufstart;
    view->buffer.gapend   = view->buffer.bufend;
    view->selection = (Sel){0,0,0};
    view_putstr(view, text);
    view_selprev(view); // clear the selection
    buf_logclear(&(view->buffer));
}

void win_setruler(size_t ruler) {
    Ruler = ruler;
}

Rune win_getkey(void) {
    return LastKey;
}

void win_setkeys(KeyBinding* bindings, void (*inputfn)(Rune)) {
    Keys = bindings;
    InputFunc = inputfn;
}

bool win_btnpressed(int btn) {
    int btnmask = (1 << (btn + 7));
    return ((x11_keybtnstate() & btnmask) == btnmask);
}

WinRegion win_getregion(void) {
    return Focused;
}

bool win_setregion(WinRegion id) {
    bool changed = true;
    if (Focused != id && (id == TAGS || id == EDIT))
        changed = true, Focused = id;
    return changed;
}

View* win_view(WinRegion id) {
    if (id == FOCUSED) id = Focused;
    return &(Regions[id].view);
}

Buf* win_buf(WinRegion id) {
    if (id == FOCUSED) id = Focused;
    return &(Regions[id].view.buffer);
}

Sel* win_sel(WinRegion id) {
    if (id == FOCUSED) id = Focused;
    return &(Regions[id].view.selection);
}

void win_setscroll(double offset, double visible) {
    ScrollOffset  = offset;
    ScrollVisible = visible;
}

static void layout(int width, int height) {
    size_t fheight = x11_font_height(CurrFont);
    size_t fwidth  = x11_font_width(CurrFont);
    View* tagview  = win_view(TAGS);
    View* editview = win_view(EDIT);

    /* update the text views and region positions and sizes */
    for (int i = 0; i < SCROLL; i++) {
        Regions[i].x      = 2;
        Regions[i].y      = 2;
        Regions[i].csrx   = SIZE_MAX;
        Regions[i].csry   = SIZE_MAX;
        Regions[i].width  = (width - 4);
        Regions[i].height = fheight;
    }

    /* Place the tag region relative to status */
    size_t maxtagrows = ((height - Regions[TAGS].y - 5) / 4) / fheight;
    size_t tagcols    = Regions[TAGS].width / fwidth;
    size_t tagrows    = view_limitrows(tagview, maxtagrows, tagcols);
    Regions[TAGS].height = tagrows * fheight;
    view_resize(tagview, tagrows, tagcols);

    /* Place the scroll region relative to tags */
    Regions[SCROLL].x      = 0;
    Regions[SCROLL].y      = 5 + Regions[TAGS].y + Regions[TAGS].height;
    Regions[SCROLL].height = (height - Regions[EDIT].y - 5);
    Regions[SCROLL].width  = 5 + fwidth;

    /* Place the edit region relative to tags */
    Regions[EDIT].x      = 3 + Regions[SCROLL].width;
    Regions[EDIT].y      = 5 + Regions[TAGS].y + Regions[TAGS].height;
    Regions[EDIT].height = (height - Regions[EDIT].y - 5);
    Regions[EDIT].width  = width - Regions[SCROLL].width - 5;
    view_resize(editview, Regions[EDIT].height / fheight, Regions[EDIT].width / fwidth);
}

static void onredraw(int width, int height) {
    size_t fheight = x11_font_height(CurrFont);
    size_t fwidth  = x11_font_width(CurrFont);

    layout(width, height);
    onupdate(); // Let the user program update the status and other content
    int clrtagnor = (Regions[TAGS].clrnor.bg << 8 | Regions[TAGS].clrnor.fg);
    int clrtagsel = (Regions[TAGS].clrsel.bg << 8 | Regions[TAGS].clrsel.fg);
    view_update(win_view(TAGS), clrtagnor, clrtagsel, &(Regions[TAGS].csrx), &(Regions[TAGS].csry));
    int clreditnor = (Regions[EDIT].clrnor.bg << 8 | Regions[EDIT].clrnor.fg);
    int clreditsel = (Regions[EDIT].clrsel.bg << 8 | Regions[EDIT].clrsel.fg);
    view_update(win_view(EDIT), clreditnor, clreditsel, &(Regions[EDIT].csrx), &(Regions[EDIT].csry));
    onlayout(); // Let the user program update the scroll bar

    int clr_hbor = Colors[ClrBorders].bg;
    int clr_vbor = Colors[ClrBorders].bg;
    CPair clr_scroll = Colors[ClrScrollNor];

    for (int i = 0; i < SCROLL; i++) {
        View* view = win_view(i);
        x11_draw_rect(Regions[i].clrnor.bg, 0, Regions[i].y - 3, width, Regions[i].height + 8);
        x11_draw_rect(clr_hbor, 0, Regions[i].y - 3, width, 1);

        if (i == EDIT) {
            if (Ruler)
                x11_draw_rect( Colors[ClrEditRul].fg,
                               ((Ruler+2) * fwidth),
                               Regions[i].y-2,
                               1,
                               Regions[i].height+7 );
        }

        for (size_t line = 0, y = 0; y < view->nrows; y++) {
            Row* row = view_getrow(view, y);
            draw_glyphs(Regions[i].x, Regions[i].y + ((y+1) * fheight), row->cols, row->rlen, row->len);
        }
    }

    /* draw the scroll region */
    size_t thumbreg = (Regions[SCROLL].height - Regions[SCROLL].y + 9);
    size_t thumboff = (size_t)((thumbreg * ScrollOffset) + (Regions[SCROLL].y - 2));
    size_t thumbsz  = (size_t)(thumbreg * ScrollVisible);
    if (thumbsz < 5) thumbsz = 5;
    x11_draw_rect(clr_vbor, Regions[SCROLL].width, Regions[SCROLL].y - 2, 1, Regions[SCROLL].height);
    x11_draw_rect(clr_scroll.bg, 0, Regions[SCROLL].y - 2, Regions[SCROLL].width, thumbreg);
    x11_draw_rect(clr_scroll.fg, 0, thumboff, Regions[SCROLL].width, thumbsz);

    /* place the cursor on screen */
    if (Regions[Focused].csrx != SIZE_MAX && Regions[Focused].csry != SIZE_MAX) {
        x11_draw_rect(
            Regions[Focused].clrcsr.fg,
            Regions[Focused].x + (Regions[Focused].csrx * fwidth),
            Regions[Focused].y + (Regions[Focused].csry * fheight),
            1, fheight);
    }
}

static void oninput(int mods, Rune key) {
    LastKey = key;
    /* mask of modifiers we don't care about */
    mods = mods & (ModCtrl|ModShift|ModAlt);
    /* handle the proper line endings */
    if (key == '\r') key = '\n';
    /* search for a key binding entry */
    uint32_t mkey = tolower(key);
    for (KeyBinding* bind = Keys; bind && bind->key; bind++) {
        bool match   = (mkey == bind->key);
        bool exact   = (bind->mods == mods);
        bool any     = (bind->mods == ModAny);
        bool oneplus = ((bind->mods == ModOneOrMore) && (mods & ModOneOrMore));
        if (match && (exact || oneplus || any)) {
            bind->action();
            return;
        }
    }

    /* translate to crlf if needed */
    if (key == '\n' && win_view(FOCUSED)->buffer.crlf)
        key = RUNE_CRLF;

    /* fallback to just inserting the rune if it doesn't fall in the private use area.
     * the private use area is used to encode special keys */
    if (key < 0xE000 || key > 0xF8FF) {
        if (InputFunc)
            InputFunc(key);
        else
            view_insert(win_view(FOCUSED), true, key);
    }
}

static void scroll_actions(int btn, bool pressed, int x, int y) {
    size_t row = (y-Regions[SCROLL].y) / x11_font_height(CurrFont);
    switch (btn) {
        case MouseLeft:
            if (pressed)
                view_scroll(win_view(EDIT), -row);
            break;
        case MouseMiddle:
            if (pressed)
                onscroll((double)(y - Regions[SCROLL].y) /
                         (double)(Regions[SCROLL].height - Regions[SCROLL].y));
            break;
        case MouseRight:
            if (pressed)
                view_scroll(win_view(EDIT), +row);
            break;
        case MouseWheelUp:
            view_scroll(win_view(EDIT), -ScrollBy);
            break;
        case MouseWheelDn:
            view_scroll(win_view(EDIT), +ScrollBy);
            break;
    }
}

static void onmousedrag(int state, int x, int y) {
    if (x < Regions[Focused].x) x = Regions[Focused].x;
    if (y < Regions[Focused].y) y = Regions[Focused].y;
    size_t row = (y-Regions[Focused].y) / x11_font_height(CurrFont);
    size_t col = (x-Regions[Focused].x) / x11_font_width(CurrFont);
    if (win_btnpressed(MouseLeft))
        view_selext(win_view(Focused), row, col);
}

static void onmousebtn(int btn, bool pressed, int x, int y) {
    WinRegion id = getregion(x, y);
    if (id == FOCUSED && x < Regions[Focused].x)
        x = Regions[Focused].x, id = getregion(x, y);
    size_t row = (y-Regions[id].y) / x11_font_height(CurrFont);
    size_t col = (x-Regions[id].x) / x11_font_width(CurrFont);

    if (id == SCROLL) {
        scroll_actions(btn, pressed, x, y);
    } else {
        switch(btn) {
            case MouseLeft:    onmouseleft(id, pressed, row, col);   break;
            case MouseMiddle:  onmousemiddle(id, pressed, row, col); break;
            case MouseRight:   onmouseright(id, pressed, row, col);  break;
            case MouseWheelUp: onwheelup(id, pressed, row, col);     break;
            case MouseWheelDn: onwheeldn(id, pressed, row, col);     break;
        }
    }
}

static void onwheelup(WinRegion id, bool pressed, size_t row, size_t col) {
    if (!pressed) return;
    view_scroll(win_view(id), -ScrollBy);
}

static void onwheeldn(WinRegion id, bool pressed, size_t row, size_t col) {
    if (!pressed) return;
    view_scroll(win_view(id), +ScrollBy);
}

static void oncommand(char* cmd) {
    size_t line = strtoul(cmd, NULL, 0);
    if (line) {
        View* view = win_view(EDIT);
        win_setregion(EDIT);
        view_setln(view, line);
        view_eol(view, false);
        view_selctx(view);
    }
}

static void draw_glyphs(size_t x, size_t y, UGlyph* glyphs, size_t rlen, size_t ncols) {
    XGlyphSpec specs[rlen];
    size_t i = 0;
    bool eol = false;
    while (rlen && i < ncols) {
        int numspecs = 0;
        uint32_t attr = glyphs[i].attr;
        while (i < ncols && glyphs[i].attr == attr) {
            if (glyphs[i].rune == '\n')
                glyphs[i].rune = ' ', eol = true;
            x11_font_getglyph(CurrFont, &(specs[numspecs]), glyphs[i].rune);
            specs[numspecs].x = x;
            specs[numspecs].y = y - x11_font_descent(CurrFont);
            x += x11_font_width(CurrFont);
            numspecs++;
            i++;
            /* skip over null chars which mark multi column runes */
            for (; i < ncols && !glyphs[i].rune; i++)
                x += x11_font_width(CurrFont);
        }
        /* Draw the glyphs with the proper colors */
        uint8_t bg = attr >> 8;
        uint8_t fg = attr & 0xFF;
        x11_draw_glyphs(fg, bg, specs, numspecs, eol);
        eol = false, rlen -= numspecs;
    }
}

static WinRegion getregion(size_t x, size_t y) {
    for (int i = 0; i < NREGIONS; i++) {
        size_t startx = Regions[i].x, endx = startx + Regions[i].width;
        size_t starty = Regions[i].y, endy = starty + Regions[i].height;
        if ((startx <= x && x <= endx) && (starty <= y && y <= endy))
            return (WinRegion)i;
    }
    return NREGIONS;
}
