#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE   700
#include <unistd.h>
#include <limits.h>
#include <stdc.h>
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

struct XFont {
    int height;
    XftFont* match;
};

#define PRESSED(btn) \
    ((KeyBtnState & (1 << (btn + 7))) == (1 << (btn + 7)))

/******************************************************************************/
static void die(const char* msg);

static XFont x11_font_load(char* name);
static size_t x11_font_height(XFont fnt);
static size_t x11_font_width(XFont fnt);
static size_t x11_font_descent(XFont fnt);
static void getglyph(XFont font, XGlyphSpec* spec, uint32_t rune);
static size_t getglyphs(XGlyphSpec* specs, const XGlyph* glyphs, int len, XFont font, int x, int y);
static void x11_draw_rect(int color, int x, int y, int width, int height);
static void x11_draw_glyphs(int fg, int bg, XGlyphSpec* specs, size_t nspecs, bool eol);

static uint32_t special_keys(uint32_t key);
static uint32_t getkey(XEvent* e);
static void mouse_click(int btn, bool pressed, int x, int y);
static void mouse_left(WinRegion id, bool pressed, size_t row, size_t col);
static void mouse_middle(WinRegion id, bool pressed, size_t row, size_t col);
static void mouse_right(WinRegion id, bool pressed, size_t row, size_t col);

static void draw_view(int i, size_t nrows, drawcsr* csr, int bg, int fg, int sel);
static void draw_hrule(drawcsr* csr);
static void draw_scroll(drawcsr* csr);
static void draw_glyphs(size_t x, size_t y, UGlyph* glyphs, size_t rlen, size_t ncols);

static void xupdate(Job* job);
static void xfocus(XEvent* e);
static void xkeypress(XEvent* e);
static void xbtnpress(XEvent* e);
static void xbtnrelease(XEvent* e);
static void xbtnmotion(XEvent* e);
static void xselclear(XEvent* e);
static void xselnotify(XEvent* e);
static void xselrequest(XEvent* e);
static void xpropnotify(XEvent* e);
static void xclientmsg(XEvent* e);
static void xresize(XEvent* e);
static void xexpose(XEvent* e);

static void (*EventHandlers[LASTEvent])(XEvent*) = {
    [FocusIn] = xfocus,
    [FocusOut] = xfocus,
    [KeyPress] = xkeypress,
    [ButtonPress] = xbtnpress,
    [ButtonRelease] = xbtnrelease,
    [MotionNotify] = xbtnmotion,
    [SelectionClear] = xselclear,
    [SelectionNotify] = xselnotify,
    [SelectionRequest] = xselrequest,
    [ClientMessage] = xclientmsg,
    [ConfigureNotify] = xresize,
};

/******************************************************************************/

enum { FontCacheSize = 16 };

static WinRegion getregion(size_t x, size_t y);
static struct XSel* selfetch(Atom atom);
static void xftcolor(XftColor* xc, int id);

