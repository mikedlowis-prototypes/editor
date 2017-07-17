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

typedef struct {
    char* tag;
    union {
        void (*noarg)(void);
        void (*arg)(char* arg);
    } action;
} Tag;

static int CmdFD = -1;
static Tag Builtins[];
static int SearchDir = DOWN;
static char* SearchTerm = NULL;
static size_t Marks[10] = {0};

/* The shell: Filled in with $SHELL. Used to execute commands */
char* ShellCmd[] = { NULL, "-c", NULL, NULL };

/* Sed command used to execute commands marked with ':' sigil */
char* SedCmd[] = { "sed", "-e", NULL, NULL };

/* Fuzzy Picker for files in the current directory and subdirectories */
char* PickFileCmd[] = { "pickfile", ".", NULL };

/* Fuzzy picker for tags in a ctags database. */
char* PickTagCmd[] = { "picktag", NULL, "tags", NULL, NULL };

/* Open a new instance of the editor */
char* OpenCmd[] = { "tide", NULL, NULL };

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
    /* parse the command sigils */
    char op = '\0', **execcmd = NULL;
    if (*cmd == ':' || *cmd == '!' || *cmd == '<' || *cmd == '|' || *cmd == '>')
        op = *cmd, cmd++;
    execcmd = (op == ':' ? SedCmd : ShellCmd);
    execcmd[2] = cmd;

    /* get the selection that the command will operate on */
    if (op && op != '<' && op != '!' && 0 == view_selsize(win_view(EDIT)))
        win_view(EDIT)->selection = (Sel){ .beg = 0, .end = buf_end(win_buf(EDIT)) };
    char* input = view_getstr(win_view(EDIT), NULL);
    size_t len  = (input ? strlen(input) : 0);
    View *tags = win_view(TAGS), *edit = win_view(EDIT), *curr = win_view(FOCUSED);

    /* execute the job */
    if (op == '!')
        free(input), exec_job(execcmd, NULL, 0, NULL);
    else if (op == '>')
        exec_job(execcmd, input, len, tags);
    else if (op == '|' || op == ':')
        exec_job(execcmd, input, len, edit);
    else
        exec_job(execcmd, input, len, (op != '<' ? curr : edit));
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
static void ondiagmsg(char* msg) {
    view_append(win_view(TAGS), msg);
    win_setregion(TAGS);
}

