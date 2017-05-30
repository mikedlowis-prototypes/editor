#include <stdc.h>
#include <utf.h>
#include <edit.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <unistd.h>

char* SelText;     // The text of the clipboard selection
Atom SelType;      // The X11 selection name. Always CLIPBOARD
Atom SelTarget;    // The conversion target for the selection (always string or utf8)
Display* XDisplay; // Handle for the X connection
Window XRoot;      // The root X window
Window XWindow;    // Our window used to serve the selection up

void serve_selection(void) {
    if (!(XDisplay = XOpenDisplay(0)))
        die("could not open display");
    XRoot = DefaultRootWindow(XDisplay);
    XWindow = XCreateSimpleWindow(XDisplay, XRoot, 0, 0, 1, 1, 0, 0, 0);
    SelType   = XInternAtom(XDisplay, "CLIPBOARD", 0);
    SelTarget = XInternAtom(XDisplay, "UTF8_STRING", 0);
    if (SelTarget == None)
        SelTarget = XInternAtom(XDisplay, "STRING", 0);
    XSetSelectionOwner(XDisplay, SelType, XWindow, CurrentTime);

    for (XEvent e;;) {
        XNextEvent(XDisplay, &e);
        if (e.type == SelectionRequest) {
            XEvent s;
            s.xselection.type      = SelectionNotify;
            s.xselection.property  = e.xselectionrequest.property;
            s.xselection.requestor = e.xselectionrequest.requestor;
            s.xselection.selection = e.xselectionrequest.selection;
            s.xselection.target    = e.xselectionrequest.target;
            s.xselection.time      = e.xselectionrequest.time;
            Atom target    = e.xselectionrequest.target;
            Atom xatargets = XInternAtom(XDisplay, "TARGETS", 0);
            Atom xastring  = XInternAtom(XDisplay, "STRING", 0);
            if (target == xatargets) {
                /* respond with the supported type */
                XChangeProperty(
                    XDisplay, s.xselection.requestor, s.xselection.property,
                    XA_ATOM, 32, PropModeReplace,
                    (unsigned char*)&SelTarget, 1);
            } else if (target == SelTarget || target == xastring) {
                XChangeProperty(
                    XDisplay, s.xselection.requestor, s.xselection.property,
                    SelTarget, 8, PropModeReplace,
                    (unsigned char*)SelText, strlen(SelText));
            }
            XSendEvent(XDisplay, s.xselection.requestor, True, 0, &s);
        } else if (e.type == SelectionClear) {
            break; // Someone else took over. We're done here.
        }
    }
}

int main(int argc, char** argv) {
    SelText = fdgets(STDIN_FILENO);
    if (SelText) {
        int pid = fork();
        if (pid == 0) {
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
            chdir("/");
            serve_selection();
        } else if (pid < 0) {
            die("fork() failed");
        }
    }
    return 0;
}
