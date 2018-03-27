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

/******************************************************************************/

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

static void (*EventHandlers[LASTEvent])(XEvent *) = {
	[FocusIn] = xfocus,
	[FocusOut] = xfocus,
	[KeyPress] = xkeypress,
	[ButtonPress] = xbtnpress,
	[ButtonRelease] = xbtnrelease,
	[MotionNotify] = xbtnmotion,
 	[SelectionClear] = xselclear,
	[SelectionNotify] = xselnotify,
	[SelectionRequest] = xselrequest,
	[PropertyNotify] = xpropnotify,
	[ClientMessage] = xclientmsg,
	[ConfigureNotify] = xresize,
	[Expose] = xexpose,
};

/******************************************************************************/

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
static uint32_t special_keys(uint32_t key);
static uint32_t getkey(XEvent* e);
static void xftcolor(XftColor* xc, int id);

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

static double ScrollOffset = 0.0;
static double ScrollVisible = 1.0;
static XFont CurrFont;
static WinRegion Focused = EDIT;
static Region Regions[NREGIONS] = {0};
static Rune LastKey;
static KeyBinding* Keys = NULL;
static void (*InputFunc)(Rune);

void x11_deinit(void) {
    Running = false;
}

void x11_init(XConfig* cfg) {
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
    X.pixmap = XCreatePixmap(X.display, X.self, width, height, X.depth);
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

/******************************************************************************/

void win_init(void (*errfn)(char*)) {
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

void win_save(char* path) {
    View* view = win_view(EDIT);
    if (!path) path = view->buffer.path;
    if (!path) return;
    path = stringdup(path);
    free(view->buffer.path);
    view->buffer.path = path;
    buf_save(&(view->buffer));
}

static void xupdate(Job* job) {
    for (XEvent e; XPending(X.display);) {
        XNextEvent(X.display, &e);
        if (!XFilterEvent(&e, None) && EventHandlers[e.type])
            (EventHandlers[e.type])(&e);
    }
    onredraw(X.width, X.height);
    XCopyArea(X.display, X.pixmap, X.self, X.gc, 0, 0, X.width, X.height, 0, 0);
	XFlush(X.display);
}

void win_loop(void) {
    XMapWindow(X.display, X.self);
    XFlush(X.display);
    job_spawn(ConnectionNumber(X.display), xupdate, 0, 0);
    while (1) job_poll(Timeout);
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
    return &(Regions[id == FOCUSED ? Focused : id].view);
}

Buf* win_buf(WinRegion id) {
    return &(Regions[id == FOCUSED ? Focused : id].view.buffer);
}

Sel* win_sel(WinRegion id) {
    return &(Regions[id == FOCUSED ? Focused : id].view.selection);
}

void win_setscroll(double offset, double visible) {
    ScrollOffset  = offset;
    ScrollVisible = visible;
}

/******************************************************************************/

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
        for (size_t line = 0, y = 0; y < view->nrows; y++) {
            Row* row = view_getrow(view, y);
            draw_glyphs(Regions[i].x, Regions[i].y + ((y+1) * fheight), row->cols, row->rlen, row->len);
        }

        /* place the cursor on screen */
        if (Regions[i].csrx != SIZE_MAX && Regions[i].csry != SIZE_MAX) {
            x11_draw_rect(
                Regions[i].clrcsr.fg,
                Regions[i].x + (Regions[i].csrx * fwidth),
                Regions[i].y + (Regions[i].csry * fheight),
                1, fheight);
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
        view_setcursor(win_view(Focused), row, col, true);
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
        [0x08] = '\b', [0x09] = '\t', [0x0d] = '\r', [0x0a] = '\n',
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

static void xftcolor(XftColor* xc, int id) {
    #define COLOR(c) ((c) | ((c) >> 8))
    uint32_t c = Palette[id];
    xc->color.alpha = COLOR((c & 0xFF000000) >> 16);
    xc->color.red   = COLOR((c & 0x00FF0000) >> 8);
    xc->color.green = COLOR((c & 0x0000FF00));
    xc->color.blue  = COLOR((c & 0x000000FF) << 8);
    XftColorAllocValue(X.display, X.visual, X.colormap, &(xc->color), xc);
}

static struct XSel* selfetch(Atom atom) {
    for (int i = 0; i < (sizeof(Selections) / sizeof(Selections[0])); i++)
        if (atom == Selections[i].atom)
            return &Selections[i];
    return NULL;
}

/******************************************************************************/

static void xfocus(XEvent* e) {
    if (X.xic)
        (e->type == FocusIn ? XSetICFocus : XUnsetICFocus)(X.xic);
    onfocus(e->type == FocusIn);
}

static void xkeypress(XEvent* e) {
    uint32_t key = getkey(e);
    KeyBtnState = e->xkey.state;
    if (key == RUNE_ERR) return;
    oninput(KeyBtnState, key);
}

static void xbtnpress(XEvent* e) {
    KeyBtnState = e->xbutton.state;
    KeyBtnState |= (1 << (e->xbutton.button + 7));
    onmousebtn(e->xbutton.button, true, e->xbutton.x,  e->xbutton.y);
}

static void xbtnrelease(XEvent* e) {
    KeyBtnState = e->xbutton.state;
    KeyBtnState &= ~(1 << (e->xbutton.button + 7));
    onmousebtn(e->xbutton.button, false, e->xbutton.x,  e->xbutton.y);
}

static void xbtnmotion(XEvent* e) {
    KeyBtnState = e->xbutton.state;
    onmousedrag(KeyBtnState, e->xbutton.x, e->xbutton.y);
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
    unsigned long format = 0, nitems = 0, nleft = 0, nread = 0;
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

static void xpropnotify(XEvent* e) {
}

static void xclientmsg(XEvent* e) {
    if (e->xclient.data.l[0] == XInternAtom(X.display, "WM_DELETE_WINDOW", False))
        onshutdown();
}

static void xresize(XEvent* e) {
    if (e->xconfigure.width != X.width || e->xconfigure.height != X.height) {
        X.width  = e->xconfigure.width;
        X.height = e->xconfigure.height;
        X.pixmap = XCreatePixmap(X.display, X.self, X.width, X.height, X.depth);
        X.xft    = XftDrawCreate(X.display, X.pixmap, X.visual, X.colormap);
    }
}

static void xexpose(XEvent* e) {
}
