#include <stdc.h>
#include <utf.h>
#include <edit.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <unistd.h>

struct {
    Display* display;
    Window root;
    Window self;
    int error;
} X;

size_t WinCount;
Window* Windows;
char**  WinFiles;

static void get_windows(Window** wins, char*** files, size_t* nwins);
static int error_handler(Display* disp, XErrorEvent* ev);
static void* prop_get(Window win, char* propname, Atom type, unsigned long* nitems);
static void prop_set(Window win, char* propname, Atom type, int format, void* items, unsigned long nitems);
static void edit(char* path);
static Window win_byfile(char* path);
static void focus_window(Window w);

/* Main Routine
 ******************************************************************************/
int main(int argc, char** argv) {
    if (!(X.display = XOpenDisplay(0)))
        die("could not open display");
    X.root = DefaultRootWindow(X.display);
    X.self = XCreateSimpleWindow(X.display, X.root, 0, 0, 1, 1, 0, 0, 0);
    XSetErrorHandler(error_handler);
    get_windows(&Windows, &WinFiles, &WinCount);

    for (int i = 1; i < argc; i++) {
        bool last = (i == argc-1);
        char* path = realpath(argv[i], NULL);
        if (!path) path = argv[i];
        Window win = win_byfile(path);
        if (!win)
            edit(path);
        else if (last)
            focus_window(win);
    }
	XFlush(X.display);
    return 0;
}

static void get_windows(Window** wins, char*** files, size_t* nwins) {
    XGrabServer(X.display);
    unsigned long nwindows = 0, nactive = 0, nstrings = 0;
    Window *windows = prop_get(X.root, "TIDE_WINDOWS", XA_WINDOW, &nwindows);
    Window *active  = calloc(nwindows, sizeof(Window));
    char   **wfiles  = calloc(nwindows, sizeof(char*));
    Atom xa_comm = XInternAtom(X.display, "TIDE_COMM", False);
    for (int i = 0; i < nwindows; i++) {
        X.error = 0;
        int nprops;
        Atom* props = XListProperties(X.display, windows[i], &nprops);
        if (!props || X.error) continue;
        for (int x = 0; x < nprops; x++) {
            if (props[x] == xa_comm) {
                active[nactive] = windows[i];
                wfiles[nactive] = prop_get(windows[i], "TIDE_FILE", XA_STRING, &nstrings);
                nactive++;
                break;
            }
        }
        XFree(props);
    }
    prop_set(X.root, "TIDE_WINDOWS", XA_WINDOW, 32, active, nactive);
    XSync(X.display, False);
    XUngrabServer(X.display);
    XFree(windows);
    *wins  = active, *files = wfiles, *nwins = nactive;
}

static int error_handler(Display* disp, XErrorEvent* ev) {
    X.error = ev->error_code;
    return 0;
}

static void* prop_get(Window win, char* propname, Atom type, unsigned long* nitems) {
    Atom rtype, prop = XInternAtom(X.display, propname, False);
    unsigned long rformat = 0, nleft = 0;
    unsigned char* data = NULL;
    XGetWindowProperty(X.display, win, prop, 0, -1, False, type, &rtype,
                       (int*)&rformat, nitems, &nleft, &data);
    if (rtype != type)
        data = NULL, *nitems = 0;
    return data;
}

static void prop_set(Window win, char* propname, Atom type, int format, void* items, unsigned long nitems) {
    Atom prop = XInternAtom(X.display, propname, False);
    XChangeProperty(X.display, win, prop, type, format, PropModeReplace, items, (int)nitems);
}

static void edit(char* path) {
    if (fork() == 0)
    	execv("tide", (char*[]){ "tide", path, NULL });
}

static Window win_byfile(char* path) {
    for (int i = 0; i < WinCount; i++)
        if (WinFiles[i] && !strcmp(path, WinFiles[i]))
            return Windows[i];
    return (Window)0;
}

static void focus_window(Window w) {
	XEvent ev = {0};
	ev.xclient.type = ClientMessage;
    ev.xclient.send_event = True;
    ev.xclient.message_type = XInternAtom(X.display, "_NET_ACTIVE_WINDOW", False);
	ev.xclient.window = w;
	ev.xclient.format = 32;
    long mask = SubstructureRedirectMask | SubstructureNotifyMask;
	XSendEvent(X.display, X.root, False, mask, &ev);
    XMapRaised(X.display, w);
    XFlush(X.display);
}
