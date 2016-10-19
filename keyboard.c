#include "edit.h"

static void toggle_mode(void) {
    Buffer.insert_mode = !Buffer.insert_mode;
}

static void toggle_colors(void) {
    ColorBase = !ColorBase;
}

static void cursor_up(void) {
    CursorPos = buf_byline(&Buffer, CursorPos, -1);
    CursorPos = buf_setcol(&Buffer, CursorPos, TargetCol);
}

static void cursor_dn(void) {
    CursorPos = buf_byline(&Buffer, CursorPos, 1);
    CursorPos = buf_setcol(&Buffer, CursorPos, TargetCol);
}

static void cursor_left(void) {
    CursorPos = buf_byrune(&Buffer, CursorPos, -1);
    TargetCol = buf_getcol(&Buffer, CursorPos);
}

static void cursor_right(void) {
    CursorPos = buf_byrune(&Buffer, CursorPos, 1);
    TargetCol = buf_getcol(&Buffer, CursorPos);
}

static void cursor_bol(void) {
    CursorPos = buf_bol(&Buffer, CursorPos);
    TargetCol = 0;
}

static void cursor_eol(void) {
    CursorPos = buf_eol(&Buffer, CursorPos);
    TargetCol = (unsigned)-1;
}

static void backspace(void) {
    if (CursorPos == 0) return;
    buf_del(&Buffer, --CursorPos);
    TargetCol = buf_getcol(&Buffer, CursorPos);
}

static void insert(Rune r) {
    buf_ins(&Buffer, CursorPos++, r);
}

static void delete(void) {
    buf_del(&Buffer, CursorPos);
}

static void insert_before(void) {
    Buffer.insert_mode = true;
}

static void insert_after(void) {
    CursorPos++;
    Buffer.insert_mode = true;
}

static void exit_insert(void) {
    Buffer.insert_mode = false;
}

static void write(void) {
    buf_save(&Buffer);
}

static void quit(void) {
    exit(0);
}

/*****************************************************************************/

typedef struct {
    Rune key;
    void (*action)(void);
} KeyBinding_T;

KeyBinding_T Specials[] = {
    { KEY_F6,     toggle_colors },
    { KEY_UP,     cursor_up     },
    { KEY_DOWN,   cursor_dn     },
    { KEY_LEFT,   cursor_left   },
    { KEY_RIGHT,  cursor_right  },
    { KEY_INSERT, toggle_mode   },
    { KEY_F1,     toggle_mode   },
    { KEY_DELETE, delete        },
    { KEY_HOME,   cursor_bol    },
    { KEY_END,    cursor_eol    },
    { 0,          NULL          }
};

KeyBinding_T Controls[] = {
    { KEY_ESCAPE,    exit_insert },
    { KEY_BACKSPACE, backspace   },
    { KEY_CTRL_W,    write       },
    { KEY_CTRL_Q,    quit        },
    { 0,             NULL        }
};

KeyBinding_T ViKeys[] = {
    { 'a', insert_after  },
    { 'i', insert_before },
    { 'k', cursor_up     },
    { 'j', cursor_dn     },
    { 'h', cursor_left   },
    { 'l', cursor_right  },
    { '0', cursor_bol    },
    { '$', cursor_eol    },
    { 0,   NULL          }
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
    /* handle the special keys */
    if (0xE000 <= key && key <= 0xF8FF) {
        process_table(Specials, key);
    } else if (key < 0x20) {
        process_table(Controls, key);
    } else if (Buffer.insert_mode) {
        buf_ins(&Buffer, CursorPos++, key);
        TargetCol = buf_getcol(&Buffer, CursorPos);
    } else {
        process_table(ViKeys, key);
    }
}
