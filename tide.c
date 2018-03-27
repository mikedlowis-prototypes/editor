#include <stdc.h>
#include <x11.h>
#include <utf.h>
#include <edit.h>
#include <win.h>
#include <ctype.h>
#include <unistd.h>

typedef struct {
    char* tag;
    union {
        void (*noarg)(void);
        void (*arg)(char* arg);
    } action;
} Tag;

static Tag Builtins[];
static int SearchDir = DOWN;
static char* SearchTerm = NULL;
static size_t Marks[10] = {0};

/* The shell: Filled in with $SHELL. Used to execute commands */
char* ShellCmd[] = { NULL, "-c", NULL, NULL };

/* Sed command used to execute commands marked with ':' sigil */
char* SedCmd[] = { "sed", "-e", NULL, NULL };

/* Try to fetch the text with tide-fetch */
char* FetchCmd[] = { "tfetch", NULL, NULL };

#define CMD_TIDE     "!tide"
#define CMD_PICKFILE "!pickfile ."
#define CMD_TO_DOS   "|unix2dos"
#define CMD_TO_UNIX  "|dos2unix"
#define CMD_COMPLETE "<picktag print tags"
#define CMD_GOTO_TAG "!picktag fetch tags"
#define CMD_FETCH    "tfetch"

/*
 ******************************************************************************/
static void select_line(void) {
    View* view = win_view(FOCUSED);
    view_eol(view, false);
    view_selctx(view);
}

static void select_all(void) {
    View* view = win_view(FOCUSED);
    view_jumpto(view, false, buf_end(&(view->buffer)));
    view->selection.beg = 0;
}

static void join_lines(void) {
    View* view = win_view(FOCUSED);
    view_eol(view, false);
    view_delete(view, RIGHT, false);
    Rune r = view_getrune(view);
    for (; r == '\t' || r == ' '; r = view_getrune(view))
        view_byrune(view, RIGHT, true);
    if (r != '\n')
        view_insert(view, false, ' ');
}

static void delete(void) {
    bool byword = x11_keymodsset(ModCtrl);
    view_delete(win_view(FOCUSED), RIGHT, byword);
}

static void onpaste(char* text) {
    view_putstr(win_view(FOCUSED), text);
}

static void cut(void) {
    View* view = win_view(FOCUSED);
    /* select the current line if no selection */
    if (!view_selsize(view))
        select_line();
    /* now perform the cut */
    char* str = view_getstr(view, NULL);
    x11_sel_set(CLIPBOARD, str);
    if (str && *str) {
        delete();
        if (view->selection.end == buf_end(&(view->buffer)))
            view_delete(view, LEFT, false);
    }
}

static void paste(void) {
    assert(x11_sel_get(CLIPBOARD, onpaste));
}

static void copy(void) {
    /* select the current line if no selection */
    if (!view_selsize(win_view(FOCUSED)))
        select_line();
    char* str = view_getstr(win_view(FOCUSED), NULL);
    x11_sel_set(CLIPBOARD, str);
}

static void del_to(void (*tofn)(View*, bool)) {
    tofn(win_view(FOCUSED), true);
    if (view_selsize(win_view(FOCUSED)) > 0)
        delete();
}

static void del_to_bol(void) {
    del_to(view_bol);
}

static void del_to_eol(void) {
    del_to(view_eol);
}

static void del_to_bow(void) {
    view_byword(win_view(FOCUSED), LEFT, true);
    if (view_selsize(win_view(FOCUSED)) > 0)
        delete();
}

static void backspace(void) {
    view_delete(win_view(FOCUSED), LEFT, x11_keymodsset(ModCtrl));
}

static void cursor_bol(void) {
    view_bol(win_view(FOCUSED), false);
}

static void cursor_eol(void) {
    view_eol(win_view(FOCUSED), false);
}

static void move_line_up(void) {
    if (!view_selsize(win_view(FOCUSED)))
        select_line();
    cut();
    view_byline(win_view(FOCUSED), UP, false);
    paste();
}

