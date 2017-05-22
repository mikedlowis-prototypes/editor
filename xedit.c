#include <stdc.h>
#include <x11.h>
#include <utf.h>
#include <edit.h>
#include <ctype.h>
#include <win.h>
#include <shortcuts.h>

typedef struct {
    char* tag;
    union {
        void (*noarg)(void);
        void (*arg)(char* arg);
    } action;
} Tag;

/* The shell: Filled in with $SHELL. Used to execute commands */
static char* ShellCmd[] = { NULL, "-c", NULL, NULL };
static char* SedCmd[] = { "sed", "-e", NULL, NULL };
static char* PickFileCmd[] = { "xfilepick", ".", NULL };
static char* PickTagCmd[] = { "xtagpick", NULL, "tags", NULL, NULL };
static char* OpenCmd[] = { "xedit", NULL, NULL };
static Tag Builtins[];
static int SearchDir = DOWN;
static char* SearchTerm = NULL;

/* Tag/Cmd Execution
 ******************************************************************************/
static Tag* tag_lookup(char* cmd) {
    size_t len = 0;
    Tag* tags = Builtins;
    for (char* tag = cmd; *tag && !isspace(*tag); tag++, len++);
    while (tags->tag) {
        if (!strncmp(tags->tag, cmd, len))
            return tags;
        tags++;
    }
    return NULL;
}

static void tag_exec(Tag* tag, char* arg) {
    /* if we didnt get an arg, find one in the selection */
    if (!arg) arg = view_getstr(win_view(TAGS), NULL);
    if (!arg) arg = view_getstr(win_view(EDIT), NULL);
    /* execute the tag handler */
    tag->action.arg(arg);
    free(arg);
}

static void cmd_exec(char* cmd) {
    char op = '\0';
    if (*cmd == ':' || *cmd == '!' || *cmd == '<' || *cmd == '|' || *cmd == '>')
        op = *cmd, cmd++;
    ShellCmd[2] = cmd;
    /* execute the command */
    char *input = NULL, *output = NULL, *error = NULL;
    WinRegion dest = EDIT;
    if (op && op != '<' && op != '!' && 0 == view_selsize(win_view(EDIT)))
        win_view(EDIT)->selection = (Sel){ .beg = 0, .end = buf_end(win_buf(EDIT)) };
    input = view_getstr(win_view(EDIT), NULL);

    if (op == '!') {
        cmdrun(ShellCmd, NULL);
    } else if (op == '>') {
        dest = TAGS;
        output = cmdwriteread(ShellCmd, input, &error);
    } else if (op == '|') {
        output = cmdwriteread(ShellCmd, input, &error);
    } else if (op == ':') {
        SedCmd[2] = cmd;
        output = cmdwriteread(SedCmd, input, &error);
    } else {
        if (op != '<') dest = win_getregion();
        output = cmdread(ShellCmd, &error);
    }

    if (error)
        view_append(win_view(TAGS), chomp(error));

    if (output) {
        if (op == '>')
            view_append(win_view(dest), chomp(output));
        else
            view_putstr(win_view(dest), output);
        win_setregion(dest);
    }
    /* cleanup */
    free(input);
    free(output);
    free(error);
}

static void exec(char* cmd) {
    /* skip leading space */
    for (; *cmd && isspace(*cmd); cmd++);
    if (!*cmd) return;
    /* see if it matches a builtin tag */
    Tag* tag = tag_lookup(cmd);
    if (tag) {
        while (*cmd && !isspace(*cmd++));
        tag_exec(tag, (*cmd ? stringdup(cmd) : NULL));
    } else {
        cmd_exec(cmd);
    }
}

/* Action Callbacks
 ******************************************************************************/
static void onerror(char* msg) {
    view_append(win_view(TAGS), msg);
}

