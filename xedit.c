#define _GNU_SOURCE
#include <time.h>
#include <signal.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <X11/cursorfont.h>

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
    XIC xic;
    XIM xim;
} X;
Buf Buffer;
bool InsertMode = false;
unsigned StartRow = 0;
unsigned EndRow = 0;
unsigned CursorPos = 0;
unsigned LastDrawnPos = 0;

void die(char* m) {
    fprintf(stderr, "dying, %s\n", m);
    exit(1);
}

static uint64_t time_ms(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (ts.tv_sec * 1000000000L + ts.tv_nsec) / 1000000L;
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

    /* set input methods */
    if((X.xim = XOpenIM(X.display, 0, 0, 0)))
        X.xic = XCreateIC(X.xim, XNInputStyle, XIMPreeditNothing|XIMStatusNothing, XNClientWindow, X.window, XNFocusWindow, X.window, NULL);

    /* allocate font */
    if(!(X.font = XftFontOpenName(X.display, X.screen, FONTNAME)))
        die("cannot open default font");

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
            if (buf_bol(&Buffer, CursorPos) > EndRow)
                EndRow++, StartRow = buf_byline(&Buffer, StartRow, 1);
            break;

        case XK_Up:
            CursorPos = buf_byline(&Buffer, CursorPos, -1);
            if (CursorPos < StartRow)
                EndRow--, StartRow = buf_byline(&Buffer, StartRow, -1);
            break;

        //default:
        //    if (len == 0)
        //        continue;
        //    if (buf[0] == '\r')
        //        buf[0] = '\n';
        //    utf8_decode_rune(&gev->key, (unsigned char *)buf, 8);
        //    break;
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
            StartRow = buf_byline(&Buffer, StartRow, -2);
            break;
        case Button5: /* Wheel Down */
            StartRow = buf_byline(&Buffer, StartRow, 2);
            break;
    }
}

static void handle_event(XEvent* e) {
    switch (e->type) {
        case FocusIn:
            if (X.xic) XSetICFocus(X.xic);
            break;

        case FocusOut:
            if (X.xic) XUnsetICFocus(X.xic);
            break;

        case Expose: // Draw the window
            XCopyArea(X.display, X.pixmap, X.window, X.gc, 0, 0, X.width, X.height, 0, 0);
            XFlush(X.display);
            break;

        case ConfigureNotify: // Resize the window
            if (e->xconfigure.width != X.width || e->xconfigure.height != X.height) {
                X.width  = e->xconfigure.width;
                X.height = e->xconfigure.height;
                X.pixmap = XCreatePixmap(X.display, X.window, X.width, X.height, X.depth);
                X.xft    = XftDrawCreate(X.display, X.pixmap, X.visual, X.colormap);
            }
            break;

        case ButtonPress: // Mouse button handling
            handle_mousebtn(e);
            break;

        case KeyPress: // Keyboard events
            handle_key(e);
            break;
    }
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

static void redraw(void) {
    static uint64_t last_draw = 0;
    uint64_t current = time_ms();
    //if (current - last_draw < 200)
    //    return;
    last_draw = current;

    puts("redraw");
    uint64_t t1start = time_ms(), t1end;
    int fheight = X.font->height;
    int fwidth  = X.font->max_advance_width;
    /* Allocate the colors */
    XftColor bkgclr = xftcolor(CLR_BASE03);
    XftColor gtrclr = xftcolor(CLR_BASE02);
    XftColor csrclr = xftcolor(CLR_BASE3);
    XftColor txtclr = xftcolor(CLR_BASE0);
    /* draw the background color */
    XftDrawRect(X.xft, &bkgclr, 0, 0, X.width, X.height);
    /* draw the status background */
    XftDrawRect(X.xft, &gtrclr, 0, 0, X.width, fheight);
    /* Draw document text */
    uint64_t t2start = time_ms(), t2end;
    int x = 0, y = 2;
    for (LastDrawnPos = StartRow; LastDrawnPos < buf_end(&Buffer); LastDrawnPos++) {
        if (x * fwidth >= X.width)
            y++, x = 0;
        if (y * fheight >= X.height)
            break;
        FcChar32 ch = (FcChar32)buf_get(&Buffer, LastDrawnPos);
        XftColor *bgclr = &bkgclr, *fgclr = &txtclr;
        /* Draw the cursor background */
        if (LastDrawnPos == CursorPos) {
            bgclr = &csrclr, fgclr = &bkgclr;
            if (InsertMode)
                fgclr = &txtclr, XftDrawRect(X.xft, bgclr, x * fwidth, ((y-1) * fheight) + 4, 1, fheight);
            else
                XftDrawRect(X.xft, bgclr, x * fwidth, ((y-1) * fheight) + 4, fwidth, fheight);
        }
        /* Draw the actual character */
        if (ch == '\n') { y++, x = 0;    continue; }
        if (ch == '\t') { x += TabWidth; continue; }
        XftDrawString32(X.xft, fgclr, X.font, x * fwidth, y * fheight, (FcChar32 *)&ch, 1);
        x++;
    }
    EndRow = buf_bol(&Buffer, LastDrawnPos-2);
    t2end = time_ms();
    printf("text time: %lu\n", t2end - t2start);
    /* flush the pixels to the screen */
    XCopyArea(X.display, X.pixmap, X.window, X.gc, 0, 0, X.width, X.height, 0, 0);
    XFlush(X.display);
    t1end = time_ms();
    printf("redraw time: %lu\n", t1end - t1start);
}

int main(int argc, char** argv) {
    init();
    buf_init(&Buffer);
    if (argc > 1)
        buf_load(&Buffer, argv[1]);
    XEvent e;

        while(XNextEvent(X.display, &e) >= 0) {
            handle_event(&e);
            if (!XPending(X.display)) {
                redraw();
            }
        }
    //while (true) {
        //XPeekEvent(X.display,&e);
        //while (XPending(X.display)) {
        //    XNextEvent(X.display, &e);
        //    if (!XFilterEvent(&e, None))
        //        handle_event(&e);
        //}
        //redraw();
    //}
    deinit();
    return 0;
}