static void move_line_dn(void) {
    if (!view_selsize(win_view(FOCUSED)))
        select_line();
    cut();
    cursor_eol();
    view_byrune(win_view(FOCUSED), RIGHT, false);
    paste();
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
    if (x11_keymodsset(ModCtrl))
        move_line_up();
    else
        view_byline(win_view(FOCUSED), UP, extsel);
}

static void cursor_dn(void) {
    bool extsel = x11_keymodsset(ModShift);
    if (x11_keymodsset(ModCtrl))
        move_line_dn();
    else
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

static void cursor_warp(void) {
    view_csrsummon(win_view(FOCUSED));
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
        free(input), job_start(execcmd, NULL, 0, NULL);
    else if (op == '>')
        job_start(execcmd, input, len, tags);
    else if (op == '|' || op == ':')
        job_start(execcmd, input, len, edit);
    else
        job_start(execcmd, input, len, (op != '<' ? curr : edit));
}

static void cmd_execwitharg(char* cmd, char* arg) {
    cmd = (arg ? strmcat(cmd, " '", arg, "'", 0) : strmcat(cmd));
    cmd_exec(cmd);
    free(cmd);
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
}

static void quit(void) {
    static uint64_t before = 0;
    uint64_t now = getmillis();
    if (!win_buf(EDIT)->modified || (now-before) <= (uint64_t)ClickTime) {
        exit(0);
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
    win_save(NULL);
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
    count = ((now-before) <= (uint64_t)ClickTime ? count+1 : 1);
    before = now;

    if (count == 1) {
        view_setcursor(win_view(id), row, col, x11_keymodsset(ModShift));
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
        char* str = view_fetch(win_view(id), row, col, riscmd);
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
        SearchTerm = view_fetch(win_view(id), row, col, risfile);
    }
}

/* Keyboard Handling
 ******************************************************************************/
static void saveas(char* arg) {
    win_save(arg);
}

static void tag_undo(void) {
    view_undo(win_view(EDIT));
}

static void tag_redo(void) {
    view_redo(win_view(EDIT));
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
    cmd_exec(CMD_PICKFILE);
}

static void pick_symbol(char* symbol) {
    cmd_execwitharg(CMD_GOTO_TAG, symbol);
}

static void pick_ctag(void) {
    pick_symbol(NULL);
}

static void complete(void) {
    View* view = win_view(FOCUSED);
    buf_getword(&(view->buffer), risword, &(view->selection));
    view->selection.end = buf_byrune(&(view->buffer), view->selection.end, RIGHT);
    cmd_execwitharg(CMD_COMPLETE, view_getstr(view, NULL));
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
    ExpandTabs = !ExpandTabs;
}

static void indent(void) {
    CopyIndent = !CopyIndent;
}

static void del_indent(void) {
    view_indent(win_view(FOCUSED), LEFT);
}

static void add_indent(void) {
    view_indent(win_view(FOCUSED), RIGHT);
}

static void eol_mode(void) {
    DosLineFeed = !DosLineFeed;
    cmd_exec(DosLineFeed ? CMD_TO_DOS : CMD_TO_UNIX);
}

static void new_win(void) {
    cmd_exec(CMD_TIDE);
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
    exit(0);
}

static void oninput(Rune rune) {
    view_insert(win_view(FOCUSED), true, rune);
}

#ifndef TEST
int main(int argc, char** argv) {
    /* setup the shell */
    if (!ShellCmd[0]) ShellCmd[0] = getenv("SHELL");
    if (!ShellCmd[0]) ShellCmd[0] = "/bin/sh";

    /* create the window */
    win_init(ondiagmsg);
    x11_window("tide", WinWidth, WinHeight);

    /* open all but the last file in new instances */
    for (argc--, argv++; argc > 1; argc--, argv++)
        cmd_execwitharg(CMD_TIDE, *argv);

    /* if we still have args left we're going to open it in this instance */
    if (*argv) view_init(win_view(EDIT), *argv, ondiagmsg);

    /* now create the window and start the event loop */
    win_settext(TAGS, TagString);
    win_setkeys(Bindings, oninput);
    win_loop();
    return 0;
}
#endif
