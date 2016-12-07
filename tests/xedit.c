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
        key_handler(ModNone, KEY_LEFT);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(left should do nothing at beginning of buffer) {
        setup_view(EDIT, "AB", 0);
        key_handler(ModNone, KEY_LEFT);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(left should move left by one rune) {
        setup_view(EDIT, "AB", 1);
        key_handler(ModNone, KEY_LEFT);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(right should do nothing for empty buffer) {
        setup_view(EDIT, "", 0);
        key_handler(ModNone, KEY_RIGHT);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(right should do nothing at end of buffer) {
        setup_view(EDIT, "AB", 2);
        key_handler(ModNone, KEY_RIGHT);
        CHECK(getsel(EDIT)->beg == 2);
        CHECK(getsel(EDIT)->end == 2);
    }
    
    TEST(right should move right by one rune) {
        setup_view(EDIT, "AB", 1);
        key_handler(ModNone, KEY_RIGHT);
        CHECK(getsel(EDIT)->beg == 2);
        CHECK(getsel(EDIT)->end == 2);
    }
    
    TEST(up should do nothing for empty buffer) {
        setup_view(EDIT, "", 0);
        key_handler(ModNone, KEY_UP);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(up should move cursor up one line) {
        setup_view(EDIT, "AB\nCD", 4);
        key_handler(ModNone, KEY_UP);
        CHECK(getsel(EDIT)->beg == 1);
        CHECK(getsel(EDIT)->end == 1);
    }
    
    TEST(up should do nothing for first line) {
        setup_view(EDIT, "AB\nCD", 1);
        key_handler(ModNone, KEY_UP);
        CHECK(getsel(EDIT)->beg == 1);
        CHECK(getsel(EDIT)->end == 1);
    }
    
    TEST(down should do nothing for empty buffer) {
        setup_view(EDIT, "", 0);
        key_handler(ModNone, KEY_DOWN);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }    
    
    TEST(down should move down one line) {
        setup_view(EDIT, "AB\nCD", 1);
        key_handler(ModNone, KEY_DOWN);
        CHECK(getsel(EDIT)->beg == 4);
        CHECK(getsel(EDIT)->end == 4);
    }
    
    TEST(down should do nothing on last line) {
        setup_view(EDIT, "AB\nCD", 4);
        key_handler(ModNone, KEY_DOWN);
        CHECK(getsel(EDIT)->beg == 4);
        CHECK(getsel(EDIT)->end == 4);
    }
    
    TEST(home should do nothing for empty buffer) {
        setup_view(EDIT, "", 0);
        key_handler(ModNone, KEY_HOME);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(home should move to beginning of indented content) {
        setup_view(EDIT, "    ABCD", 7);
        key_handler(ModNone, KEY_HOME);
        CHECK(getsel(EDIT)->beg == 4);
        CHECK(getsel(EDIT)->end == 4);
    }
    
    TEST(home should move to beginning of line if at beginning of indented content) {
        setup_view(EDIT, "    ABCD", 7);
        key_handler(ModNone, KEY_HOME);
        CHECK(getsel(EDIT)->beg == 4);
        CHECK(getsel(EDIT)->end == 4);
    }
    
    TEST(home should move to beginning of indented content when at beginning of line) {
        setup_view(EDIT, "    ABCD", 0);
        key_handler(ModNone, KEY_HOME);
        CHECK(getsel(EDIT)->beg == 4);
        CHECK(getsel(EDIT)->end == 4);
    }
    
    TEST(end should do nothing for empty buffer) {
        setup_view(EDIT, "", 0);
        key_handler(ModNone, KEY_END);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(end should do nothing at end of line) {
        setup_view(EDIT, "AB\nCD", 2);
        key_handler(ModNone, KEY_END);
        CHECK(getsel(EDIT)->beg == 2);
        CHECK(getsel(EDIT)->end == 2);
    }
    
    TEST(end should move to end of line) {
        setup_view(EDIT, "AB\nCD", 0);
        key_handler(ModNone, KEY_END);
        CHECK(getsel(EDIT)->beg == 2);
        CHECK(getsel(EDIT)->end == 2);
    }
}
