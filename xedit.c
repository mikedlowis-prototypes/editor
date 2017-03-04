#include <stdc.h>
#include <x11.h>
#include <utf.h>
#include <edit.h>
#include <ctype.h>
#include <win.h>

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
static char* PickTagCmd[] = { "xtagpick", "tags", NULL, NULL };
static char* OpenCmd[] = { "xedit", NULL, NULL };
static Tag Builtins[];
static int SearchDir = DOWN;
static char* SearchTerm = NULL;

/* Tag/Cmd Execution
 *****************************************************************************/
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
 *****************************************************************************/
static void delete(void) {
    bool byword = x11_keymodsset(ModCtrl);
    view_delete(win_view(FOCUSED), RIGHT, byword);
}

static void onpaste(char* text) {
    view_putstr(win_view(FOCUSED), text);
}

static void cut(void) {
    char* str = view_getstr(win_view(FOCUSED), NULL);
    x11_setsel(CLIPBOARD, str);
    if (str && *str) delete();
}

static void paste(void) {
    assert(x11_getsel(CLIPBOARD, onpaste));
}

static void copy(void) {
    char* str = view_getstr(win_view(FOCUSED), NULL);
    x11_setsel(CLIPBOARD, str);
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
    buf_save(win_buf(EDIT));
}

/* Mouse Handling
 *****************************************************************************/
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
        char* str = view_fetchcmd(win_view(id), row, col);
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
        view_find(win_view(id), SearchDir, row, col);
        SearchTerm = view_getstr(win_view(id), NULL);
        win_warpptr(id);
    }
}

/* Keyboard Handling
 *****************************************************************************/
static void del_to_bol(void) {
    view_bol(win_view(FOCUSED), true);
    if (view_selsize(win_view(FOCUSED)) > 0) 
        delete();
}

static void del_to_eol(void) {
    view_eol(win_view(FOCUSED), true);
    if (view_selsize(win_view(FOCUSED)) > 0) 
        delete();
}

static void del_to_bow(void) {
    view_byword(win_view(FOCUSED), LEFT, true);
    if (view_selsize(win_view(FOCUSED)) > 0) 
        delete();
}

static void backspace(void) {
    bool byword = x11_keymodsset(ModCtrl);
    view_delete(win_view(FOCUSED), LEFT, byword);
}

static void cursor_bol(void) {
    view_bol(win_view(FOCUSED), false);
}

static void cursor_eol(void) {
    view_eol(win_view(FOCUSED), false);
}

static void cursor_home(void) {
    bool extsel = x11_keymodsset(ModShift);
    if (x11_keymodsset(ModCtrl))
        view_bof(win_view(FOCUSED), extsel);
    else
        view_bol(win_view(FOCUSED), extsel);
}

static void cursor_end(void) {
    bool extsel = x11_keymodsset(ModShift);
    if (x11_keymodsset(ModCtrl))
        view_eof(win_view(FOCUSED), extsel);
    else
        view_eol(win_view(FOCUSED), extsel);
}

static void cursor_up(void) {
    bool extsel = x11_keymodsset(ModShift);
    view_byline(win_view(FOCUSED), UP, extsel);
}

static void cursor_dn(void) {
    bool extsel = x11_keymodsset(ModShift);
    view_byline(win_view(FOCUSED), DOWN, extsel);
}

static void cursor_left(void) {
    bool extsel = x11_keymodsset(ModShift);
    if (x11_keymodsset(ModCtrl))
        view_byword(win_view(FOCUSED), LEFT, extsel);
    else
        view_byrune(win_view(FOCUSED), LEFT, extsel);
}

static void cursor_right(void) {
    bool extsel = x11_keymodsset(ModShift);
    if (x11_keymodsset(ModCtrl))
        view_byword(win_view(FOCUSED), RIGHT, extsel);
    else
        view_byrune(win_view(FOCUSED), RIGHT, extsel);
}

static void page_up(void) {
    view_scrollpage(win_view(FOCUSED), UP);
}

static void page_dn(void) {
    view_scrollpage(win_view(FOCUSED), DOWN);
}

static void select_prev(void) {
    view_selprev(win_view(FOCUSED));
}

static void change_focus(void) {
    win_setregion(win_getregion() == TAGS ? EDIT : TAGS);
}

static void undo(void) {
    view_undo(win_view(FOCUSED));
}

static void redo(void) {
    view_redo(win_view(FOCUSED));
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
    win_setregion(EDIT);
    win_warpptr(EDIT);
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
    PickTagCmd[2] = symbol;
    char* pick = cmdread(PickTagCmd, NULL);
    if (pick) {
        Buf* buf = win_buf(EDIT);
        if (buf->path && 0 == strncmp(buf->path, pick, strlen(buf->path))) {
            view_setln(win_view(EDIT), strtoul(strrchr(pick, ':')+1, NULL, 0));
            win_setregion(EDIT);
        } else {
            if (!buf->path && !buf->modified) {
                view_init(win_view(EDIT), pick);
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

static void goto_ctag(void) {
    char* str = view_getctx(win_view(FOCUSED));
    if (str) {
        size_t line = strtoul(str, NULL, 0);
        if (line) {
            view_setln(win_view(EDIT), line);
            win_setregion(EDIT);
        } else {
            pick_symbol(str);
        }
    }
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

/* Main Routine
 *****************************************************************************/
static Tag Builtins[] = {
    { .tag = "Quit",   .action.noarg = quit     },
    { .tag = "Save",   .action.noarg = save     },
    { .tag = "Cut",    .action.noarg = cut      },
    { .tag = "Copy",   .action.noarg = copy     },
    { .tag = "Paste",  .action.noarg = paste    },
    { .tag = "Undo",   .action.noarg = tag_undo },
    { .tag = "Redo",   .action.noarg = tag_redo },
    { .tag = "Find",   .action.arg   = find     },
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
    { ModCtrl, 'h', backspace   },
    { ModCtrl, 'a', cursor_bol  },
    { ModCtrl, 'e', cursor_eol  },

    /* Standard Text Editing Shortcuts */
    { ModCtrl, 's', save  },
    { ModCtrl, 'z', undo  },
    { ModCtrl, 'y', redo  },
    { ModCtrl, 'x', cut   },
    { ModCtrl, 'c', copy  },
    { ModCtrl, 'v', paste },
    
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
    { 0, 0, 0 }
};

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
}

#ifndef TEST
int main(int argc, char** argv) {
    /* setup the shell */
    ShellCmd[0] = getenv("SHELL");
    if (!ShellCmd[0]) ShellCmd[0] = "/bin/sh";
    /* Create the window and enter the event loop */
    win_init("edit");
    char* tags = getenv("EDITTAGS");
    win_settext(TAGS, (tags ? tags : DEFAULT_TAGS));
    view_init(win_view(EDIT), (argc > 1 ? argv[1] : NULL));
    win_setkeys(Bindings);
    //win_setmouse(&MouseHandlers);
    win_loop();
    return 0;
}
#endif

