#define _XOPEN_SOURCE 700
#include <stdc.h>
#include <x11.h>
#include <utf.h>
#include <edit.h>
#include <win.h>
#include <shortcuts.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/select.h>
#ifdef __MACH__
    #include <util.h>
#else
    #include <pty.h>
#endif

static int PtyFD = -1;

static void update(int fd, void* data);

bool pty_active(void) { return (PtyFD >= 0);}
void pty_send_intr(void) { (void)write(PtyFD, "\x03", 1); }
void pty_send_eof(void) { (void)write(PtyFD, "\x04", 1); }
void pty_send_susp(void) { (void)write(PtyFD, "\x1A", 1); }

void pty_spawn(char** argv) {
    assert(!pty_active());
    struct termios tio;
    pid_t pid;
    putenv("TERM=dumb");
    switch ( (pid = forkpty(&PtyFD, NULL, NULL, NULL)) ) {
        case -1: // Failed
            die("forkpty() :");
            break;

        case 0: // Child Process
            if (execvp(argv[0], argv) < 0)
                die("execvp('%s', ...) :", argv[0]);
            exit(EXIT_FAILURE);
            break;

        default: // Parent Process
            tcgetattr(PtyFD, &tio);
            tio.c_lflag      &= ~(ECHO | ECHONL);
            tio.c_cc[ VMIN ]  = 1;
            tio.c_cc[ VTIME ] = 0;
            tcsetattr(PtyFD, TCSANOW, &tio);
            break;
    }
    event_watchfd(PtyFD, INPUT, update, NULL);
}

void pty_send(char* str) {
    if (!str) return;
    view_eof(win_view(EDIT), false);
    bool has_eol = (str[strlen(str)-1] == '\n');
    while (*str) {
        Rune rune = 0;
        size_t length = 0;
        while (!utf8decode(&rune, &length, *str++));
        pty_send_rune(rune);
    }
    if (!has_eol)
        pty_send_rune('\n');
}

void pty_send_rune(Rune rune) {
    view_insert(win_view(EDIT), false, rune);
    size_t point = win_buf(EDIT)->outpoint;
    size_t pos   = win_view(EDIT)->selection.end;
    if ((rune == '\n' || rune == RUNE_CRLF) && pos > point) {
        Sel range = { .beg = point, .end = pos };
        char* str = view_getstr(win_view(EDIT), &range);
        if (write(PtyFD, str, strlen(str)-1) < 0)
            PtyFD = -1;
        free(str);
    }
}

static void update(int fd, void* data) {
    /* Read from command if we have one */
    long n = 0, i = 0;
    static char cmdbuf[8192];
    if ((n = read(PtyFD, cmdbuf, sizeof(cmdbuf))) < 0)
        PtyFD = -1;
    while (i < n) {
        Rune rune = 0;
        size_t length = 0;
        while (!utf8decode(&rune, &length, cmdbuf[i++]));
        view_insert(win_view(EDIT), false, rune);
    }
    win_buf(EDIT)->outpoint = win_view(EDIT)->selection.end;
}
