#define INCLUDE_DEFS
#include <atf.h>
#include <time.h>
#include <setjmp.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

void x11_handle_event(XEvent* e);

enum {
    LF = 0,
    CRLF = 1
};

// Test Globals
bool ExitExpected = false;
int ExitCode = 0;
jmp_buf ExitPad;
Display* XDisplay;

// Include the source file so we can access everything
#include "../tide.c"

static void initialize(void) {
    ShellCmd[0] = "/bin/sh";
    win_window("edit", false, ondiagmsg);
    XDisplay = XOpenDisplay(NULL);
    win_setkeys(Bindings, NULL);
}

/* Helper Functions
 *****************************************************************************/
#define EXPECT_EXIT \
    if ((ExitExpected = true, 0 == setjmp(ExitPad)))

void setup_view(WinRegion id, char* text, int crlf, unsigned cursor) {
    win_setregion(id);
    win_buf(id)->crlf = crlf;
    win_settext(id, text);
    win_sel(id)->beg = cursor;
    win_sel(id)->end = cursor;
    win_sel(id)->col = buf_getcol(win_buf(id), win_sel(id)->end);
}

void insert(WinRegion id, char* text) {
    view_putstr(win_view(id), text);
}

void send_keys(uint mods, uint key) {
    XEvent e;
    e.xkey.display = XDisplay,
    e.xkey.type    = KeyPress,
    e.xkey.state   = mods,
    e.xkey.keycode = XKeysymToKeycode(XDisplay, key),
    x11_handle_event(&e);
}

bool verify_text(WinRegion id, char* text) {
    Sel sel = { .beg = 0, .end = buf_end(win_buf(id)) };
    char* buftext = view_getstr(win_view(id), &sel);
    //printf("buftext: '%s'\n", buftext);
    bool result;
    if (buftext == NULL)
        result = (*text == '\0');
    else
        result = (0 == strcmp(buftext, text));
    free(buftext);
    return result;
}

/* Unit Tests
 *****************************************************************************/
