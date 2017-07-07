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
#include <shortcuts.h>

int CmdFD = -1;
char* ShellCmd[] = { NULL, NULL };
static int SearchDir = DOWN;
static char* SearchTerm = NULL;

bool fdready(int fd);

/* unused functions */
void onfocus(bool focused) {}
void onerror(char* msg) {}

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
    long n = 0, r = 0, i = 0;
    static char buf[8192];
    if (!fdready(CmdFD)) return;
    if ((n = read(CmdFD, buf, sizeof(buf))) < 0)
        die("read() subprocess :");
    while (i < n) {
        Rune rune = 0;
        size_t length = 0;
        while (!utf8decode(&rune, &length, buf[i++]));
        view_insert(win_view(EDIT), false, rune);
    }
    win_buf(EDIT)->outpoint = win_view(EDIT)->selection.end;
}

void onshutdown(void) {
    x11_deinit();
}

void onmouseleft(WinRegion id, bool pressed, size_t row, size_t col) {
    static int count = 0;
    static uint64_t before = 0;
    if (!pressed) return;
    uint64_t now = getmillis();
    count = ((now-before) <= config_get_int(DblClickTime) ? count+1 : 1);
    before = now;

    if (count == 1) {
        if (x11_keymodsset(ModShift))
            view_selext(win_view(id), row, col);
        else
            view_setcursor(win_view(id), row, col);
    } else if (count == 2) {
        view_select(win_view(id), row, col);
    } else if (count == 3) {
        view_selword(win_view(id), row, col);
    }
}

void onmousemiddle(WinRegion id, bool pressed, size_t row, size_t col) {
    if (pressed) return;
    if (win_btnpressed(MouseLeft)) {
        cut();
    } else {
        char* str = view_fetch(win_view(id), row, col);
        //if (str) exec(str);
        free(str);
    }
}

void onmouseright(WinRegion id, bool pressed, size_t row, size_t col) {
    if (pressed) return;
    if (win_btnpressed(MouseLeft)) {
        paste();
    } else {
        SearchDir *= (x11_keymodsset(ModShift) ? -1 : +1);
        free(SearchTerm);
        SearchTerm = view_fetch(win_view(id), row, col);
        if (view_findstr(win_view(EDIT), SearchDir, SearchTerm)) {
            win_setregion(EDIT);
            win_warpptr(EDIT);
        }
    }
}

static void oninput(Rune rune) {
    view_insert(win_view(FOCUSED), false, rune);
    if (win_getregion() == EDIT) {
        size_t point = win_buf(EDIT)->outpoint;
        size_t pos   = win_view(EDIT)->selection.end;
        if (rune == '\n' && pos > point) {
            Sel range = { .beg = point, .end = pos };
            char* str = view_getstr(win_view(EDIT), &range);
            write(CmdFD, str, strlen(str)-1);
            free(str);
        }
    }
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

static void quit(void) {
    x11_deinit();
}

static KeyBinding Bindings[] = {
    /* Cursor Movements */
    { ModAny,  KEY_HOME,  cursor_home  },
    { ModAny,  KEY_END,   cursor_end   },
    { ModAny,  KEY_UP,    cursor_up    },
    { ModAny,  KEY_DOWN,  cursor_dn    },
    { ModAny,  KEY_LEFT,  cursor_left  },
    { ModAny,  KEY_RIGHT, cursor_right },
    { ModCtrl, ';',       cursor_warp  },

    /* Standard Unix Shortcuts */
    { ModCtrl, 'u', del_to_bol  },
    { ModCtrl, 'k', del_to_eol  },
    { ModCtrl, 'w', del_to_bow  },
    { ModCtrl, 'a', cursor_bol  },
    { ModCtrl, 'e', cursor_eol  },

    /* Standard Text Editing Shortcuts */
//    { ModCtrl,          's', save        },
    { ModCtrl,          'z', undo        },
    { ModCtrl,          'y', redo        },
    { ModCtrl,          'x', cut         },
    { ModCtrl,          'c', copy        },
    { ModCtrl,          'v', paste       },
    { ModCtrl,          'j', join_lines  },
    { ModCtrl,          'l', select_line },
    { ModCtrl|ModShift, 'a', select_all  },

    /* Common Special Keys */
    { ModNone, KEY_PGUP,      page_up   },
    { ModNone, KEY_PGDN,      page_dn   },
    { ModAny,  KEY_DELETE,    delete    },
    { ModAny,  KEY_BACKSPACE, backspace },

    /* Implementation Specific */
    { ModNone,      KEY_ESCAPE, select_prev  },
    { ModCtrl,      't',        change_focus },
    { ModCtrl,      'q',        quit         },
//    { ModCtrl,      'h',        highlight    },
//    { ModOneOrMore, 'f',        search       },
//    { ModCtrl,      'd',        execute      },
//    { ModCtrl,      'n',        new_win      },
    { 0, 0, 0 }
};


#ifndef TEST
int main(int argc, char** argv) {
    if (!ShellCmd[0]) ShellCmd[0] = getenv("SHELL");
    if (!ShellCmd[0]) ShellCmd[0] = "/bin/sh";
    CmdFD = pty_spawn(argc > 1 ? argv+1 : ShellCmd);
    win_window("term", onerror);
    win_setkeys(Bindings, oninput);
    win_buf(EDIT)->crlf = 1;
    config_set_int(TabWidth, 8);
    win_loop();
    return 0;
}
#endif
