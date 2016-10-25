#include "edit.h"

static void toggle_colors(void) {
    ColorBase = !ColorBase;
}

static void cursor_up(void) {
    DotBeg = DotEnd = buf_byline(&Buffer, DotEnd, -1);
    DotBeg = DotEnd = buf_setcol(&Buffer, DotEnd, TargetCol);
}

static void cursor_dn(void) {
    DotBeg = DotEnd = buf_byline(&Buffer, DotEnd, 1);
    DotBeg = DotEnd = buf_setcol(&Buffer, DotEnd, TargetCol);
}

static void cursor_left(void) {
    DotBeg = DotEnd = buf_byrune(&Buffer, DotEnd, -1);
    TargetCol = buf_getcol(&Buffer, DotEnd);
}

static void cursor_right(void) {
    DotBeg = DotEnd = buf_byrune(&Buffer, DotEnd, 1);
    TargetCol = buf_getcol(&Buffer, DotEnd);
}

static void cursor_bol(void) {
    DotBeg = DotEnd = buf_bol(&Buffer, DotEnd);
    TargetCol = 0;
}

static void cursor_eol(void) {
    DotBeg = DotEnd = buf_eol(&Buffer, DotEnd);
    TargetCol = (unsigned)-1;
}

static void insert_before(void) {
    DotEnd = DotBeg;
    Buffer.insert_mode = true;
}

static void insert_after(void) {
    DotBeg = ++DotEnd;
    Buffer.insert_mode = true;
}

static void exit_insert(void) {
    Buffer.insert_mode = false;
}

static void write(void) {
    buf_save(&Buffer);
}

static void quit(void) {
    static uint32_t num_clicks = 0;
    static uint32_t prevtime = 0;
    uint32_t now = getmillis();
    num_clicks = (now - prevtime < 250 ? num_clicks+1 : 1);
    prevtime = now;
    if (!Buffer.modified || num_clicks >= 2)
        exit(0);
}

static void dot_delete(void) {
    if (DotEnd == buf_end(&Buffer)) return;
    size_t n = DotEnd - DotBeg;
    bool insert = Buffer.insert_mode;
    if (!insert || !n) n++;
    Buffer.insert_mode = true;
    for (size_t i = 0; i < n; i++)
        buf_del(&Buffer, DotBeg);
    DotEnd = DotBeg;
    TargetCol = buf_getcol(&Buffer, DotEnd);
    Buffer.insert_mode = insert;
}

static void dot_change(void) {
    dot_delete();
    Buffer.insert_mode = true;
}

static void dot_backspace(void) {
    if (DotBeg > 0 && DotBeg == DotEnd) DotBeg--;
    while (DotBeg < DotEnd)
        buf_del(&Buffer, --DotEnd);
    TargetCol = buf_getcol(&Buffer, DotEnd);
}

static void insert(Rune r) {
    if (!Buffer.insert_mode) return;
    buf_ins(&Buffer, DotEnd++, r);
    DotBeg = DotEnd;
    TargetCol = buf_getcol(&Buffer, DotEnd);
}

/*****************************************************************************/

static void cursor_nextword(void) {
}

static void cursor_nextbigword(void) {
}

static void cursor_endword(void) {
}

static void cursor_endbigword(void) {
}

static void cursor_prevword(void) {
}

static void cursor_prevbigword(void) {
}

/*****************************************************************************/

typedef struct {
    Rune key;
    void (*action)(void);
} KeyBinding_T;

static KeyBinding_T Normal[] = {
    { KEY_F6,     toggle_colors },
    { KEY_CTRL_Q, quit          },
    { KEY_CTRL_W, write         },
    //{ 'q',        quit          },
    //{ 's',        write         },

    /* visual selection modes */
    //{ 'v',        visual        },
    //{ 'V',        visual_line   },
    //{ KEY_CTRL_V, visual_column },

    /* normal cursor movements */
    { KEY_UP,     cursor_up     },
    { 'k',        cursor_up     },
    { KEY_DOWN,   cursor_dn     },
    { 'j',        cursor_dn     },
    { KEY_LEFT,   cursor_left   },
    { 'h',        cursor_left   },
    { KEY_RIGHT,  cursor_right  },
    { 'l',        cursor_right  },
    { KEY_HOME,   cursor_bol    },
    { '0',        cursor_bol    },
    { KEY_END,    cursor_eol    },
    { '$',        cursor_eol    },

    /* advanced cursor movements */
    { 'w',        cursor_nextword    },
    { 'W',        cursor_nextbigword },
    { 'e',        cursor_endword     },
    { 'E',        cursor_endbigword  },
    { 'b',        cursor_prevword    },
    { 'B',        cursor_prevbigword },

    /* undo/redo handling */
    //{ 'u',        undo },
    //{ 'r',        redo },

    /* insert mode handling */
    { 'a',        insert_after    },
    //{ 'A',        insert_afterln  },
    { 'i',        insert_before   },
    //{ 'I',        insert_beforeln },
    { 'd',        dot_delete      },
    { 'c',        dot_change      },
    //{ 'o',        insert_lnafter  },
    //{ 'O',        insert_lnbefore },
    { KEY_DELETE, dot_delete    },

    /* Copy/Paste */
    //{ 'y',        yank_selection },
    //{ 'Y',        yank_line      },
    //{ 'p',        paste_after    },
    //{ 'P',        paste_before   },

    /* context sensitive language */
    //{ 's', dot_select  },
    //{ 'x', dot_execute },
    //{ 'f', dot_find    },

    { 0,          NULL          }
};

static KeyBinding_T Insert[] = {
    { KEY_F6,        toggle_colors },
    { KEY_UP,        cursor_up     },
    { KEY_DOWN,      cursor_dn     },
    { KEY_LEFT,      cursor_left   },
    { KEY_RIGHT,     cursor_right  },
    { KEY_HOME,      cursor_bol    },
    { KEY_END,       cursor_eol    },
    { KEY_ESCAPE,    exit_insert   },
    { KEY_DELETE,    dot_delete    },
    { KEY_BACKSPACE, dot_backspace },
    { 0,             NULL          }
};

static void process_table(KeyBinding_T* bindings, Rune key) {
    while (bindings->key) {
        if (key == bindings->key) {
            bindings->action();
            return;
        }
        bindings++;
    }
    insert(key);
}

void handle_key(Rune key) {
    /* ignore invalid keys */
    if (key == RUNE_ERR) return;
    /* handle the proper line endings */
    if (key == '\r') key = '\n';
    if (key == '\n' && Buffer.crlf) key = RUNE_CRLF;
    /* handle the key */
    if (Buffer.insert_mode)
        process_table(Insert, key);
    else
        process_table(Normal, key);
}
