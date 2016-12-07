#include <atf.h>
#include "../xedit.c"

int Mods = 0;

/* Helper Functions
 *****************************************************************************/
void setup_view(enum RegionId id, char* text, unsigned cursor) {
    Focused = id;
    view_init(getview(id), NULL);
    view_putstr(getview(id), text);
    getsel(id)->beg = cursor;
    getsel(id)->end = cursor;
    getsel(id)->col = buf_getcol(getbuf(id), getsel(id)->end);
}

void send_keys(int mods, Rune key) {
    Mods = mods;
    key_handler(mods, key);
}

/* Stubbed Functions
 *****************************************************************************/
bool x11_keymodsset(int mask) {
    return ((Mods & mask) == mask);
}

size_t x11_font_height(XFont fnt) {
    return 10;
}

size_t x11_font_width(XFont fnt) {
    return 10;
}

static void redraw(int width, int height) {
    /* do nothing for unit testing */
}

/* Unit Tests
 *****************************************************************************/
TEST_SUITE(XeditTests) {
    /* Key Handling - Cursor Movement - Basic
     *************************************************************************/
    TEST(left should do nothing for empty buffer) {
        setup_view(EDIT, "", 0);
        send_keys(ModNone, KEY_LEFT);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(ctrl+left should do nothing for empty buffer) {
        setup_view(EDIT, "", 0);
        send_keys(ModCtrl, KEY_LEFT);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(left should do nothing at beginning of buffer) {
        setup_view(EDIT, "AB", 0);
        send_keys(ModNone, KEY_LEFT);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(ctrl+left should do nothing at beginning of buffer) {
        setup_view(EDIT, "AB", 0);
        send_keys(ModCtrl, KEY_LEFT);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(ctrl+left should move left by one word) {
        setup_view(EDIT, "AB CD", 3);
        send_keys(ModCtrl, KEY_LEFT);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(left should move left by one rune) {
        setup_view(EDIT, "AB", 1);
        send_keys(ModNone, KEY_LEFT);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(right should do nothing for empty buffer) {
        setup_view(EDIT, "", 0);
        send_keys(ModNone, KEY_RIGHT);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(right should do nothing for empty buffer) {
        setup_view(EDIT, "", 0);
        send_keys(ModCtrl, KEY_RIGHT);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(ctrl+right should do nothing at end of buffer) {
        setup_view(EDIT, "AB", 2);
        send_keys(ModNone, KEY_RIGHT);
        CHECK(getsel(EDIT)->beg == 2);
        CHECK(getsel(EDIT)->end == 2);
    }
    
    TEST(ctrl+right should do nothing at end of buffer) {
        setup_view(EDIT, "AB", 2);
        send_keys(ModCtrl, KEY_RIGHT);
        CHECK(getsel(EDIT)->beg == 2);
        CHECK(getsel(EDIT)->end == 2);
    }
    
    TEST(ctrl+right should move right by one word) {
        setup_view(EDIT, "AB CD", 0);
        send_keys(ModCtrl, KEY_RIGHT);
        CHECK(getsel(EDIT)->beg == 3);
        CHECK(getsel(EDIT)->end == 3);
    }
    
    TEST(right should move right by one rune) {
        setup_view(EDIT, "AB", 1);
        send_keys(ModNone, KEY_RIGHT);
        CHECK(getsel(EDIT)->beg == 2);
        CHECK(getsel(EDIT)->end == 2);
    }
    
    TEST(up should do nothing for empty buffer) {
        setup_view(EDIT, "", 0);
        send_keys(ModNone, KEY_UP);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(up should move cursor up one line) {
        setup_view(EDIT, "AB\nCD", 4);
        send_keys(ModNone, KEY_UP);
        CHECK(getsel(EDIT)->beg == 1);
        CHECK(getsel(EDIT)->end == 1);
    }
    
    TEST(up should do nothing for first line) {
        setup_view(EDIT, "AB\nCD", 1);
        send_keys(ModNone, KEY_UP);
        CHECK(getsel(EDIT)->beg == 1);
        CHECK(getsel(EDIT)->end == 1);
    }
    
    TEST(down should do nothing for empty buffer) {
        setup_view(EDIT, "", 0);
        send_keys(ModNone, KEY_DOWN);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }    
    
    TEST(down should move down one line) {
        setup_view(EDIT, "AB\nCD", 1);
        send_keys(ModNone, KEY_DOWN);
        CHECK(getsel(EDIT)->beg == 4);
        CHECK(getsel(EDIT)->end == 4);
    }
    
    TEST(down should do nothing on last line) {
        setup_view(EDIT, "AB\nCD", 4);
        send_keys(ModNone, KEY_DOWN);
        CHECK(getsel(EDIT)->beg == 4);
        CHECK(getsel(EDIT)->end == 4);
    }
    
    TEST(home should do nothing for empty buffer) {
        setup_view(EDIT, "", 0);
        send_keys(ModNone, KEY_HOME);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(ctrl+home should do nothing for empty buffer) {
        setup_view(EDIT, "", 0);
        send_keys(ModCtrl, KEY_HOME);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(ctrl+home should move to beginning of buffer) {
        setup_view(EDIT, "ABCD", 4);
        send_keys(ModCtrl, KEY_HOME);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(home should move to beginning of indented content) {
        setup_view(EDIT, "    ABCD", 7);
        send_keys(ModNone, KEY_HOME);
        CHECK(getsel(EDIT)->beg == 4);
        CHECK(getsel(EDIT)->end == 4);
    }
    
    TEST(home should move to beginning of line if at beginning of indented content) {
        setup_view(EDIT, "    ABCD", 7);
        send_keys(ModNone, KEY_HOME);
        CHECK(getsel(EDIT)->beg == 4);
        CHECK(getsel(EDIT)->end == 4);
    }
    
    TEST(home should move to beginning of indented content when at beginning of line) {
        setup_view(EDIT, "    ABCD", 0);
        send_keys(ModNone, KEY_HOME);
        CHECK(getsel(EDIT)->beg == 4);
        CHECK(getsel(EDIT)->end == 4);
    }
    
    TEST(end should do nothing for empty buffer) {
        setup_view(EDIT, "", 0);
        send_keys(ModNone, KEY_END);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(ctrl+end should do nothing for empty buffer) {
        setup_view(EDIT, "", 0);
        send_keys(ModCtrl, KEY_END);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(ctrl+end should move to end of buffer) {
        setup_view(EDIT, "ABCD", 0);
        send_keys(ModCtrl, KEY_END);
        CHECK(getsel(EDIT)->beg == 4);
        CHECK(getsel(EDIT)->end == 4);
    }
    
    TEST(end should do nothing at end of line) {
        setup_view(EDIT, "AB\nCD", 2);
        send_keys(ModNone, KEY_END);
        CHECK(getsel(EDIT)->beg == 2);
        CHECK(getsel(EDIT)->end == 2);
    }
    
    TEST(end should move to end of line) {
        setup_view(EDIT, "AB\nCD", 0);
        send_keys(ModNone, KEY_END);
        CHECK(getsel(EDIT)->beg == 2);
        CHECK(getsel(EDIT)->end == 2);
    }
    
    /* Key Handling - Unix Standard Shortcuts
     *************************************************************************/
    TEST(ctrl+u should do nothing for empty buffer) {
        setup_view(EDIT, "", 0);
        send_keys(ModCtrl, 'u');
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(ctrl+u should delete to beginning of line) {
        setup_view(EDIT, "\nABC\n", 2);
        send_keys(ModCtrl, 'u');
        CHECK(getsel(EDIT)->beg == 1);
        CHECK(getsel(EDIT)->end == 1);
        CHECK(buf_end(getbuf(EDIT)) == 4);
    }
    
    TEST(ctrl+u should delete entire lines contents) {
        setup_view(EDIT, "\nABC\n", 3);
        send_keys(ModCtrl, 'u');
        CHECK(getsel(EDIT)->beg == 1);
        CHECK(getsel(EDIT)->end == 1);
        CHECK(buf_end(getbuf(EDIT)) == 3);
    }
    
    TEST(ctrl+k should do nothing for empty buffer) {
        setup_view(EDIT, "", 0);
        send_keys(ModCtrl, 'k');
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
        CHECK(buf_end(getbuf(EDIT)) == 0);
    }
    
    TEST(ctrl+k should delete to end of line) {
        setup_view(EDIT, "\nABC\n", 2);
        send_keys(ModCtrl, 'k');
        CHECK(getsel(EDIT)->beg == 2);
        CHECK(getsel(EDIT)->end == 2);
        CHECK(buf_end(getbuf(EDIT)) == 3);
    }
    
    TEST(ctrl+k should delete entire lines contents) {
        setup_view(EDIT, "\nABC\n", 1);
        send_keys(ModCtrl, 'k');
        CHECK(getsel(EDIT)->beg == 1);
        CHECK(getsel(EDIT)->end == 1);
        CHECK(buf_end(getbuf(EDIT)) == 2);
    }
    
    TEST(ctrl+w should do nothing for empty buffer) {
        setup_view(EDIT, "", 0);
        send_keys(ModCtrl, 'w');
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
        CHECK(buf_end(getbuf(EDIT)) == 0);
    }
    
    TEST(ctrl+w should delete previous word) {
        setup_view(EDIT, "abc def", 4);
        send_keys(ModCtrl, 'w');
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
        CHECK(buf_end(getbuf(EDIT)) == 3);
    }
    
    TEST(ctrl+h should do nothing for empty buffer) {
        setup_view(EDIT, "", 0);
        send_keys(ModCtrl, 'h');
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(ctrl+h should delete previous character) {
        setup_view(EDIT, "AB", 1);
        send_keys(ModCtrl, 'h');
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
        CHECK(buf_end(getbuf(EDIT)) == 1);
    }
    
    TEST(ctrl+a should do nothing for empty buffer) {
        setup_view(EDIT, "", 0);
        send_keys(ModCtrl, 'a');
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(ctrl+a should move to beginning of indented content) {
        setup_view(EDIT, "    ABCD", 7);
        send_keys(ModCtrl, 'a');
        CHECK(getsel(EDIT)->beg == 4);
        CHECK(getsel(EDIT)->end == 4);
    }
    
    TEST(ctrl+a should move to beginning of line if at beginning of indented content) {
        setup_view(EDIT, "    ABCD", 7);
        send_keys(ModCtrl, 'a');
        CHECK(getsel(EDIT)->beg == 4);
        CHECK(getsel(EDIT)->end == 4);
    }
    
    TEST(ctrl+a should move to beginning of indented content when at beginning of line) {
        setup_view(EDIT, "    ABCD", 0);
        send_keys(ModCtrl, 'a');
        CHECK(getsel(EDIT)->beg == 4);
        CHECK(getsel(EDIT)->end == 4);
    }
    
    TEST(ctrl+e should do nothing for empty buffer) {
        setup_view(EDIT, "", 0);
        send_keys(ModCtrl, 'e');
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(ctrl+e should do nothing at end of line) {
        setup_view(EDIT, "AB\nCD", 2);
        send_keys(ModCtrl, 'e');
        CHECK(getsel(EDIT)->beg == 2);
        CHECK(getsel(EDIT)->end == 2);
    }
    
    TEST(ctrl+e should move to end of line) {
        setup_view(EDIT, "AB\nCD", 0);
        send_keys(ModCtrl, 'e');
        CHECK(getsel(EDIT)->beg == 2);
        CHECK(getsel(EDIT)->end == 2);
    }
    
    /* Key Handling - Normal Input
     *************************************************************************/
    TEST(input not matching a shortcut should be inserted as text) {
        setup_view(EDIT, "", 0);
        send_keys(ModNone, 'e');
        CHECK(getsel(EDIT)->beg == 1);
        CHECK(getsel(EDIT)->end == 1);
    }
    
    /* Key Handling - Implementation Specific
     *************************************************************************/
    TEST(ctrl+t should switch focus to EDIT view) {
        Focused = TAGS;
        send_keys(ModCtrl, 't');
        CHECK(Focused == EDIT);
    }
    
    TEST(ctrl+t should switch focus to TAGs view) {
        Focused = EDIT;
        send_keys(ModCtrl, 't');
        CHECK(Focused == TAGS);
    }
}