static struct {
    Time now;
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

static XFont CurrFont;
static WinRegion Focused = EDIT;
static View Regions[NREGIONS];
static Rune LastKey;
static KeyBinding* Keys = NULL;
static int Divider;

int SearchDir = DOWN;
char* SearchTerm = NULL;

/******************************************************************************/

void win_init(KeyBinding* bindings) {
    view_init(&Regions[TAGS], NULL);
    view_init(&Regions[EDIT], NULL);
    x11_init();
    Keys = bindings;
    CurrFont = x11_font_load(FontString);
    View* view = win_view(TAGS);
    view_putstr(view, TagString);
    view_selprev(view); // clear the selection
    buf_logclear(&(view->buffer));
}

void win_save(char* path) {
    View* view = win_view(EDIT);
    if (!path) path = view->buffer.path;
    if (!path) return;
    path = strdup(path);
    free(view->buffer.path);
    view->buffer.path = path;
    buf_save(&(view->buffer));
}

void win_loop(void) {
    XMapWindow(X.display, X.self);
    XFlush(X.display);
    job_spawn(ConnectionNumber(X.display), xupdate, 0, 0);
    while (1) job_poll(Timeout);
}

WinRegion win_getregion(void) {
    return Focused;
}

void win_setregion(WinRegion id) {
    Focused = id;
}

View* win_view(WinRegion id) {
    return &(Regions[id == FOCUSED ? Focused : id]);
}

Buf* win_buf(WinRegion id) {
    return &(Regions[id == FOCUSED ? Focused : id].buffer);
}

void win_quit(void) {
    static uint64_t before = 0;
    if (!win_buf(EDIT)->modified || (X.now - before) <= (uint64_t)ClickTime)
        exit(0);
    before = X.now;
}

/******************************************************************************/

void x11_init(void) {
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

bool x11_keymodsset(int mask) {
    return ((KeyBtnState & mask) == mask);
}

void x11_window(char* name) {
    /* create the main window */
    X.width = WinWidth, X.height = WinHeight;
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
    XSelectInput(X.display, X.self, 0
        | FocusChangeMask
        | KeyPressMask
        | ButtonPressMask
        | ButtonReleaseMask
        | ButtonMotionMask
        | StructureNotifyMask
        | PropertyChangeMask
        | ExposureMask
    );

    /* set input methods */
    if ((X.xim = XOpenIM(X.display, 0, 0, 0)))
        X.xic = XCreateIC(X.xim, XNInputStyle, XIMPreeditNothing|XIMStatusNothing, XNClientWindow, X.self, XNFocusWindow, X.self, NULL);

    /* initialize pixmap and drawing context */
    X.pixmap = XCreatePixmap(X.display, X.self, X.width, X.height, X.depth);
    X.xft    = XftDrawCreate(X.display, X.pixmap, X.visual, X.colormap);

    /* initialize the graphics context */
    XGCValues gcv;
    gcv.foreground = WhitePixel(X.display, X.screen);
    gcv.graphics_exposures = False;
    X.gc = XCreateGC(X.display, X.self, GCForeground|GCGraphicsExposures, &gcv);
}

/******************************************************************************/

XFont x11_font_load(char* name) {
    struct XFont* font = calloc(1, sizeof(struct XFont));
    /* init the library and the base font pattern */
    if (!FcInit())
        die("Could not init fontconfig.\n");
    FcPattern* pattern = FcNameParse((FcChar8 *)name);
    if (!pattern)
        die("could not parse font name\n");
    /* load the base font */
    FcResult result;
    FcPattern* match = XftFontMatch(X.display, X.screen, pattern, &result);
    if (!match || !(font->match = XftFontOpenPattern(X.display, match)))
        die("could not load base font");
    font->height = font->match->ascent + font->match->descent;
    return font;
}

size_t x11_font_height(XFont fnt) {
    return fnt->match->height;
}

size_t x11_font_width(XFont fnt) {
    XGlyphInfo extents;
    XftTextExtentsUtf8(X.display, fnt->match, (const FcChar8*)"0", 1, &extents);
    return extents.xOff;
}

size_t x11_font_descent(XFont fnt) {
    return fnt->match->descent;
}

void getglyph(XFont fnt, XGlyphSpec* spec, uint32_t rune) {
    spec->glyph = XftCharIndex(X.display, fnt->match, rune);
    spec->font  = fnt->match;
}

size_t getglyphs(XGlyphSpec* specs, const XGlyph* glyphs, int len, XFont fnt, int x, int y) {
    int winx = x, winy = y;
    size_t numspecs = 0;
    for (int i = 0, xp = winx, yp = winy; i < len;) {
        getglyph(fnt, &(specs[numspecs]), glyphs[i].rune);
        specs[numspecs].x = xp;
        specs[numspecs].y = yp;
        xp += x11_font_width(fnt);
        numspecs++;
        i++;
        /* skip over null chars which mark multi column runes */
        for (; i < len && !glyphs[i].rune; i++)
            xp += x11_font_width(fnt);
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

void x11_draw_rect(int color, int x, int y, int width, int height) {
    XftColor clr;
    xftcolor(&clr, color);
    XftDrawRect(X.xft, &clr, x, y, width, height);
    XftColorFree(X.display, X.visual, X.colormap, &clr);
}

/******************************************************************************/

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

static struct XSel* selfetch(Atom atom) {
    for (int i = 0; i < (sizeof(Selections) / sizeof(Selections[0])); i++)
        if (atom == Selections[i].atom)
            return &Selections[i];
    return NULL;
}

/******************************************************************************/

static WinRegion getregion(size_t x, size_t y) {
    return (y <= Divider ? TAGS : EDIT);
}

static void xftcolor(XftColor* xc, int id) {
    #define COLOR(c) ((c) | ((c) >> 8))
    uint32_t c = Palette[id];
    xc->color.alpha = 0xFFFF;
    xc->color.red   = COLOR((c & 0x00FF0000) >> 8);
    xc->color.green = COLOR((c & 0x0000FF00));
    xc->color.blue  = COLOR((c & 0x000000FF) << 8);
    XftColorAllocValue(X.display, X.visual, X.colormap, &(xc->color), xc);
}

static void get_position(WinRegion id, int x, int y, size_t* row, size_t* col) {
    int starty = (id == EDIT ? Divider+3 : 0);
    int startx = (id == EDIT ? ScrollWidth+3 : 0);
    *row = (y - starty) / x11_font_height(CurrFont);
    *col = (x - startx) / x11_font_width(CurrFont);
}

/******************************************************************************/

static void xupdate(Job* job) {
    size_t fheight = x11_font_height(CurrFont);
    size_t fwidth  = x11_font_width(CurrFont);
    /* process events from the queue */
    for (XEvent e; XPending(X.display);) {
        XNextEvent(X.display, &e);
        if (!XFilterEvent(&e, None) && EventHandlers[e.type])
            (EventHandlers[e.type])(&e);
    }
    /* determine the size of the regions */
    drawcsr csr = { .w = X.width, .h = X.height };
    size_t maxtagrows = ((X.height - 2) / 4) / fheight;
    size_t tagcols    = csr.w / fwidth;
    size_t tagrows    = view_limitrows(win_view(TAGS), maxtagrows, tagcols);
    size_t editrows   = ((X.height - 7) / fheight) - tagrows;
    /* draw the regions to the window */
    draw_view(TAGS, tagrows, &csr, TagsBg, TagsFg, TagsSel);
    draw_hrule(&csr);
    draw_view(EDIT, editrows, &csr, EditBg, EditFg, EditSel);
    draw_scroll(&csr);
    /* flush to the server */
    XCopyArea(X.display, X.pixmap, X.self, X.gc, 0, 0, X.width, X.height, 0, 0);
    XFlush(X.display);
}

static void xfocus(XEvent* e) {
    if (X.xic)
        (e->type == FocusIn ? XSetICFocus : XUnsetICFocus)(X.xic);
}

static void xkeypress(XEvent* e) {
    X.now = e->xkey.time;
    win_setregion(getregion(e->xkey.x, e->xkey.y));
    uint32_t key = getkey(e);
    if (key == RUNE_ERR) return;
    KeyBtnState = e->xkey.state, LastKey = key;
    int mods = KeyBtnState & (ModCtrl|ModShift|ModAlt);
    uint32_t mkey = tolower(key);
    for (KeyBinding* bind = Keys; bind && bind->key; bind++) {
        bool match   = (mkey == bind->key);
        bool exact   = (bind->mods == mods);
        bool any     = (bind->mods == ModAny);
        bool oneplus = ((bind->mods == ModOneOrMore) && (mods & ModOneOrMore));
        if (match && (exact || oneplus || any)) {
            bind->action(NULL);
            return;
        }
    }
    /* fallback to just inserting the rune if it doesn't fall in the private use area.
     * the private use area is used to encode special keys */
    if (key < 0xE000 || key > 0xF8FF)
        view_insert(win_view(FOCUSED), true, key);
}

static void xbtnpress(XEvent* e) {
    X.now = e->xbutton.time;
    KeyBtnState = (e->xbutton.state | (1 << (e->xbutton.button + 7)));
    mouse_click(e->xbutton.button, true, e->xbutton.x,  e->xbutton.y);
}

static void xbtnrelease(XEvent* e) {
    X.now = e->xbutton.time;
    KeyBtnState = (e->xbutton.state & ~(1 << (e->xbutton.button + 7)));
    mouse_click(e->xbutton.button, false, e->xbutton.x,  e->xbutton.y);
}

static void xbtnmotion(XEvent* e) {
    X.now = e->xbutton.time;
    size_t row, col;
    KeyBtnState = e->xbutton.state;
    int x = e->xbutton.x, y = e->xbutton.y;
    get_position(Focused, x, y, &row, &col);
    if (PRESSED(MouseLeft))
        view_setcursor(win_view(Focused), row, col, true);
}

static void xselclear(XEvent* e) {
    struct XSel* sel = selfetch(e->xselectionrequest.selection);
    if (!sel) return;
    free(sel->text);
    sel->text = NULL;
}

static void xselnotify(XEvent* e) {
    /* bail if the selection cannot be converted */
    if (e->xselection.property == None)
        return;
    struct XSel* sel = selfetch( e->xselection.selection );
    Atom rtype;
    unsigned long format = 0, nitems = 0, nleft = 0;
    unsigned char* propdata = NULL;
    XGetWindowProperty(X.display, X.self, sel->atom, 0, -1, False, AnyPropertyType, &rtype,
                       (int*)&format, &nitems, &nleft, &propdata);
    if (e->xselection.target == SelTarget) {
        void(*cbfn)(char*) = sel->callback;
        sel->callback = NULL;
        cbfn((char*)propdata);
    }
    /* cleanup */
    if (propdata) XFree(propdata);
}

static void xselrequest(XEvent* e) {
    XEvent s;
    struct XSel* sel = selfetch( e->xselectionrequest.selection );
    s.xselection.type      = SelectionNotify;
    s.xselection.property  = e->xselectionrequest.property;
    s.xselection.requestor = e->xselectionrequest.requestor;
    s.xselection.selection = e->xselectionrequest.selection;
    s.xselection.target    = e->xselectionrequest.target;
    s.xselection.time      = e->xselectionrequest.time;
    Atom target    = e->xselectionrequest.target;
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
    XSendEvent(X.display, s.xselection.requestor, True,
0, &s);
}

static void xclientmsg(XEvent* e) {
    if (e->xclient.data.l[0] == XInternAtom(X.display, "WM_DELETE_WINDOW", False))
        win_quit();
}

static void xresize(XEvent* e) {
    if (e->xconfigure.width != X.width || e->xconfigure.height != X.height) {
        X.width  = e->xconfigure.width;
        X.height = e->xconfigure.height;
        X.pixmap = XCreatePixmap(X.display, X.self, X.width, X.height, X.depth);
        X.xft    = XftDrawCreate(X.display, X.pixmap, X.visual, X.colormap);
    }
}

/******************************************************************************/

static uint32_t special_keys(uint32_t key) {
    static uint32_t keymap[256] = {
        /* Function keys */
        [0xBE] = KEY_F1, [0xBF] = KEY_F2,  [0xC0] = KEY_F3,  [0xC1] = KEY_F4,
        [0xC2] = KEY_F5, [0xC3] = KEY_F6,  [0xC4] = KEY_F7,  [0xC5] = KEY_F8,
        [0xC6] = KEY_F9, [0xC7] = KEY_F10, [0xC8] = KEY_F11, [0xC9] = KEY_F12,
        /* Navigation keys */
        [0x50] = KEY_HOME,  [0x51] = KEY_LEFT, [0x52] = KEY_UP,
        [0x53] = KEY_RIGHT, [0x54] = KEY_DOWN, [0x55] = KEY_PGUP,
        [0x56] = KEY_PGDN,  [0x57] = KEY_END,
        /* Control keys */
        [0x08] = '\b', [0x09] = '\t', [0x0d] = '\n', [0x0a] = '\n',
        /* Miscellaneous */
        [0x63] = KEY_INSERT, [0x1B] = KEY_ESCAPE, [0xFF] = KEY_DELETE,
    };
    /* lookup the key by keysym */
    key = ((key & 0xFF00) == 0xFF00 ? keymap[key & 0xFF] : key);
    return (!key ? RUNE_ERR : key);
}

static uint32_t getkey(XEvent* e) {
    char buf[8];
    KeySym key;
    Status status;
    /* Read the key string */
    if (X.xic)
        Xutf8LookupString(X.xic, &(e->xkey), buf, sizeof(buf), &key, &status);
    else
        XLookupString(&(e->xkey), buf, sizeof(buf), &key, 0);
    /* if it's ascii, just return it */
    if (key >= 0x20 && key <= 0x7F)
        return (uint32_t)key;
    /* translate special key codes into unicode codepoints */
    return special_keys(key);
}

static void mouse_click(int btn, bool pressed, int x, int y) {
    size_t row, col;
    WinRegion id = getregion(x, y);
    win_setregion(id);
    get_position(id, x, y, &row, &col);
    switch(btn) {
        case MouseLeft:    mouse_left(id, pressed, row, col);    break;
        case MouseMiddle:  mouse_middle(id, pressed, row, col);  break;
        case MouseRight:   mouse_right(id, pressed, row, col);   break;
        case MouseWheelUp: view_scroll(win_view(id), -ScrollBy); break;
        case MouseWheelDn: view_scroll(win_view(id), +ScrollBy); break;
    }
}

static void mouse_left(WinRegion id, bool pressed, size_t row, size_t col) {
    static int count = 0;
    static Time before = 0;
    if (!pressed) return;
    count = ((X.now - before) <= (uint64_t)ClickTime ? count+1 : 1);
    before = X.now;
    if (PRESSED(MouseRight)) {
        puts("fetch tag");
    }  else if (PRESSED(MouseMiddle)) {
        puts("exec with arg");
    } else {
        if (count == 1)
            view_setcursor(win_view(id), row, col, x11_keymodsset(ModShift));
        else if (count == 2)
            view_select(win_view(id), row, col);
        else if (count == 3)
            view_selword(win_view(id), row, col);
    }
}

static void mouse_middle(WinRegion id, bool pressed, size_t row, size_t col) {
    if (pressed) return;
    if (PRESSED(MouseLeft)) {
        cut(NULL);
    } else {
        char* str = view_fetch(win_view(id), row, col, riscmd);
        if (str) exec(str);
        free(str);
    }
}

static void mouse_right(WinRegion id, bool pressed, size_t row, size_t col) {
    if (pressed) return;
    if (PRESSED(MouseLeft)) {
        paste(NULL);
    } else {
        SearchDir *= (x11_keymodsset(ModShift) ? -1 : +1);
        free(SearchTerm);
        SearchTerm = view_fetch(win_view(id), row, col, risfile);
        view_findstr(win_view(id), SearchDir, SearchTerm);
    }
}

/******************************************************************************/

static void draw_view(int i, size_t nrows, drawcsr* csr, int bg, int fg, int sel) {
    size_t fwidth = x11_font_width(CurrFont);
    size_t fheight = x11_font_height(CurrFont);
    size_t csrx = SIZE_MAX, csry = SIZE_MAX;
    /* draw the view to the window */
    View* view = win_view(i);
    view_resize(view, nrows, (csr->w - csr->x) / fwidth);
    view_update(view, (bg << 8 | fg), (sel << 8 | fg), &csrx, &csry);
    x11_draw_rect(bg, csr->x, csr->y, csr->w, (nrows * fheight) + 9);
    for (size_t y = 0; y < view->nrows; y++) {
        Row* row = view_getrow(view, y);
        draw_glyphs(csr->x + 2, csr->y + 2 + ((y+1) * fheight), row->cols, row->rlen, row->len);
    }
    /* place the cursor on screen */
    if (!view_selsize(view) && csrx != SIZE_MAX && csry != SIZE_MAX) {
        x11_draw_rect(
            (i == TAGS ? TagsCsr : EditCsr),
            csr->x + 2 + (csrx * fwidth),
            csr->y + 2 + (csry * fheight),
            1, fheight);
    }
    csr->y += (nrows * fheight) + 3;
}

static void draw_hrule(drawcsr* csr) {
    x11_draw_rect(HorBdr, csr->x, csr->y + 1, csr->w, 1);
    Divider = csr->y;
    csr->y += 2;
    csr->x += ScrollWidth + 1;
}

static void draw_scroll(drawcsr* csr) {
    View* view = win_view(EDIT);
    size_t bend = buf_end(win_buf(EDIT));
    if (bend == 0) bend = 1;
    if (!view->rows) return;
    size_t vbeg = view->rows[0]->off;
    size_t vend = view->rows[view->nrows-1]->off + view->rows[view->nrows-1]->rlen;
    double scroll_vis = (double)(vend - vbeg) / (double)bend;
    double scroll_off = ((double)vbeg / (double)bend);
    size_t thumbreg = (csr->y - Divider) + 4;
    size_t thumboff = (size_t)((thumbreg * scroll_off) + Divider);
    size_t thumbsz  = (size_t)(thumbreg * scroll_vis);
    if (thumbsz < 5) thumbsz = 5;
    x11_draw_rect(VerBdr,   ScrollWidth, Divider + 2,  1,           thumbreg);
    x11_draw_rect(ScrollBg, 0,           Divider + 2,  ScrollWidth, thumbreg);
    x11_draw_rect(ScrollFg, 0,           thumboff + 2, ScrollWidth, thumbsz);
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
            getglyph(CurrFont, &(specs[numspecs]), glyphs[i].rune);
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

static void die(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}
