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
ScreenBuf ScreenBuffer;

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

#define StartRow (ScreenBuffer.off)

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
            StartRow = buf_byline(&Buffer, StartRow, 1);
            //CursorPos = buf_byline(&Buffer, CursorPos, 1);
            //if (buf_bol(&Buffer, CursorPos) > EndRow)
            //    EndRow++, StartRow = buf_byline(&Buffer, StartRow, 1);
            break;

        case XK_Up:
            StartRow = buf_byline(&Buffer, StartRow, -1);
            //CursorPos = buf_byline(&Buffer, CursorPos, -1);
            //if (CursorPos < StartRow)
            //    EndRow--, StartRow = buf_byline(&Buffer, StartRow, -1);
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
                X.rows = X.height / X.font->height;
                X.cols = X.width  / X.font->max_advance_width;
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

static void redraw(void) {
    uint64_t tstart;
    int fheight = X.font->height;
    int fwidth  = X.font->max_advance_width;

    /* Allocate the colors */
    XftColor bkgclr = xftcolor(CLR_BASE03);
    XftColor gtrclr = xftcolor(CLR_BASE02);
    XftColor csrclr = xftcolor(CLR_BASE3);
    XftColor txtclr = xftcolor(CLR_BASE0);

    /* Update the screen buffer */
    tstart = time_ms();
    LastDrawnPos = StartRow;
    int csrx = 0, csry = 0;
    for (int y = 0; y < 73; y++) {
        ScreenBuffer.rows[y].off = LastDrawnPos;
        ScreenBuffer.rows[y].len = 0;
        for (int x = 0; x < 100; x++) {
            if (CursorPos == LastDrawnPos)
                csrx = x, csry = y;
            Rune r = buf_get(&Buffer, LastDrawnPos++);
            if (r == '\n') { break; }
            if (r == '\t') { x += 4; break; }
            ScreenBuffer.rows[y].cols[x] = r;
            ScreenBuffer.rows[y].len++;
        }
    }
    printf("\nT1: %lu\n", time_ms() - tstart);

    /* draw the background colors */
    XftDrawRect(X.xft, &bkgclr, 0, 0, X.width, X.height);
    XftDrawRect(X.xft, &gtrclr, 0, 0, X.width, fheight);
    XftDrawRect(X.xft, &gtrclr, 80 * fwidth, 0, fwidth, X.height);

    /* flush the screen buffer */
    tstart = time_ms();
    for (int y = 0; y < 73; y++)
        if (ScreenBuffer.rows[y].len > 0)
            XftDrawString32(X.xft, &txtclr, X.font, 0, (y+2) * fheight, (FcChar32*)(ScreenBuffer.rows[y].cols), (ScreenBuffer.rows[y].len));
    printf("T2: %lu\n", time_ms() - tstart);

    /* Place cursor on screen */
    XftDrawRect(X.xft, &txtclr, csrx * fwidth, (csry+1) * fheight + 3, fwidth, fheight);
    XftDrawString32(X.xft, &bkgclr, X.font, csrx * fwidth, (csry+2) * fheight, (FcChar32*)&(ScreenBuffer.rows[csry].cols[csrx]), 1);

    /* flush pixels to the screen */
    tstart = time_ms();
    XCopyArea(X.display, X.pixmap, X.window, X.gc, 0, 0, X.width, X.height, 0, 0);
    XFlush(X.display);
    printf("T3: %lu\n", time_ms() - tstart);

    //uint64_t tstart;
    //printf("rows: %d, cols: %d\n", X.rows, X.cols);

    //tstart = time_ms();
    //int fheight = X.font->height;
    //int fwidth  = X.font->max_advance_width;
    ///* Allocate the colors */
    //XftColor bkgclr = xftcolor(CLR_BASE03);
    //XftColor gtrclr = xftcolor(CLR_BASE02);
    //XftColor csrclr = xftcolor(CLR_BASE3);
    //XftColor txtclr = xftcolor(CLR_BASE0);
    ///* draw the background color */
    //XftDrawRect(X.xft, &bkgclr, 0, 0, X.width, X.height);
    ///* draw the status background */
    //XftDrawRect(X.xft, &gtrclr, 0, 0, X.width, fheight);
    //printf("\nT1: %lu\n", time_ms() - tstart);

    ///* Draw document text */
    //tstart = time_ms();
    //int x = 0, y = 2;
    //for (LastDrawnPos = StartRow; LastDrawnPos < buf_end(&Buffer); LastDrawnPos++) {
    //    if (x * fwidth >= X.width)
    //        y++, x = 0;
    //    if (y * fheight >= X.height)
    //        break;
    //    FcChar32 ch = (FcChar32)buf_get(&Buffer, LastDrawnPos);
    //    XftColor *bgclr = &bkgclr, *fgclr = &txtclr;
    //    /* Draw the cursor background */
    //    if (LastDrawnPos == CursorPos) {
    //        bgclr = &csrclr, fgclr = &bkgclr;
    //        if (InsertMode)
    //            fgclr = &txtclr, XftDrawRect(X.xft, bgclr, x * fwidth, ((y-1) * fheight) + 4, 1, fheight);
    //        else
    //            XftDrawRect(X.xft, bgclr, x * fwidth, ((y-1) * fheight) + 4, fwidth, fheight);
    //    }
    //    /* Draw the actual character */
    //    if (ch == '\n') { y++, x = 0;    continue; }
    //    if (ch == '\t') { x += TabWidth; continue; }
    //    XftDrawString32(X.xft, fgclr, X.font, x * fwidth, y * fheight, (FcChar32 *)&ch, 1);
    //    x++;
    //}
    //EndRow = buf_bol(&Buffer, LastDrawnPos-2);
    //printf("T2: %lu\n", time_ms() - tstart);


    //tstart = time_ms();
    ///* flush the pixels to the screen */
    //XCopyArea(X.display, X.pixmap, X.window, X.gc, 0, 0, X.width, X.height, 0, 0);
    //printf("T3: %lu\n", time_ms() - tstart);

    //tstart = time_ms();
    //XFlush(X.display);
    //printf("T4: %lu\n", time_ms() - tstart);
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