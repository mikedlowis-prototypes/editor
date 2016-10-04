#define _GNU_SOURCE
#include <time.h>
#include <signal.h>
#include <X11/Xlib.h>

#include "edit.h"

struct {
    Display* display;
    Visual* visual;
    Colormap colormap;
    unsigned depth;
    int screen;
    GC gc;
    XftFont* font;
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
Buf Buffer;
bool InsertMode = false;
unsigned StartRow = 0;
unsigned CursorPos = 0;
unsigned EndPos = 0;

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
    if(!(X.font = XftFontOpenName(X.display, X.screen, FONTNAME)))
        die("cannot open default font");

    /* set input methods */
    if((X.xim = XOpenIM(X.display, 0, 0, 0)))
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

static void deinit(void) {
    if (X.pixmap != None) {
        XftDrawDestroy(X.xft);
        XFreePixmap(X.display, X.pixmap);
    }
    XCloseDisplay(X.display);
}

static void handle_key(XEvent* e) {
    int len;
    char buf[8];
    KeySym key;
    Status status;
    /* Read the key string */
    if (X.xic)
        len = Xutf8LookupString(X.xic, &e->xkey, buf, sizeof(buf), &key, &status);
    else
        len = XLookupString(&e->xkey, buf, sizeof(buf), &key, 0);
    /* Handle the key */
    switch (key) {
        case XK_F1:
            InsertMode = !InsertMode;
            break;
        case XK_F6:
            ColorBase = !ColorBase;
            break;

        case XK_Left:
            CursorPos = buf_byrune(&Buffer, CursorPos, -1);
            break;

        case XK_Right:
            CursorPos = buf_byrune(&Buffer, CursorPos, 1);
            break;

        case XK_Down:
            CursorPos = buf_byline(&Buffer, CursorPos, 1);
            break;

        case XK_Up:
            CursorPos = buf_byline(&Buffer, CursorPos, -1);
            break;

        case XK_Escape:
            InsertMode = false;
            break;

        case XK_BackSpace:
            if (InsertMode)
                buf_del(&Buffer, --CursorPos);
            break;

        case XK_Delete:
            if (InsertMode)
                buf_del(&Buffer, CursorPos);
            break;

        default:
            if (len > 0) {
                Rune r;
                size_t len = 0;
                if (buf[0] == '\r')
                    buf[0] = '\n';
                for(int i = 0; i < 8 && !utf8decode(&r, &len, buf[i]); i++);
                if (InsertMode)
                    buf_ins(&Buffer, CursorPos++, r);
            }
            break;
    }
}

static void handle_mousebtn(XEvent* e) {
    switch (e->xbutton.button) {
        case Button1: /* Left Button */
            break;
        case Button2: /* Middle Button */
            break;
        case Button3: /* Right Button */
            break;
        case Button4: /* Wheel Up */
            CursorPos = buf_byline(&Buffer, CursorPos, -ScrollLines);
            break;
        case Button5: /* Wheel Down */
            CursorPos = buf_byline(&Buffer, CursorPos, ScrollLines);
            break;
    }
}

static void handle_event(XEvent* e) {
    switch (e->type) {
        case FocusIn:     if (X.xic) XSetICFocus(X.xic);   break;
        case FocusOut:    if (X.xic) XUnsetICFocus(X.xic); break;
        case ButtonPress: handle_mousebtn(e);              break;
        case KeyPress:    handle_key(e);                   break;
        case ConfigureNotify: // Resize the window
            if (e->xconfigure.width != X.width || e->xconfigure.height != X.height) {
                X.width  = e->xconfigure.width;
                X.height = e->xconfigure.height;
                X.pixmap = XCreatePixmap(X.display, X.window, X.width, X.height, X.depth);
                X.xft    = XftDrawCreate(X.display, X.pixmap, X.visual, X.colormap);
                screen_setsize(X.height / X.font->height, X.width / X.font->max_advance_width);
            }
            break;
    }
}

static void scroll_up(void) {
    while (CursorPos < StartRow)
        StartRow = buf_byline(&Buffer, StartRow, -1);
}

static void scroll_dn(void) {
    unsigned nrows, ncols;
    screen_getsize(&nrows, &ncols);
    while (CursorPos > EndPos) {
        StartRow = buf_byline(&Buffer, StartRow, 1);
        unsigned pos = StartRow;
        for (unsigned y = 1; y < nrows; y++) {
            for (unsigned x = 0; x < ncols; x++) {
                Rune r = buf_get(&Buffer, pos++);
                if (r == '\n') { break; }
                if (r == '\t') { x += 4; break; }
            }
        }
        EndPos = pos-1;
    }
}

static void redraw(void) {
    int fheight = X.font->height;
    int fwidth  = X.font->max_advance_width;
    /* Allocate the colors */
    XftColor bkgclr = xftcolor(CLR_BASE03);
    XftColor gtrclr = xftcolor(CLR_BASE02);
    XftColor csrclr = xftcolor(CLR_BASE3);
    XftColor txtclr = xftcolor(CLR_BASE0);

    /* Scroll up or down if needed */
    if (CursorPos < StartRow)
        scroll_up();
    else if (CursorPos > EndPos)
        scroll_dn();

    /* Update the screen buffer */
    unsigned nrows, ncols, pos = StartRow;
    unsigned csrx = 0, csry = 1;
    screen_getsize(&nrows,&ncols);
    screen_clearrow(0);
    for (unsigned y = 1; y < nrows; y++) {
        screen_clearrow(y);
        for (unsigned x = 0; x < ncols; x++) {
            if (CursorPos == pos)
                csrx = x, csry = y;
            Rune r = buf_get(&Buffer, pos++);
            if (r == '\n') {
                screen_setcell(y,x,' ');
                break;
            } else if (r == '\t') {
                screen_setcell(y,x,' ');
                x += TabWidth - (x % TabWidth) - 1;
            } else {
                screen_setcell(y,x,r);
            }
        }
    }
    EndPos = pos-1;

    /* draw the background colors */
    XftDrawRect(X.xft, &bkgclr, 0, 0, X.width, X.height);
    XftDrawRect(X.xft, &gtrclr, 0, 0, X.width, fheight);
    XftDrawRect(X.xft, &gtrclr, 79 * fwidth, 0, fwidth, X.height);

    /* flush the screen buffer */
    for (unsigned y = 0; y < nrows; y++) {
        Row* row = screen_getrow(y);
        if (row->len > 0)
            XftDrawString32(X.xft, &txtclr, X.font, 0, (y+1) * fheight, (FcChar32*)(row->cols), (row->len));
    }

    /* Place cursor on screen */
    Rune csrrune = screen_getcell(csry,csrx);
    if (InsertMode) {
        XftDrawRect(X.xft, &csrclr, csrx * fwidth, csry * fheight + X.font->descent, 2, fheight);
    } else {
        XftDrawRect(X.xft, &csrclr, csrx * fwidth, csry * fheight + X.font->descent, fwidth, fheight);
        XftDrawString32(X.xft, &bkgclr, X.font, csrx * fwidth, (csry+1) * fheight, (FcChar32*)&csrrune, 1);
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
    deinit();
    return 0;
}
