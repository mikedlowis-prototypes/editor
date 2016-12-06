#include <atf.h>
#include "../xedit.c"

int Mods = 0;

/* Helper Functions
 *****************************************************************************/
void setup_view(enum RegionId id, char* text) {
    Focused = id;
    view_init(getview(id), NULL);
    view_putstr(getview(id), text);
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
    /* Key Handling - Cursor Movement
     *************************************************************************/
    TEST(left arrow should do nothing for empty buffer) {
        setup_view(EDIT, "");
        key_handler(ModNone, KEY_LEFT);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
        CHECK(getsel(EDIT)->col == 0);
    }
    
    TEST(right arrow should do nothing for empty buffer) {
        setup_view(EDIT, "");
        key_handler(ModNone, KEY_RIGHT);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
        CHECK(getsel(EDIT)->col == 0);
    }
    
    TEST(up arrow should do nothing for empty buffer) {
        setup_view(EDIT, "");
        key_handler(ModNone, KEY_UP);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
        CHECK(getsel(EDIT)->col == 0);
    }
    
    TEST(down arrow should do nothing for empty buffer) {
        setup_view(EDIT, "");
        key_handler(ModNone, KEY_DOWN);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
        CHECK(getsel(EDIT)->col == 0);
    }
    
    TEST(home should do nothing for empty buffer) {
        setup_view(EDIT, "");
        key_handler(ModNone, KEY_HOME);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
        CHECK(getsel(EDIT)->col == 0);
    }
    
    TEST(end should do nothing for empty buffer) {
        setup_view(EDIT, "");
        key_handler(ModNone, KEY_END);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
        CHECK(getsel(EDIT)->col == 0);
    }
}