static void trim_whitespace(void) {
    Buf* buf = win_buf(EDIT);
    if (config_get_bool(TrimOnSave) && buf_end(buf) > 0) {
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
}

static void quit(void) {
    static uint64_t before = 0;
    uint64_t now = getmillis();
    if (!win_buf(EDIT)->modified || (now-before) <= (uint64_t)config_get_int(DblClickTime)) {
        #ifndef TEST
        x11_deinit();
        #else
        exit(0);
        #endif
    } else {
        ondiagmsg("File is modified. Repeat action twice quickly to quit.");
    }
    before = now;
}

static bool changed_externally(Buf* buf) {
    if (!buf->path) return false;
    bool modified = (buf->modtime != modtime(buf->path));
    if (modified)
        ondiagmsg("File modified externally: {SaveAs } Overwrite Reload");
    return modified;
}

static void overwrite(void) {
    trim_whitespace();
    buf_save(win_buf(EDIT));
}

static void save(void) {
    if (!changed_externally(win_buf(EDIT)))
        overwrite();
}

static void reload(void) {
    view_reload(win_view(EDIT));
}

/* Mouse Handling
 ******************************************************************************/
void onmouseleft(WinRegion id, bool pressed, size_t row, size_t col) {
    static int count = 0;
    static uint64_t before = 0;
    if (!pressed) return;
    uint64_t now = getmillis();
    count = ((now-before) <= (uint64_t)config_get_int(DblClickTime) ? count+1 : 1);
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
        if (str) exec(str);
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

static void tag_lnnum(void) {
    win_setlinenums(!win_getlinenums());
}

static void search(void) {
    char* str;
    SearchDir *= (x11_keymodsset(ModShift) ? UP : DOWN);
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
    SearchDir *= (x11_keymodsset(ModShift) ? UP : DOWN);
    view_findstr(win_view(EDIT), SearchDir, arg);
}

static void open_file(void) {
    char* file = NULL;
    exec_cmd(PickFileCmd, NULL, &file, NULL);
    if (file) {
        file = chomp(file);
        if ((!win_buf(EDIT)->path || x11_keymodsset(ModShift)) &&
            !win_buf(EDIT)->modified) {
            view_init(win_view(EDIT), file, ondiagmsg);
        } else {
            OpenCmd[1] = file;
            exec_job(OpenCmd,0,0,0);
        }
    }
    free(file);
}

static void pick_symbol(char* symbol) {
    PickTagCmd[1] = "fetch";
    PickTagCmd[3] = symbol;
    char* pick = NULL;
    exec_cmd(PickTagCmd, NULL, &pick, NULL);
    if (pick) {
        Buf* buf = win_buf(EDIT);
        if (buf->path && 0 == strncmp(buf->path, pick, strlen(buf->path))) {
            view_setln(win_view(EDIT), strtoul(strrchr(pick, ':')+1, NULL, 0));
            win_setregion(EDIT);
        } else {
            if (!buf->path && !buf->modified) {
                view_init(win_view(EDIT), pick, ondiagmsg);
            } else {
                OpenCmd[1] = chomp(pick);
                exec_job(OpenCmd,0,0,0);

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
    char* pick = NULL;
    exec_cmd(PickTagCmd, NULL, &pick, NULL);
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
    if (x11_keymodsset(ModShift)) {
        view_jumpback(win_view(FOCUSED));
    } else {
        char* str = view_getctx(win_view(FOCUSED));
        jump_to(str);
        free(str);
    }
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
    cmd_exec("!tide");
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

static void highlight(void) {
    view_selctx(win_view(FOCUSED));
}

static void jumpmark(void) {
    int mark = (win_getkey() - '0');
    assert(mark < 10);
    if (x11_keymodsset(ModAlt))
        Marks[mark] = win_view(FOCUSED)->selection.end;
    else
        view_jumpto(win_view(FOCUSED), false, Marks[mark]);
}

/* Main Routine
 ******************************************************************************/
static Tag Builtins[] = {
    { .tag = "Cut",       .action.noarg = cut       },
    { .tag = "Copy",      .action.noarg = copy      },
    { .tag = "Eol",       .action.noarg = eol_mode  },
    { .tag = "Find",      .action.arg   = find      },
    { .tag = "GoTo",      .action.arg   = jump_to   },
    { .tag = "Indent",    .action.noarg = indent    },
    { .tag = "Overwrite", .action.noarg = overwrite },
    { .tag = "Paste",     .action.noarg = paste     },
    { .tag = "Quit",      .action.noarg = quit      },
    { .tag = "Redo",      .action.noarg = tag_redo  },
    { .tag = "Reload",    .action.noarg = reload    },
    { .tag = "Save",      .action.noarg = save      },
    { .tag = "SaveAs",    .action.arg   = saveas    },
    { .tag = "Tabs",      .action.noarg = tabs      },
    { .tag = "Undo",      .action.noarg = tag_undo  },
    { .tag = "LineNums",  .action.noarg = tag_lnnum },
    { .tag = NULL,        .action.noarg = NULL      }
};

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
    { ModCtrl,          's', save        },
    { ModCtrl,          'z', undo        },
    { ModCtrl,          'y', redo        },
    { ModCtrl,          'x', cut         },
    { ModCtrl,          'c', copy        },
    { ModCtrl,          'v', paste       },
    { ModCtrl,          'j', join_lines  },
    { ModCtrl,          'l', select_line },
    { ModCtrl|ModShift, 'a', select_all  },

    /* Block Indent */
    { ModCtrl, '[', del_indent },
    { ModCtrl, ']', add_indent },

    /* Common Special Keys */
    { ModNone, KEY_PGUP,      page_up   },
    { ModNone, KEY_PGDN,      page_dn   },
    { ModAny,  KEY_DELETE,    delete    },
    { ModAny,  KEY_BACKSPACE, backspace },

    /* Marks Handling */
    { ModOneOrMore, '0', jumpmark },
    { ModOneOrMore, '1', jumpmark },
    { ModOneOrMore, '2', jumpmark },
    { ModOneOrMore, '3', jumpmark },
    { ModOneOrMore, '4', jumpmark },
    { ModOneOrMore, '5', jumpmark },
    { ModOneOrMore, '6', jumpmark },
    { ModOneOrMore, '7', jumpmark },
    { ModOneOrMore, '8', jumpmark },
    { ModOneOrMore, '9', jumpmark },

    /* Implementation Specific */
    { ModNone,      KEY_ESCAPE, select_prev  },
    { ModCtrl,      't',        change_focus },
    { ModCtrl,      'q',        quit         },
    { ModCtrl,      'h',        highlight    },
    { ModOneOrMore, 'f',        search       },
    { ModCtrl,      'd',        execute      },
    { ModOneOrMore, 'o',        open_file    },
    { ModCtrl,      'p',        pick_ctag    },
    { ModOneOrMore, 'g',        goto_ctag    },
    { ModCtrl,      'n',        new_win      },
    { ModOneOrMore, '\n',       newline      },
    { ModCtrl,      ' ',        complete     },
    { 0, 0, 0 }
};

void onscroll(double percent) {
    size_t bend = buf_end(win_buf(EDIT));
    size_t off  = (size_t)((double)bend * percent);
    view_scrollto(win_view(EDIT), (off >= bend ? bend : off));
}

void onfocus(bool focused) {
    /* notify the user if the file has changed externally */
    if (focused)
        (void)changed_externally(win_buf(EDIT));
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

bool update_needed(void) {
    return false;
}

static void oninput(Rune rune) {
    view_insert(win_view(FOCUSED), (CmdFD == -1), rune);
    if (win_getregion() == EDIT) {
        size_t point = win_buf(EDIT)->outpoint;
        size_t pos   = win_view(EDIT)->selection.end;
        if ((rune == '\n' || rune == RUNE_CRLF) && pos > point) {
            Sel range = { .beg = point, .end = pos };
            char* str = view_getstr(win_view(EDIT), &range);
            if (write(CmdFD, str, strlen(str)-1) < 0)
                CmdFD = -1;
            free(str);
        }
    }
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

void pty_update(int fd, void* data) {
    /* Read from command if we have one */
    long n = 0, i = 0;
    static char cmdbuf[8192];
    if ((n = read(CmdFD, cmdbuf, sizeof(cmdbuf))) < 0)
        CmdFD = -1;
    while (i < n) {
        Rune rune = 0;
        size_t length = 0;
        while (!utf8decode(&rune, &length, cmdbuf[i++]));
        view_insert(win_view(EDIT), false, rune);
    }
    win_buf(EDIT)->outpoint = win_view(EDIT)->selection.end;
}

void edit_relative(char* path) {
    char *currdir = NULL, *currpath = NULL, *relpath = NULL;
    char* origdir = getcurrdir();

    /* search for a ctags index file indicating the project root */
    if (try_chdir(path)) {
        currdir   = getcurrdir();
        size_t sz = strlen(currdir) + strlen(path) + strlen("/tags") + 1;
        currpath  = calloc(sz, 1);
        relpath   = calloc(sz, 1);
        while (true) {
            /* figure out the current path to check */
            strconcat(currpath, currdir, "/tags", 0);
            if (file_exists(currpath)) {
                /* move up a dir */
                char* end = strrchr(currdir,'/');
                if (!end) break;
                char* temp = stringdup(relpath);
                strconcat(relpath, end, temp, 0);
                free(temp);
                *end = '\0';
            } else {
                break;
            }
        }
    }

    /* cd to the project directory or the original working directory and open
       the file relative to the new working directory */
    if (currdir && *currdir) {
        char* fname = strrchr(path, '/')+1;
        if (*relpath)
            strconcat(currpath, (*relpath == '/' ? relpath+1 : relpath), "/", fname, 0);
        else
            strconcat(currpath, fname, 0);
        chdir(currdir);
        view_init(win_view(EDIT), currpath, ondiagmsg);
    } else {
        chdir(origdir);
        view_init(win_view(EDIT), path, ondiagmsg);
    }

    /* cleanup */
    free(currdir);
    free(currpath);
    free(relpath);
    free(origdir);
}

void edit_command(char** cmd) {
    char* shellcmd[] = { ShellCmd[0], NULL };
    win_buf(EDIT)->crlf = 1;
    config_set_int(TabWidth, 8);
    win_setlinenums(false);
    win_setruler(0);
    CmdFD = pty_spawn(*cmd ? cmd : shellcmd);
    event_watchfd(CmdFD, INPUT, pty_update, NULL);
}

#ifndef TEST
int main(int argc, char** argv) {
    /* setup the shell */
    if (!ShellCmd[0]) ShellCmd[0] = getenv("SHELL");
    if (!ShellCmd[0]) ShellCmd[0] = "/bin/sh";

    /* Create the window and enter the event loop */
    win_window("tide", ondiagmsg);
    char* tags = getenv("EDITTAGS");
    win_settext(TAGS, (tags ? tags : config_get_str(TagString)));
    win_setruler(config_get_int(RulerColumn));
    win_setlinenums(config_get_bool(LineNumbers));

    /* open all but the last file in new instances */
    for (argc--, argv++; argc > 1; argc--, argv++) {
        if (!strcmp(*argv, "--")) break;
        OpenCmd[1] = *argv;
        exec_job(OpenCmd,0,0,0);
    }

    /* if we still have args left we're going to open it in this instance */
    if (*argv) {
        /* if it's a command we treat that specially */
        if (!strcmp(*argv, "--"))
            edit_command(argv+1);
        else
            edit_relative(*argv);
    }

    /* now create the window and start the event loop */
    win_setkeys(Bindings, oninput);
    win_loop();
    return 0;
}
#endif
