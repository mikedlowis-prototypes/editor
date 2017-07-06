#define _XOPEN_SOURCE 700
#include <stdc.h>
#include <unistd.h>
#ifdef __MACH__
    #include <util.h>
#else
    #include <pty.h>
#endif
#include <poll.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/types.h>
#include <x11.h>
#include <utf.h>
#include <edit.h>
#include <ctype.h>
#include <win.h>

int CmdFD = -1;
char* ShellCmd[] = { NULL, NULL };

bool fdready(int fd);

/* unused functions */
void onfocus(bool focused) {}

void onerror(char* msg) {
}

void onscroll(double percent) {
    size_t bend = buf_end(win_buf(EDIT));
    size_t off  = (size_t)((double)bend * percent);
    view_scrollto(win_view(EDIT), (off >= bend ? bend : off));
}

void onlayout(void) {
    /* calculate and update scroll region */
    View* view = win_view(EDIT);
    size_t bend = buf_end(win_buf(EDIT));
    if (bend == 0) bend = 1;
    if (!view->rows) return;
    size_t vbeg = view->rows[0]->off;
    size_t vend = view->rows[view->nrows-1]->off + view->rows[view->nrows-1]->rlen;
    double scroll_vis = (double)(vend - vbeg) / (double)bend;
    double scroll_off = ((double)vbeg / (double)bend);
    win_setscroll(scroll_off, scroll_vis);
}

void onupdate(void) {
    static char buf[8192];
    long n;
    if (!fdready(CmdFD)) return;
    if ((n = read(CmdFD, buf, sizeof(buf))) < 0)
        die("read() subprocess :");
    for (long i = 0; i < n;) {
        Rune rune = 0;
        size_t length = 0;
        while (!utf8decode(&rune, &length, buf[i++]));
        view_insert(win_view(EDIT), false, rune);
    }
}

void onshutdown(void) {
    x11_deinit();
}

void onmouseleft(WinRegion id, bool pressed, size_t row, size_t col) {
}

void onmousemiddle(WinRegion id, bool pressed, size_t row, size_t col) {
}

void onmouseright(WinRegion id, bool pressed, size_t row, size_t col) {
}

static void oninput(Rune rune) {
    char c = (char)rune;
    write(CmdFD, &c, 1);
    view_insert(win_view(FOCUSED), false, rune);
}

bool update_needed(void) {
    return fdready(CmdFD);
}

/******************************************************************************/

bool fdready(int fd) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    struct timeval tv = { .tv_usec = 0 };
    return (select(fd+1, &fds, NULL, NULL, &tv) > 0);
}

int pty_spawn(char** argv) {
    int fd;
    struct termios tio;
    pid_t pid;
    putenv("TERM=dumb");
    switch ( (pid = forkpty(&fd, NULL, NULL, NULL)) ) {
        case -1: // Failed
            die("forkpty() :");
            break;

        case 0: // Child Process
            if (execvp(argv[0], argv) < 0)
                die("execvp('%s', ...) :", argv[0]);
            exit(EXIT_FAILURE);
            break;

        default: // Parent Process
            tcgetattr(fd, &tio);
            tio.c_lflag      &= ~(ECHO | ECHONL);
            tio.c_cc[ VMIN ]  = 1;
            tio.c_cc[ VTIME ] = 0;
            tcsetattr(fd, TCSANOW, &tio);
            break;
    }
    return fd;
}

/******************************************************************************/

#ifndef TEST
int main(int argc, char** argv) {
    if (!ShellCmd[0]) ShellCmd[0] = getenv("SHELL");
    if (!ShellCmd[0]) ShellCmd[0] = "/bin/sh";
    CmdFD = pty_spawn(argc > 1 ? argv+1 : ShellCmd);
    win_window("term", onerror);
    win_setkeys(NULL, oninput);
    win_buf(EDIT)->crlf = 1;
    win_loop();
    return 0;
}
#endif
