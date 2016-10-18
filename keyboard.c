#include "edit.h"

static void special_keys(Rune key);
static void control_keys(Rune key);
static void vi_keys(Rune key);

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

void handle_key(Rune key) {
    /* handle the proper line endings */
    if (key == '\r') key = '\n';
    if (key == '\n' && Buffer.crlf) key = RUNE_CRLF;
    /* ignore invalid keys */
    if (key == RUNE_ERR) return;
    /* handle the special keys */
    if (0xE000 <= key && key <= 0xF8FF) {
        special_keys(key);
    } else if (key < 0x20) {
        control_keys(key);
    } else if (Buffer.insert_mode) {
        buf_ins(&Buffer, CursorPos++, key);
        TargetCol = buf_getcol(&Buffer, CursorPos);
    } else {
        vi_keys(key);
    }
}

static void special_keys(Rune key) {
    switch (key) {
        case KEY_F6:     toggle_colors(); break;
        case KEY_UP:     cursor_up();     break;
        case KEY_DOWN:   cursor_dn();     break;
        case KEY_LEFT:   cursor_left();   break;
        case KEY_RIGHT:  cursor_right();  break;
        case KEY_INSERT: toggle_mode();   break;
        case KEY_F1:     toggle_mode();   break;
        case KEY_DELETE: delete();        break;
        case KEY_HOME:   cursor_bol();    break;
        case KEY_END:    cursor_eol();    break;
    }
}

static void control_keys(Rune key) {
    switch (key) {
        case KEY_ESCAPE:    Buffer.insert_mode = false; break;
        case KEY_BACKSPACE: backspace();                break;
        case KEY_CTRL_W:    buf_save(&Buffer);          break;
        case KEY_CTRL_Q:    exit(0);                    break;
        default:            insert(key);                break;
    }
}

static void vi_keys(Rune key) {
    switch (key) {
        case 'a': insert_after();  break;
        case 'i': insert_before(); break;
        case 'k': cursor_up();     break;
        case 'j': cursor_dn();     break;
        case 'h': cursor_left();   break;
        case 'l': cursor_right();  break;
        case '0': cursor_bol();    break;
        case '$': cursor_eol();    break;
    }
}
