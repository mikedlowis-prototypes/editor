#include "edit.h"

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
    static uint32_t num_clicks = 0;
    static uint32_t prevtime = 0;
    uint32_t now = getmillis();
    num_clicks = (now - prevtime < 250 ? num_clicks+1 : 1);
    prevtime = now;
    if (!Buffer.modified || num_clicks >= 2)
        exit(0);
}

static void insert(Rune r) {
    if (!Buffer.insert_mode) return;
    buf_ins(&Buffer, CursorPos++, r);
    TargetCol = buf_getcol(&Buffer, CursorPos);
}

/*****************************************************************************/

typedef struct {
    Rune key;
    void (*action)(void);
} KeyBinding_T;

static KeyBinding_T Normal[] = {
    { KEY_F6,    toggle_colors },
    { KEY_UP,    cursor_up     },
    { KEY_DOWN,  cursor_dn     },
    { KEY_LEFT,  cursor_left   },
    { KEY_RIGHT, cursor_right  },
    { KEY_HOME,  cursor_bol    },
    { KEY_END,   cursor_eol    },
    { 'q',       quit          },
    { 's',       write         },
    { 'a',       insert_after  },
    { 'i',       insert_before },
    { 'k',       cursor_up     },
    { 'j',       cursor_dn     },
    { 'h',       cursor_left   },
    { 'l',       cursor_right  },
    { '0',       cursor_bol    },
    { '$',       cursor_eol    },
    { 0,         NULL          }
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
    { KEY_DELETE,    delete        },
    { KEY_BACKSPACE, backspace     },
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