static void quit(void) {
    static uint64_t before = 0;
    uint64_t now = getmillis();
    if (!win_buf(EDIT)->modified || (now-before) <= 250) {
        #ifndef TEST
        x11_deinit();
        #else
        exit(0);
        #endif
    } else {
        view_append(win_view(TAGS),
            "File is modified. Repeat action twice in < 250ms to quit.");
    }
    before = now;
}

static void save(void) {
    Buf* buf = win_buf(EDIT);
    if (TrimOnSave && buf_end(buf) > 0) {
        View* view = win_view(EDIT);
        unsigned off = 0, prev = 1;
        /* loop through the buffer till we hit the end or we stop advancing */
        while (off < buf_end(buf) && prev != off) {
            off = buf_eol(buf, off);
            Rune r = buf_get(buf, off-1);
            for (; (r == ' ' || r == '\t'); r = buf_get(buf, off-1)) {
                if (off <= view->selection.beg) {
                    view->selection.end--;
                    view->selection.beg--;
                }
                off = buf_delete(buf, off-1, off);
            }
            /* make sure we keep advancing */
            prev = off;
            off  = buf_byline(buf, off, +1);
        }
    }
    buf_save(buf);
}

/* Mouse Handling
 ******************************************************************************/
void onmouseleft(WinRegion id, size_t count, size_t row, size_t col) {
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

void onmousemiddle(WinRegion id, size_t count, size_t row, size_t col) {
    if (win_btnpressed(MOUSE_BTN_LEFT)) {
        cut();
    } else {
        char* str = view_fetch(win_view(id), row, col);
        if (str) exec(str);
        free(str);
    }
}

void onmouseright(WinRegion id, size_t count, size_t row, size_t col) {
    if (win_btnpressed(MOUSE_BTN_LEFT)) {
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

/* Keyboard Handling
 ******************************************************************************/
static void saveas(char* arg) {
    if (arg) {
        char* path = win_buf(EDIT)->path;
        win_buf(EDIT)->path = stringdup(arg);
        buf_save(win_buf(EDIT));
        free(path);
    }
}

static void tag_undo(void) {
    view_undo(win_view(EDIT));
}

static void tag_redo(void) {
    view_redo(win_view(EDIT));
}

static void search(void) {
    char* str;
    SearchDir *= (x11_keymodsset(ModShift) ? -1 : +1);
    if (x11_keymodsset(ModAlt) && SearchTerm)
        str = stringdup(SearchTerm);
    else
        str = view_getctx(win_view(FOCUSED));
    view_findstr(win_view(EDIT), SearchDir, str);
    free(SearchTerm);
    SearchTerm = str;
    if (view_selsize(win_view(EDIT))) {
        win_setregion(EDIT);
        win_warpptr(EDIT);
    }
}

static void execute(void) {
    char* str = view_getcmd(win_view(FOCUSED));
    if (str) exec(str);
    free(str);
}

static void find(char* arg) {
    SearchDir *= (x11_keymodsset(ModShift) ? -1 : +1);
    view_findstr(win_view(EDIT), SearchDir, arg);
}

static void open_file(void) {
    char* file = cmdread(PickFileCmd, NULL);
    if (file) {
        file = chomp(file);
        if (!win_buf(EDIT)->path && !win_buf(EDIT)->modified) {
            buf_load(win_buf(EDIT), file);
        } else {
            OpenCmd[1] = file;
            cmdrun(OpenCmd, NULL);
        }
    }
    free(file);
}

static void pick_symbol(char* symbol) {
    PickTagCmd[1] = "fetch";
    PickTagCmd[3] = symbol;
    char* pick = cmdread(PickTagCmd, NULL);
    if (pick) {
        Buf* buf = win_buf(EDIT);
        if (buf->path && 0 == strncmp(buf->path, pick, strlen(buf->path))) {
            view_setln(win_view(EDIT), strtoul(strrchr(pick, ':')+1, NULL, 0));
            win_setregion(EDIT);
        } else {
            if (!buf->path && !buf->modified) {
                view_init(win_view(EDIT), pick, onerror);
            } else {
                OpenCmd[1] = chomp(pick);
                cmdrun(OpenCmd, NULL);
            }
        }
    }
}

static void pick_ctag(void) {
    pick_symbol(NULL);
}

static void complete(void) {
    View* view = win_view(FOCUSED);
    buf_getword(&(view->buffer), risword, &(view->selection));
    view->selection.end = buf_byrune(&(view->buffer), view->selection.end, RIGHT);
    PickTagCmd[1] = "print";
    PickTagCmd[3] = view_getstr(view, NULL);
    char* pick = cmdread(PickTagCmd, NULL);
    if (pick)
        view_putstr(view, chomp(pick));
    free(PickTagCmd[3]);
}

static void jump_to(char* arg) {
    if (arg) {
        size_t line = strtoul(arg, NULL, 0);
        if (line) {
            view_setln(win_view(EDIT), line);
            win_setregion(EDIT);
        } else {
            pick_symbol(arg);
        }
    }
}

static void goto_ctag(void) {
    char* str = view_getctx(win_view(FOCUSED));
    jump_to(str);
    free(str);
}

static void tabs(void) {
    bool enabled = !(win_buf(EDIT)->expand_tabs);
    win_buf(EDIT)->expand_tabs = enabled;
    win_buf(TAGS)->expand_tabs = enabled;
}

static void indent(void) {
    bool enabled = !(win_buf(EDIT)->copy_indent);
    win_buf(EDIT)->copy_indent = enabled;
    win_buf(TAGS)->copy_indent = enabled;
}

static void del_indent(void) {
    view_indent(win_view(FOCUSED), LEFT);
}

static void add_indent(void) {
    view_indent(win_view(FOCUSED), RIGHT);
}

static void eol_mode(void) {
    int crlf = win_buf(EDIT)->crlf;
    win_buf(EDIT)->crlf = !crlf;
    win_buf(TAGS)->crlf = !crlf;
    exec(crlf ? "|dos2unix" : "|unix2dos");
}

static void new_win(void) {
    cmd_exec("!edit");
}

static void newline(void) {
    View* view = win_view(FOCUSED);
    if (x11_keymodsset(ModShift)) {
        view_byline(view, UP, false);
        view_bol(view, false);
        if (view->selection.end == 0) {
            view_insert(view, true, '\n');
            view->selection = (Sel){0,0,0};
            return;
        }
    }
    view_eol(view, false);
    view_insert(view, true, '\n');
}

void highlight(void) {
    view_selctx(win_view(FOCUSED));
}

/* Main Routine
 ******************************************************************************/
static Tag Builtins[] = {
    { .tag = "Quit",   .action.noarg = quit     },
    { .tag = "Save",   .action.noarg = save     },
    { .tag = "SaveAs", .action.arg   = saveas   },
    { .tag = "Cut",    .action.noarg = cut      },
    { .tag = "Copy",   .action.noarg = copy     },
    { .tag = "Paste",  .action.noarg = paste    },
    { .tag = "Undo",   .action.noarg = tag_undo },
    { .tag = "Redo",   .action.noarg = tag_redo },
    { .tag = "Find",   .action.arg   = find     },
    { .tag = "GoTo",   .action.arg   = jump_to  },
    { .tag = "Tabs",   .action.noarg = tabs     },
    { .tag = "Indent", .action.noarg = indent   },
    { .tag = "Eol",    .action.noarg = eol_mode },
    { .tag = NULL,     .action.noarg = NULL     }
};

static KeyBinding Bindings[] = {
    /* Cursor Movements */
    { ModAny, KEY_HOME,  cursor_home  },
    { ModAny, KEY_END,   cursor_end   },
    { ModAny, KEY_UP,    cursor_up    },
    { ModAny, KEY_DOWN,  cursor_dn    },
    { ModAny, KEY_LEFT,  cursor_left  },
    { ModAny, KEY_RIGHT, cursor_right },

    /* Standard Unix Shortcuts */
    { ModCtrl, 'u', del_to_bol  },
    { ModCtrl, 'k', del_to_eol  },
    { ModCtrl, 'w', del_to_bow  },
    { ModCtrl, 'a', cursor_bol  },
    { ModCtrl, 'e', cursor_eol  },

    /* Standard Text Editing Shortcuts */
    { ModCtrl, 's', save        },
    { ModCtrl, 'z', undo        },
    { ModCtrl, 'y', redo        },
    { ModCtrl, 'x', cut         },
    { ModCtrl, 'c', copy        },
    { ModCtrl, 'v', paste       },
    { ModCtrl, 'j', join_lines  },
    { ModCtrl, 'l', select_line },

    /* Block Indent */
    { ModCtrl, '[', del_indent },
    { ModCtrl, ']', add_indent },

    /* Common Special Keys */
    { ModNone, KEY_PGUP,      page_up   },
    { ModNone, KEY_PGDN,      page_dn   },
    { ModAny,  KEY_DELETE,    delete    },
    { ModAny,  KEY_BACKSPACE, backspace },

    /* Implementation Specific */
    { ModNone,                 KEY_ESCAPE, select_prev  },
    { ModCtrl,                 't',        change_focus },
    { ModCtrl,                 'q',        quit         },
    { ModCtrl,                 'h',        highlight    },
    { ModCtrl,                 'f',        search       },
    { ModCtrl|ModShift,        'f',        search       },
    { ModCtrl|ModAlt,          'f',        search       },
    { ModCtrl|ModAlt|ModShift, 'f',        search       },
    { ModCtrl,                 'd',        execute      },
    { ModCtrl,                 'o',        open_file    },
    { ModCtrl,                 'p',        pick_ctag    },
    { ModCtrl,                 'g',        goto_ctag    },
    { ModCtrl,                 'n',        new_win      },
    { ModCtrl,                 '\n',       newline      },
    { ModCtrl|ModShift,        '\n',       newline      },
    { ModCtrl,                 ' ',        complete     },
    { 0, 0, 0 }
};

void onscroll(double percent) {
    size_t bend = buf_end(win_buf(EDIT));
    size_t off  = (size_t)((double)bend * percent);
    view_scrollto(win_view(EDIT), (off >= bend ? bend : off));
}

void onupdate(void) {
    static char status_bytes[256];
    memset(status_bytes, 0, sizeof(status_bytes));
    char* status = status_bytes;
    Buf* buf = win_buf(EDIT);
    *(status++) = (buf->charset == BINARY ? 'B' : 'U');
    *(status++) = (buf->crlf ? 'C' : 'N');
    *(status++) = (buf->expand_tabs ? 'S' : 'T');
    *(status++) = (buf->copy_indent ? 'I' : 'i');
    *(status++) = (SearchDir < 0 ? '<' : '>');
    *(status++) = (buf->modified ? '*' : ' ');
    *(status++) = ' ';
    char* path = (buf->path ? buf->path : "*scratch*");
    size_t remlen = sizeof(status_bytes) - strlen(status_bytes) - 1;
    strncat(status, path, remlen);
    win_settext(STATUS, status_bytes);
    win_view(STATUS)->selection = (Sel){0,0,0};
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

void onshutdown(void) {
    quit();
}

#ifndef TEST
int main(int argc, char** argv) {
    /* setup the shell */
    ShellCmd[0] = getenv("SHELL");
    if (!ShellCmd[0]) ShellCmd[0] = "/bin/sh";
    /* Create the window and enter the event loop */
    win_window("edit", onerror);
    char* tags = getenv("EDITTAGS");
    win_settext(TAGS, (tags ? tags : DEFAULT_TAGS));
    win_setruler(80);
    view_init(win_view(EDIT), (argc > 1 ? argv[1] : NULL), onerror);
    win_setkeys(Bindings);
    win_loop();
    return 0;
}
#endif
