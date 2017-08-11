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
} X;

static void* prop_get(Window win, char* propname, Atom type, unsigned long* nitems);
static void prop_set(Window win, char* propname, Atom type, int format, void* items, unsigned long nitems);

char* abspath(char* path) {
    char* newpath = calloc(1, PATH_MAX+1);
    return realpath(path, newpath);
}

/* Main Routine
 ******************************************************************************/
int main(int argc, char** argv) {
    if (!(X.display = XOpenDisplay(0)))
        die("could not open display");
    X.root = DefaultRootWindow(X.display);
    X.self = XCreateSimpleWindow(X.display, X.root, 0, 0, 1, 1, 0, 0, 0);

    XGrabServer(X.display);
    unsigned long nwindows = 0;
    Window* windows = prop_get(X.root, "TIDE_WINDOWS", XA_WINDOW, &nwindows);
    printf("Windows: %lu\n", nwindows);
    XUngrabServer(X.display);

    for (int i = 1; i < argc; i++) {
        printf("abspath: '%s'\n", abspath(argv[i]));
    }

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