TEST_SUITE(UnitTests) {
    /* Key Handling - Normal Input
     *************************************************************************/
    TEST(input not matching a shortcut should be inserted as text) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModNone, XK_e);
        verify_text(EDIT, "e");
        CHECK(win_sel(EDIT)->beg == 1);
        CHECK(win_sel(EDIT)->end == 1);
    }

    TEST(input \n should result in RUNE_CRLF if in crlf mode) {
        setup_view(EDIT, "", CRLF, 0);
        win_buf(EDIT)->crlf = 1;
        send_keys(ModNone, XK_Return);
        CHECK(win_sel(EDIT)->beg == 1);
        CHECK(win_sel(EDIT)->end == 1);
        CHECK(RUNE_CRLF == buf_get(win_buf(EDIT), 0));
    }

    TEST(input \n chould result in \n if not in crlf mode) {
        setup_view(EDIT, "", CRLF, 0);
        win_buf(EDIT)->crlf = 0;
        send_keys(ModNone, XK_Return);
        CHECK(win_sel(EDIT)->beg == 1);
        CHECK(win_sel(EDIT)->end == 1);
        CHECK('\n' == buf_get(win_buf(EDIT), 0));
    }
    /* Key Handling - Cursor Movement - Basic
     *************************************************************************/
    TEST(left should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModNone, XK_Left);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
    }

    TEST(ctrl+left should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModCtrl, XK_Left);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
    }

    TEST(left should do nothing at beginning of buffer) {
        setup_view(EDIT, "AB", CRLF, 0);
        send_keys(ModNone, XK_Left);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
    }

    TEST(ctrl+left should do nothing at beginning of buffer) {
        setup_view(EDIT, "AB", CRLF, 0);
        send_keys(ModCtrl, XK_Left);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
    }

    TEST(ctrl+left should move left by one word) {
        setup_view(EDIT, "AB CD", CRLF, 3);
        send_keys(ModCtrl, XK_Left);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
    }

    TEST(left should move left by one rune) {
        setup_view(EDIT, "AB", CRLF, 1);
        send_keys(ModNone, XK_Left);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
    }

    TEST(right should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModNone, XK_Right);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
    }

    TEST(right should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModCtrl, XK_Right);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
    }

    TEST(ctrl+right should do nothing at end of buffer) {
        setup_view(EDIT, "AB", CRLF, 2);
        send_keys(ModNone, XK_Right);
        CHECK(win_sel(EDIT)->beg == 2);
        CHECK(win_sel(EDIT)->end == 2);
    }

    TEST(ctrl+right should do nothing at end of buffer) {
        setup_view(EDIT, "AB", CRLF, 2);
        send_keys(ModCtrl, XK_Right);
        CHECK(win_sel(EDIT)->beg == 2);
        CHECK(win_sel(EDIT)->end == 2);
    }

    TEST(ctrl+right should move right by one word) {
        setup_view(EDIT, "AB CD", CRLF, 0);
        send_keys(ModCtrl, XK_Right);
        CHECK(win_sel(EDIT)->beg == 2);
        CHECK(win_sel(EDIT)->end == 2);
    }

    TEST(right should move right by one rune) {
        setup_view(EDIT, "AB", CRLF, 1);
        send_keys(ModNone, XK_Right);
        CHECK(win_sel(EDIT)->beg == 2);
        CHECK(win_sel(EDIT)->end == 2);
    }

    TEST(up should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModNone, XK_Up);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
    }

    TEST(up should move cursor up one line) {
        setup_view(EDIT, "AB\nCD", CRLF, 4);
        send_keys(ModNone, XK_Up);
        CHECK(win_sel(EDIT)->beg == 1);
        CHECK(win_sel(EDIT)->end == 1);
    }

    TEST(up should do nothing for first line) {
        setup_view(EDIT, "AB\nCD", CRLF, 1);
        send_keys(ModNone, XK_Up);
        CHECK(win_sel(EDIT)->beg == 1);
        CHECK(win_sel(EDIT)->end == 1);
    }

    TEST(down should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModNone, XK_Down);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
    }

    TEST(down should move down one line) {
        setup_view(EDIT, "AB\nCD", CRLF, 1);
        send_keys(ModNone, XK_Down);
        CHECK(win_sel(EDIT)->beg == 4);
        CHECK(win_sel(EDIT)->end == 4);
    }

    TEST(down should do nothing on last line) {
        setup_view(EDIT, "AB\nCD", CRLF, 4);
        send_keys(ModNone, XK_Down);
        CHECK(win_sel(EDIT)->beg == 4);
        CHECK(win_sel(EDIT)->end == 4);
    }

    TEST(home should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModNone, XK_Home);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
    }

    TEST(ctrl+home should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModCtrl, XK_Home);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
    }

    TEST(ctrl+home should move to beginning of buffer) {
        setup_view(EDIT, "ABCD", CRLF, 4);
        send_keys(ModCtrl, XK_Home);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
    }

    TEST(home should move to beginning of indented content) {
        setup_view(EDIT, "    ABCD", CRLF, 7);
        send_keys(ModNone, XK_Home);
        CHECK(win_sel(EDIT)->beg == 4);
        CHECK(win_sel(EDIT)->end == 4);
    }

    TEST(home should move to beginning of line if at beginning of indented content) {
        setup_view(EDIT, "    ABCD", CRLF, 7);
        send_keys(ModNone, XK_Home);
        CHECK(win_sel(EDIT)->beg == 4);
        CHECK(win_sel(EDIT)->end == 4);
    }

    TEST(home should move to beginning of indented content when at beginning of line) {
        setup_view(EDIT, "    ABCD", CRLF, 0);
        send_keys(ModNone, XK_Home);
        CHECK(win_sel(EDIT)->beg == 4);
        CHECK(win_sel(EDIT)->end == 4);
    }

    TEST(end should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModNone, XK_End);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
    }

    TEST(ctrl+end should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModCtrl, XK_End);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
    }

    TEST(ctrl+end should move to end of buffer) {
        setup_view(EDIT, "ABCD", CRLF, 0);
        send_keys(ModCtrl, XK_End);
        CHECK(win_sel(EDIT)->beg == 4);
        CHECK(win_sel(EDIT)->end == 4);
    }

    TEST(end should do nothing at end of line) {
        setup_view(EDIT, "AB\nCD", CRLF, 2);
        send_keys(ModNone, XK_End);
        CHECK(win_sel(EDIT)->beg == 2);
        CHECK(win_sel(EDIT)->end == 2);
    }

    TEST(end should move to end of line) {
        setup_view(EDIT, "AB\nCD", CRLF, 0);
        send_keys(ModNone, XK_End);
        CHECK(win_sel(EDIT)->beg == 2);
        CHECK(win_sel(EDIT)->end == 2);
    }

    /* Key Handling - Unix Editing Shortcuts
     *************************************************************************/
    TEST(ctrl+u should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModCtrl, XK_u);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
    }

    TEST(ctrl+u should delete to beginning of line) {
        setup_view(EDIT, "\nABC\n", CRLF, 2);
        send_keys(ModCtrl, XK_u);
        CHECK(win_sel(EDIT)->beg == 1);
        CHECK(win_sel(EDIT)->end == 1);
        CHECK(buf_end(win_buf(EDIT)) == 4);
    }

    TEST(ctrl+u should delete entire lines contents) {
        setup_view(EDIT, "\nABC\n", CRLF, 3);
        send_keys(ModCtrl, XK_u);
        CHECK(win_sel(EDIT)->beg == 1);
        CHECK(win_sel(EDIT)->end == 1);
        CHECK(buf_end(win_buf(EDIT)) == 3);
    }

    TEST(ctrl+k should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModCtrl, XK_k);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
        CHECK(buf_end(win_buf(EDIT)) == 0);
    }

    TEST(ctrl+k should delete to end of line) {
        setup_view(EDIT, "\nABC\n", CRLF, 2);
        send_keys(ModCtrl, XK_k);
        CHECK(win_sel(EDIT)->beg == 2);
        CHECK(win_sel(EDIT)->end == 2);
        CHECK(buf_end(win_buf(EDIT)) == 3);
    }

    TEST(ctrl+k should delete entire lines contents) {
        setup_view(EDIT, "\nABC\n", CRLF, 1);
        send_keys(ModCtrl, XK_k);
        CHECK(win_sel(EDIT)->beg == 1);
        CHECK(win_sel(EDIT)->end == 1);
        CHECK(buf_end(win_buf(EDIT)) == 2);
    }

    TEST(ctrl+w should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModCtrl, XK_w);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
        CHECK(buf_end(win_buf(EDIT)) == 0);
    }

    TEST(ctrl+w should delete previous word) {
        setup_view(EDIT, "abc def", CRLF, 4);
        send_keys(ModCtrl, XK_w);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
        CHECK(buf_end(win_buf(EDIT)) == 3);
    }

    TEST(ctrl+a should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModCtrl, XK_a);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
    }

    TEST(ctrl+a should move to beginning of indented content) {
        setup_view(EDIT, "    ABCD", CRLF, 7);
        send_keys(ModCtrl, XK_a);
        CHECK(win_sel(EDIT)->beg == 4);
        CHECK(win_sel(EDIT)->end == 4);
    }

    TEST(ctrl+a should move to beginning of line if at beginning of indented content) {
        setup_view(EDIT, "    ABCD", CRLF, 7);
        send_keys(ModCtrl, XK_a);
        CHECK(win_sel(EDIT)->beg == 4);
        CHECK(win_sel(EDIT)->end == 4);
    }

    TEST(ctrl+a should move to beginning of indented content when at beginning of line) {
        setup_view(EDIT, "    ABCD", CRLF, 0);
        send_keys(ModCtrl, XK_a);
        CHECK(win_sel(EDIT)->beg == 4);
        CHECK(win_sel(EDIT)->end == 4);
    }

    TEST(ctrl+e should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModCtrl, XK_e);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
    }

    TEST(ctrl+e should do nothing at end of line) {
        setup_view(EDIT, "AB\nCD", CRLF, 2);
        send_keys(ModCtrl, XK_e);
        CHECK(win_sel(EDIT)->beg == 2);
        CHECK(win_sel(EDIT)->end == 2);
    }

    TEST(ctrl+e should move to end of line) {
        setup_view(EDIT, "AB\nCD", CRLF, 0);
        send_keys(ModCtrl, XK_e);
        CHECK(win_sel(EDIT)->beg == 2);
        CHECK(win_sel(EDIT)->end == 2);
    }

    TEST(esc should select previously inserted text) {
        setup_view(EDIT, "", CRLF, 0);
        insert(EDIT, "foo");
        win_view(EDIT)->selection = (Sel) {0,0,0};
        send_keys(ModNone, XK_Escape);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 3);
    }

    TEST(esc should select previously edited text) {
        setup_view(EDIT, "foo", CRLF, 0);
        insert(EDIT, "foob");
        win_view(EDIT)->selection = (Sel) {4,4,4};
        send_keys(ModNone, XK_BackSpace);
        send_keys(ModNone, XK_Escape);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 3);
    }

    /* Key Handling - Standard Text Editing Shortcuts
     *************************************************************************/
    TEST(cut and paste should delete selection and transfer it to+from the system clipboard) {
        IGNORE("paste callback isn't happening");
        setup_view(EDIT, "foo\nbar\nbaz\n", CRLF, 0);
        win_view(EDIT)->selection = (Sel){ 0, 8, 0 };
        send_keys(ModCtrl, XK_x);
        win_view(EDIT)->selection = (Sel){ 4, 4, 0 };
        send_keys(ModCtrl, XK_v);
        CHECK(win_sel(EDIT)->beg == 4);
        CHECK(win_sel(EDIT)->end == 12);
        CHECK(verify_text(EDIT, "baz\r\nfoo\r\nbar\r\n"));
    }

    TEST(copy and paste should copy selection and transfer it to+from the system clipboard) {
        IGNORE("paste callback isn't happening");
        win_buf(EDIT)->crlf = 1;
        setup_view(EDIT, "foo\nbar\nbaz\n", CRLF, 0);
        win_view(EDIT)->selection = (Sel){ 0, 8, 0 };
        send_keys(ModCtrl, XK_c);
        win_view(EDIT)->selection = (Sel){ 12, 12, 0 };
        send_keys(ModCtrl, XK_v);
        CHECK(win_sel(EDIT)->beg == 12);
        CHECK(win_sel(EDIT)->end == 20);
        CHECK(verify_text(EDIT, "foo\r\nbar\r\nbaz\r\nfoo\r\nbar\r\n"));
    }

    /* Key Handling - Block Indent
     *************************************************************************/
    TEST(ctrl+[ should do nothing on empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModCtrl, XK_bracketleft);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
    }

    TEST(ctrl+[ should do nothing at beginning of buffer) {
        setup_view(EDIT, "a", CRLF, 0);
        send_keys(ModCtrl, XK_bracketleft);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 1);
    }

    TEST(ctrl+] should do nothing on empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModCtrl, XK_bracketright);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
    }

    TEST(ctrl+] should indent the first line) {
        setup_view(EDIT, "a", CRLF, 0);
        send_keys(ModCtrl, XK_bracketright);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 5);
        CHECK(verify_text(EDIT, "    a"));
    }

    /* Key Handling - Special
     *************************************************************************/
    TEST(backspace should do nothing on empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModNone, XK_BackSpace);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
        CHECK(verify_text(EDIT, ""));
    }

    TEST(backspace should do nothing at beginning of buffer) {
        setup_view(EDIT, "abc", CRLF, 0);
        send_keys(ModNone, XK_BackSpace);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
        CHECK(verify_text(EDIT, "abc"));
    }

    TEST(backspace should delete previous character) {
        setup_view(EDIT, "abc", CRLF, 1);
        send_keys(ModNone, XK_BackSpace);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
        CHECK(verify_text(EDIT, "bc"));
    }

    TEST(backspace should delete selection) {
        setup_view(EDIT, "abcde", CRLF, 0);
        win_view(EDIT)->selection = (Sel){ .beg = 1, .end = 4 };
        send_keys(ModNone, XK_BackSpace);
        CHECK(win_sel(EDIT)->beg == 1);
        CHECK(win_sel(EDIT)->end == 1);
        CHECK(verify_text(EDIT, "ae"));
    }

    TEST(delete should do nothing on empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModNone, XK_Delete);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
        CHECK(verify_text(EDIT, ""));
    }

    TEST(delete should do nothing at end of buffer) {
        setup_view(EDIT, "abc", CRLF, 3);
        send_keys(ModNone, XK_Delete);
        CHECK(win_sel(EDIT)->beg == 3);
        CHECK(win_sel(EDIT)->end == 3);
        CHECK(verify_text(EDIT, "abc"));
    }

    TEST(delete should delete next character) {
        setup_view(EDIT, "abc", CRLF, 2);
        send_keys(ModNone, XK_Delete);
        CHECK(win_sel(EDIT)->beg == 2);
        CHECK(win_sel(EDIT)->end == 2);
        CHECK(verify_text(EDIT, "ab"));
    }

    TEST(delete should delete selection) {
        setup_view(EDIT, "abcde", CRLF, 0);
        win_view(EDIT)->selection = (Sel){ .beg = 1, .end = 4 };
        send_keys(ModNone, XK_Delete);
        CHECK(win_sel(EDIT)->beg == 1);
        CHECK(win_sel(EDIT)->end == 1);
        CHECK(verify_text(EDIT, "ae"));
    }

    /* Key Handling - Implementation Specific
     *************************************************************************/
    TEST(ctrl+t should switch focus to EDIT view) {
        win_setregion(TAGS);
        send_keys(ModCtrl, XK_t);
        CHECK(win_getregion() == EDIT);
    }

    TEST(ctrl+t should switch focus to TAGs view) {
        win_setregion(EDIT);
        send_keys(ModCtrl, XK_t);
        CHECK(win_getregion() == TAGS);
    }

    TEST(ctrl+q should quit) {
        setup_view(TAGS, "", CRLF, 0);
        setup_view(EDIT, "", CRLF, 0);
        win_buf(EDIT)->modified = false;
        ExitCode = 42;
        EXPECT_EXIT {
            send_keys(ModCtrl, XK_q);
        }
        ExitExpected = false;
        CHECK(ExitCode == 0);
        CHECK(verify_text(TAGS, ""));
    }

    TEST(ctrl+f should find next occurrence of selected text) {
        setup_view(EDIT, "foobarfoo", CRLF, 0);
        win_view(EDIT)->selection = (Sel){ 0, 3, 0 };
        send_keys(ModCtrl, XK_f);
        CHECK(win_view(EDIT)->selection.beg == 6);
        CHECK(win_view(EDIT)->selection.end == 9);
    }

    TEST(ctrl+f should wrap around to beginning of buffer) {
        setup_view(EDIT, "foobarbazfoo", CRLF, 0);
        win_view(EDIT)->selection = (Sel){ 9, 12, 0 };
        send_keys(ModCtrl, XK_f);
        CHECK(win_view(EDIT)->selection.beg == 0);
        CHECK(win_view(EDIT)->selection.end == 3);
    }

    TEST(ctrl+h should nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModCtrl, XK_h);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 0);
    }

    TEST(ctrl+h should highlight content in parens from left paren) {
        setup_view(EDIT, " (foo bar) ", CRLF, 1);
        send_keys(ModCtrl, XK_h);
        CHECK(win_sel(EDIT)->beg == 2);
        CHECK(win_sel(EDIT)->end == 9);
    }

    TEST(ctrl+h should highlight content in parens from right paren) {
        setup_view(EDIT, " (foo bar) ", CRLF, 9);
        send_keys(ModCtrl, XK_h);
        CHECK(win_sel(EDIT)->beg == 9);
        CHECK(win_sel(EDIT)->end == 2);
    }

    TEST(ctrl+h should highlight content in parens from left bracket) {
        setup_view(EDIT, " [foo bar] ", CRLF, 1);
        send_keys(ModCtrl, XK_h);
        CHECK(win_sel(EDIT)->beg == 2);
        CHECK(win_sel(EDIT)->end == 9);
    }

    TEST(ctrl+h should highlight content in parens from right bracket) {
        setup_view(EDIT, " [foo bar] ", CRLF, 9);
        send_keys(ModCtrl, XK_h);
        CHECK(win_sel(EDIT)->beg == 9);
        CHECK(win_sel(EDIT)->end == 2);
    }

    TEST(ctrl+h should highlight content in parens from left brace) {
        setup_view(EDIT, " {foo bar} ", CRLF, 1);
        send_keys(ModCtrl, XK_h);
        CHECK(win_sel(EDIT)->beg == 2);
        CHECK(win_sel(EDIT)->end == 9);
    }

    TEST(ctrl+h should highlight content in parens from right brace) {
        setup_view(EDIT, " {foo bar} ", CRLF, 9);
        send_keys(ModCtrl, XK_h);
        CHECK(win_sel(EDIT)->beg == 9);
        CHECK(win_sel(EDIT)->end == 2);
    }

    TEST(ctrl+h should highlight whole line from bol) {
        setup_view(EDIT, "foo bar\n", CRLF, 0);
        send_keys(ModCtrl, XK_h);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 8);
    }

    TEST(ctrl+h should highlight whole line from eol) {
        setup_view(EDIT, "foo bar\n", CRLF, 7);
        send_keys(ModCtrl, XK_h);
        CHECK(win_sel(EDIT)->beg == 0);
        CHECK(win_sel(EDIT)->end == 8);
    }

    TEST(ctrl+h should highlight word under cursor) {
        setup_view(EDIT, " foo.bar \n", CRLF, 1);
        send_keys(ModCtrl, XK_h);
        CHECK(win_sel(EDIT)->beg == 1);
        CHECK(win_sel(EDIT)->end == 4);
    }

    TEST(ctrl+h should highlight word under cursor) {
        setup_view(EDIT, " foo.bar \n", CRLF, 4);
        send_keys(ModCtrl, XK_h);
        CHECK(win_sel(EDIT)->beg == 1);
        CHECK(win_sel(EDIT)->end == 8);
    }

    /* Mouse Input Handling
     *************************************************************************/

    /* Command Execution
     *************************************************************************/
    TEST(Commands starting with : should be executed as sed scripts) {
        setup_view(EDIT, "foo", CRLF, 0);
        win_view(EDIT)->selection = (Sel){ .beg = 0, .end = 3 };
        setup_view(TAGS, ":s/foo/bar/", CRLF, 0);
        win_view(TAGS)->selection = (Sel){ .beg = 0, .end = 11 };
        send_keys(ModCtrl, XK_d);
        do { event_poll(50); } while (exec_reap());
        #ifdef __MACH__
        CHECK(verify_text(EDIT, "bar\r\n"));
        #else
        CHECK(verify_text(EDIT, "bar"));
        #endif
    }

    TEST(Commands starting with | should take selection as input and replace it with output) {
        setup_view(EDIT, "foo", CRLF, 0);
        win_view(EDIT)->selection = (Sel){ .beg = 0, .end = 3 };
        setup_view(TAGS, "|sed -e 's/foo/bar/'", CRLF, 0);
        win_view(TAGS)->selection = (Sel){ .beg = 0, .end = 20 };
        send_keys(ModCtrl, XK_d);
        do { event_poll(50); } while (exec_reap());
        #ifdef __MACH__
        CHECK(verify_text(EDIT, "bar\r\n"));
        #else
        CHECK(verify_text(EDIT, "bar"));
        #endif
    }

    TEST(Commands starting with ! should execute in the background with no input or output) {
        setup_view(EDIT, "foo", CRLF, 0);
        win_view(EDIT)->selection = (Sel){ .beg = 0, .end = 3 };
        setup_view(TAGS, "!ls", CRLF, 0);
        win_view(TAGS)->selection = (Sel){ .beg = 0, .end = 3 };
        send_keys(ModCtrl, XK_d);
        do { event_poll(50); } while (exec_reap());
        CHECK(verify_text(EDIT, "foo"));
    }

    TEST(Commands starting with > should execute in the background with selection as input) {
        setup_view(EDIT, "foo", CRLF, 0);
        win_view(EDIT)->selection = (Sel){ .beg = 0, .end = 3 };
        setup_view(TAGS, ">cat", CRLF, 0);
        win_view(TAGS)->selection = (Sel){ .beg = 0, .end = 4 };
        send_keys(ModCtrl, XK_d);
        do { event_poll(50); } while (exec_reap());
        CHECK(verify_text(EDIT, "foo"));
    }

    TEST(Commands starting with < should replace selection with output) {
        setup_view(EDIT, "foo", CRLF, 0);
        win_view(EDIT)->selection = (Sel){ .beg = 0, .end = 3 };
        setup_view(TAGS, "<echo bar", CRLF, 0);
        win_view(TAGS)->selection = (Sel){ .beg = 0, .end = 9 };
        send_keys(ModCtrl, XK_d);
        do { event_poll(50); } while (exec_reap());
        CHECK(verify_text(EDIT, "bar\r\n"));
    }

    TEST(Commands not starting with a sigil should replace themselves with their output) {
        setup_view(EDIT, "echo foo", CRLF, 0);
        win_view(EDIT)->selection = (Sel){ .beg = 0, .end = 8 };
        send_keys(ModCtrl, XK_d);
        do { event_poll(50); } while (exec_reap());
        CHECK(verify_text(EDIT, "foo\r\n"));
    }

    /* Tag Handling
     *************************************************************************/
    TEST(Quit should quit immediately if buffer is unmodified) {
        setup_view(TAGS, "", CRLF, 0);
        setup_view(EDIT, "", CRLF, 0);
        win_buf(EDIT)->modified = false;
        ExitCode = 42;
        EXPECT_EXIT {
            exec("Quit");
        }
        ExitExpected = false;
        CHECK(ExitCode == 0);
        CHECK(verify_text(TAGS, ""));
    }

    TEST(Quit should display error message when quit called with unsaved changes) {
        setup_view(TAGS, "", CRLF, 0);
        setup_view(EDIT, "", CRLF, 0);
        win_buf(EDIT)->modified = true;
        ExitCode = 42;
        usleep(251 * 1000);
        exec("Quit");
        CHECK(ExitCode == 42);
        CHECK(verify_text(TAGS, "File is modified. Repeat action twice quickly to quit."));
    }

    TEST(Quit should discard changes if quit executed twice in less than DblClickTime) {
        setup_view(TAGS, "", CRLF, 0);
        setup_view(EDIT, "", CRLF, 0);
        win_buf(EDIT)->modified = true;
        ExitCode = 42;
        usleep((config_get_int(DblClickTime)+1) * 1000);
        EXPECT_EXIT {
            exec("Quit");
            CHECK(ExitCode == 42);
            CHECK(verify_text(TAGS, "File is modified. Repeat action twice quickly to quit."));
            exec("Quit");
        }
        ExitExpected = false;
        CHECK(ExitCode == 0);
        CHECK(verify_text(TAGS, "File is modified. Repeat action twice quickly to quit."));
    }

    TEST(Save should save changes to disk with crlf line endings) {
        setup_view(TAGS, "", CRLF, 0);
        view_init(win_view(EDIT), "testdocs/crlf.txt", ondiagmsg);
        CHECK(verify_text(EDIT, "this file\r\nuses\r\ndos\r\nline\r\nendings\r\n"));
        exec("Save");
        view_init(win_view(EDIT), "testdocs/crlf.txt", ondiagmsg);
        CHECK(verify_text(EDIT, "this file\r\nuses\r\ndos\r\nline\r\nendings\r\n"));
    }

    TEST(Save should save changes to disk with lf line endings) {
        setup_view(TAGS, "", CRLF, 0);
        view_init(win_view(EDIT), "testdocs/lf.txt", ondiagmsg);
        CHECK(verify_text(EDIT, "this file\nuses\nunix\nline\nendings\n"));
        exec("Save");
        view_init(win_view(EDIT), "testdocs/lf.txt", ondiagmsg);
        CHECK(verify_text(EDIT, "this file\nuses\nunix\nline\nendings\n"));
    }

    TEST(Cut and Paste tags should move selection to new location) {
        IGNORE("paste callback isn't happening");
        setup_view(EDIT, "", CRLF, 0);
        insert(EDIT, "foo\nbar\nbaz\n");
        win_view(EDIT)->selection = (Sel){ 0, 8, 0 };
        exec("Cut");
        win_view(EDIT)->selection = (Sel){ 4, 4, 0 };
        exec("Paste");
        CHECK(win_sel(EDIT)->beg == 4);
        CHECK(win_sel(EDIT)->end == 12);
        CHECK(verify_text(EDIT, "baz\r\nfoo\r\nbar\r\n"));
    }

    TEST(Copy and Paste tags should copy selection to new location) {
        IGNORE("paste callback isn't happening");
        setup_view(EDIT, "", CRLF, 0);
        insert(EDIT, "foo\nbar\nbaz\n");
        win_view(EDIT)->selection = (Sel){ 0, 8, 0 };
        exec("Copy");
        win_view(EDIT)->selection = (Sel){ 12, 12, 0 };
        exec("Paste");
        CHECK(win_sel(EDIT)->beg == 12);
        CHECK(win_sel(EDIT)->end == 20);
        CHECK(verify_text(EDIT, "foo\r\nbar\r\nbaz\r\nfoo\r\nbar\r\n"));
    }

    TEST(Undo+Redo should undo+redo the previous insert) {
        setup_view(EDIT, "", CRLF, 0);
        insert(EDIT, "foo");
        exec("Undo");
        CHECK(verify_text(EDIT, ""));
        exec("Redo");
        CHECK(verify_text(EDIT, "foo"));
    }

    TEST(Undo+Redo should undo+redo the previous delete) {
        setup_view(EDIT, "foo", CRLF, 0);
        win_view(EDIT)->selection = (Sel){ 0, 3, 0 };
        send_keys(ModNone, XK_Delete);
        exec("Undo");
        CHECK(verify_text(EDIT, "foo"));
        exec("Redo");
        CHECK(verify_text(EDIT, ""));
    }

    TEST(Undo+Redo should undo+redo the previous delete with two non-contiguous deletes logged) {
        setup_view(EDIT, "foobarbaz", CRLF, 0);
        win_view(EDIT)->selection = (Sel){ 0, 3, 0 };
        send_keys(ModNone, XK_Delete);
        win_view(EDIT)->selection = (Sel){ 3, 6, 0 };
        send_keys(ModNone, XK_Delete);
        CHECK(verify_text(EDIT, "bar"));
        exec("Undo");
        CHECK(verify_text(EDIT, "barbaz"));
        exec("Redo");
        CHECK(verify_text(EDIT, "bar"));
    }

    TEST(Undo+Redo should undo+redo the previous delete performed with backspace) {
        setup_view(EDIT, "foo", CRLF, 0);
        win_view(EDIT)->selection = (Sel){ 3, 3, 0 };
        for(int i = 0; i < 3; i++)
            send_keys(ModNone, XK_BackSpace);
        CHECK(verify_text(EDIT, ""));
        exec("Undo");
        CHECK(verify_text(EDIT, "foo"));
        exec("Redo");
        CHECK(verify_text(EDIT, ""));
    }


    TEST(Find should find next occurrence of text selected in EDIT view) {
        setup_view(EDIT, "foobarfoo", CRLF, 0);
        win_view(EDIT)->selection = (Sel){ 0, 3, 0 };
        exec("Find");
        CHECK(win_view(EDIT)->selection.beg == 6);
        CHECK(win_view(EDIT)->selection.end == 9);
    }

    TEST(Find should find wrap around to beginning of EDIT buffer) {
        setup_view(EDIT, "foobarbazfoo", CRLF, 0);
        win_view(EDIT)->selection = (Sel){ 9, 12, 0 };
        exec("Find");
        CHECK(win_view(EDIT)->selection.beg == 0);
        CHECK(win_view(EDIT)->selection.end == 3);
    }

    TEST(Find should find next occurrence of text selected in TAGS view) {
        setup_view(TAGS, "foo", CRLF, 0);
        win_view(TAGS)->selection = (Sel){ 0, 3, 0 };
        setup_view(EDIT, "barfoo", CRLF, 0);
        exec("Find");
        CHECK(win_view(EDIT)->selection.beg == 3);
        CHECK(win_view(EDIT)->selection.end == 6);
    }

    TEST(Find should find next occurrence of text selected after tag) {
        setup_view(EDIT, "barfoo", CRLF, 0);
        exec("Find foo");
        CHECK(win_view(EDIT)->selection.beg == 3);
        CHECK(win_view(EDIT)->selection.end == 6);
    }

    TEST(Find should do nothing if nothing selected) {
        setup_view(EDIT, "foo bar baz", CRLF, 0);
        setup_view(TAGS, "Find", CRLF, 0);
        win_view(TAGS)->selection = (Sel){ 0, 4, 0 };
        win_view(EDIT)->selection = (Sel){ 0, 0, 0 };
        send_keys(ModCtrl, 'f');
        CHECK(win_view(EDIT)->selection.beg == 0);
        CHECK(win_view(EDIT)->selection.end == 0);
    }

    TEST(Tabs should set indent style to tabs) {
        setup_view(TAGS, "Tabs", CRLF, 0);
        win_view(TAGS)->selection = (Sel){ 0, 4, 0 };
        win_buf(EDIT)->expand_tabs = true;
        win_buf(TAGS)->expand_tabs = true;
        send_keys(ModCtrl, 'd');
        CHECK(win_buf(EDIT)->expand_tabs == false);
        CHECK(win_buf(TAGS)->expand_tabs == false);
    }

    TEST(Tabs should set indent style to spaces) {
        setup_view(TAGS, "Tabs", CRLF, 0);
        win_view(TAGS)->selection = (Sel){ 0, 4, 0 };
        win_buf(EDIT)->expand_tabs = false;
        win_buf(TAGS)->expand_tabs = false;
        send_keys(ModCtrl, 'd');
        CHECK(win_buf(EDIT)->expand_tabs == true);
        CHECK(win_buf(TAGS)->expand_tabs == true);
    }

    TEST(Indent should disable copyindent) {
        setup_view(TAGS, "Indent", CRLF, 0);
        win_view(TAGS)->selection = (Sel){ 0, 6, 0 };
        win_buf(EDIT)->copy_indent = true;
        win_buf(TAGS)->copy_indent = true;
        send_keys(ModCtrl, 'd');
        CHECK(win_buf(EDIT)->copy_indent == false);
        CHECK(win_buf(TAGS)->copy_indent == false);
    }

    TEST(Indent should enable copyindent) {
        setup_view(TAGS, "Indent", CRLF, 0);
        win_view(TAGS)->selection = (Sel){ 0, 6, 0 };
        win_buf(EDIT)->copy_indent = false;
        win_buf(TAGS)->copy_indent = false;
        send_keys(ModCtrl, 'd');
        CHECK(win_buf(EDIT)->copy_indent == true);
        CHECK(win_buf(TAGS)->copy_indent == true);
    }
}

// fake out the exit routine
void exit(int code) {
    if (ExitExpected) {
        ExitCode = code;
        ExitExpected = false;
        longjmp(ExitPad, 1);
    } else {
        assert(!"Unexpected exit. Something went wrong");
    }
}

int main(int argc, char** argv) {
    initialize();
    atf_init(argc,argv);
    RUN_TEST_SUITE(UnitTests);
    _Exit(atf_print_results());
    return 0;
}
