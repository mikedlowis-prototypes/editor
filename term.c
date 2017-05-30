#include <stdc.h>
#include <x11.h>
#include <utf.h>
#include <edit.h>
#include <ctype.h>
#include <win.h>

static int ShellFD;

#define INCLUDE_DEFS
#include "config.h"

void onmouseleft(WinRegion id, bool pressed, size_t row, size_t col) {
}

void onmousemiddle(WinRegion id, bool pressed, size_t row, size_t col) {
}

void onmouseright(WinRegion id, bool pressed, size_t row, size_t col) {
}

void onscroll(double percent) {
}

void onfocus(bool focused) {
}

void onupdate(void) {
}

void onlayout(void) {
}

void onshutdown(void) {
    x11_deinit();
}

void onerror(char* msg) {
}

#ifndef TEST
#include <unistd.h>
#include <sys/ioctl.h>
#ifdef __MACH__
#include <util.h>
#else
#include <pty.h>
#endif

void spawn_shell(int master, int slave) {
	static char* shell[] = { "/bin/sh", "-l", NULL };
    dup2(slave, 0);
    dup2(slave, 1);
    dup2(slave, 2);
    if (ioctl(slave, TIOCSCTTY, NULL) < 0)
        die("ioctl(TIOSCTTY) failed");
    close(slave);
    close(master);
    execvp(shell[0], shell);
}

int main(int argc, char** argv) {
    int pid, m, s;
	if (openpty(&m, &s, NULL, NULL, NULL) < 0)
		die("openpty failed: %s\n", strerror(errno));
    if ((pid = fork()))
        die("fork failed");

    if (pid == 0) {
    	spawn_shell(m, s);
    } else {
        close(s);
        ShellFD = m;
        //signal(SIGCHLD, sigchld);
    }

    win_window("term", onerror);
    //win_setkeys(&Bindings);
    //win_setmouse(&MouseHandlers);
    win_loop();

    return 0;
}
#endif
