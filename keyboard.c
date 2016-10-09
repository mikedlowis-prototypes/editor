#include "edit.h"

static void special_keys(Rune key);
static void control_keys(Rune key);
static void vi_keys(Rune key);

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

void handle_key(Rune key) {
    /* ignore invalid keys */
    if (key == RUNE_ERR) return;
    /* handle the special keys */
    if (0xE000 <= key && key <= 0xF8FF)
        special_keys(key);
    /* Handle regular key */
    else if (key < 0x20)
        control_keys(key);
    else if (Buffer.insert_mode)
        buf_ins(&Buffer, CursorPos++, key);
    else
        vi_keys(key);
}

static void special_keys(Rune key) {
    switch (key) {
        case KEY_F6:     ColorBase = !ColorBase;                   break;
        case KEY_UP:     cursor_up();                              break;
        case KEY_DOWN:   cursor_dn();                              break;
        case KEY_LEFT:   cursor_left();                            break;
        case KEY_RIGHT:  cursor_right();                           break;
        case KEY_INSERT: Buffer.insert_mode = !Buffer.insert_mode; break;
        case KEY_F1:     Buffer.insert_mode = !Buffer.insert_mode; break;
        case KEY_DELETE: buf_del(&Buffer, CursorPos);              break;
        case KEY_HOME:   CursorPos = buf_bol(&Buffer, CursorPos);  break;
        case KEY_END:    CursorPos = buf_eol(&Buffer, CursorPos);  break;
    }
}

static void control_keys(Rune key) {
    switch (key) {
        case KEY_ESCAPE:    Buffer.insert_mode = false;         break;
        case KEY_BACKSPACE: buf_del(&Buffer, --CursorPos);      break;
        case KEY_CTRL_W:    buf_save(&Buffer);                  break;
        case KEY_CTRL_Q:    exit(0);                            break;
        default:            buf_ins(&Buffer, CursorPos++, key); break;
    }
}

static void vi_keys(Rune key) {
    (void)key;
}

/*****************************************************************************/
