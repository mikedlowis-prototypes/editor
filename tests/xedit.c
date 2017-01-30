#include <atf.h>
#include <time.h>

enum {
    LF = 0, 
    CRLF = 1
};

// Test Globals
int Mods = 0;
int ExitCode = 0;
char* PrimaryText = NULL;
char* ClipboardText = NULL;

// fake out the exit routine
void mockexit(int code) {
    ExitCode = code;
}

// Inculd the source file so we can access everything 
#include "../xedit.c"

/* Helper Functions
 *****************************************************************************/
void setup_view(enum RegionId id, char* text, int crlf, unsigned cursor) {
    Focused = id;
    view_init(getview(id), NULL);
    getbuf(id)->crlf = crlf;
    view_putstr(getview(id), text);
    getsel(id)->beg = cursor;
    getsel(id)->end = cursor;
    getsel(id)->col = buf_getcol(getbuf(id), getsel(id)->end);
}

void send_keys(int mods, Rune key) {
    Mods = mods;
    key_handler(mods, key);
}

bool verify_text(enum RegionId id, char* text) {
    Sel sel = { .beg = 0, .end = buf_end(getbuf(id)) };
    char* buftext = view_getstr(getview(id), &sel);
    //printf("buftext: '%s'\n", buftext);
    bool result;
    if (buftext == NULL)
        result = (*text == '\0');
    else
        result = (0 == strcmp(buftext, text));
    free(buftext);
    return result;
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

void x11_deinit(void) {
    mockexit(0);
}

bool x11_getsel(int selid, void(*cbfn)(char*)) {
    cbfn(selid == PRIMARY ? PrimaryText : ClipboardText);
    return true;
}

bool x11_setsel(int selid, char* str) {
    if (selid == PRIMARY) {
        free(PrimaryText);
        PrimaryText = str;
    } else {
        free(ClipboardText);
        ClipboardText = str;
    }   
    return true;
}

/* Unit Tests
 *****************************************************************************/
TEST_SUITE(XeditTests) {
    /* Key Handling - Normal Input
     *************************************************************************/
    TEST(input not matching a shortcut should be inserted as text) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModNone, 'e');
        CHECK(getsel(EDIT)->beg == 1);
        CHECK(getsel(EDIT)->end == 1);
    }

    TEST(input \n chould result in RUNE_CRLF if in crlf mode) {
        setup_view(EDIT, "", CRLF, 0);
        getbuf(EDIT)->crlf = 1;
        send_keys(ModNone, '\n');
        CHECK(getsel(EDIT)->beg == 1);
        CHECK(getsel(EDIT)->end == 1);
        CHECK(RUNE_CRLF == buf_get(getbuf(EDIT), 0));
    }
    
    TEST(input \n chould result in \n if not in crlf mode) {
        setup_view(EDIT, "", CRLF, 0);
        getbuf(EDIT)->crlf = 0;
        send_keys(ModNone, '\n');
        CHECK(getsel(EDIT)->beg == 1);
        CHECK(getsel(EDIT)->end == 1);
        CHECK('\n' == buf_get(getbuf(EDIT), 0));
    }

    /* Key Handling - Cursor Movement - Basic
     *************************************************************************/
    TEST(left should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModNone, KEY_LEFT);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(ctrl+left should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModCtrl, KEY_LEFT);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(left should do nothing at beginning of buffer) {
        setup_view(EDIT, "AB", CRLF, 0);
        send_keys(ModNone, KEY_LEFT);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(ctrl+left should do nothing at beginning of buffer) {
        setup_view(EDIT, "AB", CRLF, 0);
        send_keys(ModCtrl, KEY_LEFT);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(ctrl+left should move left by one word) {
        setup_view(EDIT, "AB CD", CRLF, 3);
        send_keys(ModCtrl, KEY_LEFT);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(left should move left by one rune) {
        setup_view(EDIT, "AB", CRLF, 1);
        send_keys(ModNone, KEY_LEFT);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(right should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModNone, KEY_RIGHT);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(right should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModCtrl, KEY_RIGHT);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(ctrl+right should do nothing at end of buffer) {
        setup_view(EDIT, "AB", CRLF, 2);
        send_keys(ModNone, KEY_RIGHT);
        CHECK(getsel(EDIT)->beg == 2);
        CHECK(getsel(EDIT)->end == 2);
    }
    
    TEST(ctrl+right should do nothing at end of buffer) {
        setup_view(EDIT, "AB", CRLF, 2);
        send_keys(ModCtrl, KEY_RIGHT);
        CHECK(getsel(EDIT)->beg == 2);
        CHECK(getsel(EDIT)->end == 2);
    }
    
    TEST(ctrl+right should move right by one word) {
        setup_view(EDIT, "AB CD", CRLF, 0);
        send_keys(ModCtrl, KEY_RIGHT);
        CHECK(getsel(EDIT)->beg == 3);
        CHECK(getsel(EDIT)->end == 3);
    }
    
    TEST(right should move right by one rune) {
        setup_view(EDIT, "AB", CRLF, 1);
        send_keys(ModNone, KEY_RIGHT);
        CHECK(getsel(EDIT)->beg == 2);
        CHECK(getsel(EDIT)->end == 2);
    }
    
    TEST(up should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModNone, KEY_UP);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(up should move cursor up one line) {
        setup_view(EDIT, "AB\nCD", CRLF, 4);
        send_keys(ModNone, KEY_UP);
        CHECK(getsel(EDIT)->beg == 1);
        CHECK(getsel(EDIT)->end == 1);
    }
    
    TEST(up should do nothing for first line) {
        setup_view(EDIT, "AB\nCD", CRLF, 1);
        send_keys(ModNone, KEY_UP);
        CHECK(getsel(EDIT)->beg == 1);
        CHECK(getsel(EDIT)->end == 1);
    }
    
    TEST(down should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModNone, KEY_DOWN);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }    
    
    TEST(down should move down one line) {
        setup_view(EDIT, "AB\nCD", CRLF, 1);
        send_keys(ModNone, KEY_DOWN);
        CHECK(getsel(EDIT)->beg == 4);
        CHECK(getsel(EDIT)->end == 4);
    }
    
    TEST(down should do nothing on last line) {
        setup_view(EDIT, "AB\nCD", CRLF, 4);
        send_keys(ModNone, KEY_DOWN);
        CHECK(getsel(EDIT)->beg == 4);
        CHECK(getsel(EDIT)->end == 4);
    }
    
    TEST(home should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModNone, KEY_HOME);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(ctrl+home should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModCtrl, KEY_HOME);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(ctrl+home should move to beginning of buffer) {
        setup_view(EDIT, "ABCD", CRLF, 4);
        send_keys(ModCtrl, KEY_HOME);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(home should move to beginning of indented content) {
        setup_view(EDIT, "    ABCD", CRLF, 7);
        send_keys(ModNone, KEY_HOME);
        CHECK(getsel(EDIT)->beg == 4);
        CHECK(getsel(EDIT)->end == 4);
    }
    
    TEST(home should move to beginning of line if at beginning of indented content) {
        setup_view(EDIT, "    ABCD", CRLF, 7);
        send_keys(ModNone, KEY_HOME);
        CHECK(getsel(EDIT)->beg == 4);
        CHECK(getsel(EDIT)->end == 4);
    }
    
    TEST(home should move to beginning of indented content when at beginning of line) {
        setup_view(EDIT, "    ABCD", CRLF, 0);
        send_keys(ModNone, KEY_HOME);
        CHECK(getsel(EDIT)->beg == 4);
        CHECK(getsel(EDIT)->end == 4);
    }
    
    TEST(end should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModNone, KEY_END);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(ctrl+end should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModCtrl, KEY_END);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(ctrl+end should move to end of buffer) {
        setup_view(EDIT, "ABCD", CRLF, 0);
        send_keys(ModCtrl, KEY_END);
        CHECK(getsel(EDIT)->beg == 4);
        CHECK(getsel(EDIT)->end == 4);
    }
    
    TEST(end should do nothing at end of line) {
        setup_view(EDIT, "AB\nCD", CRLF, 2);
        send_keys(ModNone, KEY_END);
        CHECK(getsel(EDIT)->beg == 2);
        CHECK(getsel(EDIT)->end == 2);
    }
    
    TEST(end should move to end of line) {
        setup_view(EDIT, "AB\nCD", CRLF, 0);
        send_keys(ModNone, KEY_END);
        CHECK(getsel(EDIT)->beg == 2);
        CHECK(getsel(EDIT)->end == 2);
    }

    /* Key Handling - Unix Editing Shortcuts
     *************************************************************************/
    TEST(ctrl+u should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModCtrl, 'u');
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(ctrl+u should delete to beginning of line) {
        setup_view(EDIT, "\nABC\n", CRLF, 2);
        send_keys(ModCtrl, 'u');
        CHECK(getsel(EDIT)->beg == 1);
        CHECK(getsel(EDIT)->end == 1);
        CHECK(buf_end(getbuf(EDIT)) == 4);
    }
    
    TEST(ctrl+u should delete entire lines contents) {
        setup_view(EDIT, "\nABC\n", CRLF, 3);
        send_keys(ModCtrl, 'u');
        CHECK(getsel(EDIT)->beg == 1);
        CHECK(getsel(EDIT)->end == 1);
        CHECK(buf_end(getbuf(EDIT)) == 3);
    }
    
    TEST(ctrl+k should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModCtrl, 'k');
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
        CHECK(buf_end(getbuf(EDIT)) == 0);
    }
    
    TEST(ctrl+k should delete to end of line) {
        setup_view(EDIT, "\nABC\n", CRLF, 2);
        send_keys(ModCtrl, 'k');
        CHECK(getsel(EDIT)->beg == 2);
        CHECK(getsel(EDIT)->end == 2);
        CHECK(buf_end(getbuf(EDIT)) == 3);
    }
    
    TEST(ctrl+k should delete entire lines contents) {
        setup_view(EDIT, "\nABC\n", CRLF, 1);
        send_keys(ModCtrl, 'k');
        CHECK(getsel(EDIT)->beg == 1);
        CHECK(getsel(EDIT)->end == 1);
        CHECK(buf_end(getbuf(EDIT)) == 2);
    }
    
    TEST(ctrl+w should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModCtrl, 'w');
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
        CHECK(buf_end(getbuf(EDIT)) == 0);
    }
    
    TEST(ctrl+w should delete previous word) {
        setup_view(EDIT, "abc def", CRLF, 4);
        send_keys(ModCtrl, 'w');
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
        CHECK(buf_end(getbuf(EDIT)) == 3);
    }
    
    TEST(ctrl+h should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModCtrl, 'h');
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(ctrl+h should delete previous character) {
        setup_view(EDIT, "AB", CRLF, 1);
        send_keys(ModCtrl, 'h');
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
        CHECK(buf_end(getbuf(EDIT)) == 1);
    }
    
    TEST(ctrl+a should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModCtrl, 'a');
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(ctrl+a should move to beginning of indented content) {
        setup_view(EDIT, "    ABCD", CRLF, 7);
        send_keys(ModCtrl, 'a');
        CHECK(getsel(EDIT)->beg == 4);
        CHECK(getsel(EDIT)->end == 4);
    }
    
    TEST(ctrl+a should move to beginning of line if at beginning of indented content) {
        setup_view(EDIT, "    ABCD", CRLF, 7);
        send_keys(ModCtrl, 'a');
        CHECK(getsel(EDIT)->beg == 4);
        CHECK(getsel(EDIT)->end == 4);
    }
    
    TEST(ctrl+a should move to beginning of indented content when at beginning of line) {
        setup_view(EDIT, "    ABCD", CRLF, 0);
        send_keys(ModCtrl, 'a');
        CHECK(getsel(EDIT)->beg == 4);
        CHECK(getsel(EDIT)->end == 4);
    }
    
    TEST(ctrl+e should do nothing for empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModCtrl, 'e');
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(ctrl+e should do nothing at end of line) {
        setup_view(EDIT, "AB\nCD", CRLF, 2);
        send_keys(ModCtrl, 'e');
        CHECK(getsel(EDIT)->beg == 2);
        CHECK(getsel(EDIT)->end == 2);
    }
    
    TEST(ctrl+e should move to end of line) {
        setup_view(EDIT, "AB\nCD", CRLF, 0);
        send_keys(ModCtrl, 'e');
        CHECK(getsel(EDIT)->beg == 2);
        CHECK(getsel(EDIT)->end == 2);
    }
    
    TEST(esc should select previously inserted text) {
        setup_view(EDIT, "foo", CRLF, 0);
        send_keys(ModNone, KEY_ESCAPE);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 3);
    }
    
    TEST(esc should select previously edited text) {
        setup_view(EDIT, "foob", CRLF, 4);
        send_keys(ModNone, KEY_BACKSPACE);
        send_keys(ModNone, KEY_ESCAPE);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 3);
    }
    
    /* Key Handling - Standard Text Editing Shortcuts
     *************************************************************************/
    TEST(cut and paste should delete selection and transfer it to+from the system clipboard) {
        setup_view(EDIT, "foo\nbar\nbaz\n", CRLF, 0);
        getview(EDIT)->selection = (Sel){ 0, 8, 0 };
        send_keys(ModCtrl, 'x');
        getview(EDIT)->selection = (Sel){ 4, 4, 0 };
        send_keys(ModCtrl, 'v');
        CHECK(getsel(EDIT)->beg == 4);
        CHECK(getsel(EDIT)->end == 12);
        CHECK(verify_text(EDIT, "baz\r\nfoo\r\nbar\r\n"));
    }
    
    TEST(copy and paste should copy selection and transfer it to+from the system clipboard) {
        getbuf(EDIT)->crlf = 1;
        setup_view(EDIT, "foo\nbar\nbaz\n", CRLF, 0);
        getview(EDIT)->selection = (Sel){ 0, 8, 0 };
        send_keys(ModCtrl, 'c');
        getview(EDIT)->selection = (Sel){ 12, 12, 0 };
        send_keys(ModCtrl, 'v');
        CHECK(getsel(EDIT)->beg == 12);
        CHECK(getsel(EDIT)->end == 20);
        CHECK(verify_text(EDIT, "foo\r\nbar\r\nbaz\r\nfoo\r\nbar\r\n"));
    }
    
    /* Key Handling - Block Indent
     *************************************************************************/
    TEST(ctrl+[ should do nothing on empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModCtrl, '[');
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(ctrl+[ should do nothing at beginning of buffer) {
        setup_view(EDIT, "a", CRLF, 0);
        send_keys(ModCtrl, '[');
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 1);
    }
    
    TEST(ctrl+] should do nothing on empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModCtrl, ']');
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
    }
    
    TEST(ctrl+] should indent the first line) {
        setup_view(EDIT, "a", CRLF, 0);
        send_keys(ModCtrl, ']');
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 5);
        CHECK(verify_text(EDIT, "    a"));
    }
    
    /* Key Handling - Special 
     *************************************************************************/
    TEST(backspace should do nothing on empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModNone, KEY_BACKSPACE);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
        CHECK(verify_text(EDIT, ""));
    }

    TEST(backspace should do nothing at beginning of buffer) {
        setup_view(EDIT, "abc", CRLF, 0);
        send_keys(ModNone, KEY_BACKSPACE);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
        CHECK(verify_text(EDIT, "abc"));
    }
    
    TEST(backspace should delete previous character) {
        setup_view(EDIT, "abc", CRLF, 1);
        send_keys(ModNone, KEY_BACKSPACE);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
        CHECK(verify_text(EDIT, "bc"));
    }
    
    TEST(backspace should delete selection) {
        setup_view(EDIT, "abcde", CRLF, 0);
        getview(EDIT)->selection = (Sel){ .beg = 1, .end = 4 };
        send_keys(ModNone, KEY_BACKSPACE);
        CHECK(getsel(EDIT)->beg == 1);
        CHECK(getsel(EDIT)->end == 1);
        CHECK(verify_text(EDIT, "ae"));
    }
    
    TEST(delete should do nothing on empty buffer) {
        setup_view(EDIT, "", CRLF, 0);
        send_keys(ModNone, KEY_DELETE);
        CHECK(getsel(EDIT)->beg == 0);
        CHECK(getsel(EDIT)->end == 0);
        CHECK(verify_text(EDIT, ""));
    }

    TEST(delete should do nothing at end of buffer) {
        setup_view(EDIT, "abc", CRLF, 3);
        send_keys(ModNone, KEY_DELETE);
        CHECK(getsel(EDIT)->beg == 3);
        CHECK(getsel(EDIT)->end == 3);
        CHECK(verify_text(EDIT, "abc"));
    }
    
    TEST(delete should delete next character) {
        setup_view(EDIT, "abc", CRLF, 2);
        send_keys(ModNone, KEY_DELETE);
        CHECK(getsel(EDIT)->beg == 2);
        CHECK(getsel(EDIT)->end == 2);
        CHECK(verify_text(EDIT, "ab"));
    }
    
    TEST(delete should delete selection) {
        setup_view(EDIT, "abcde", CRLF, 0);
        getview(EDIT)->selection = (Sel){ .beg = 1, .end = 4 };
        send_keys(ModNone, KEY_DELETE);
        CHECK(getsel(EDIT)->beg == 1);
        CHECK(getsel(EDIT)->end == 1);
        CHECK(verify_text(EDIT, "ae"));
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
    
    TEST(ctrl+q should quit) {
        setup_view(TAGS, "", CRLF, 0);
        setup_view(EDIT, "", CRLF, 0);
        getbuf(EDIT)->modified = false;
        ExitCode = 42;
        send_keys(ModCtrl, 'q');
        CHECK(ExitCode == 0);
        CHECK(verify_text(TAGS, ""));
    }
    
    TEST(ctrl+f should find next occurrence of selected text) {
        setup_view(EDIT, "foobarfoo", CRLF, 0);
        getview(EDIT)->selection = (Sel){ 0, 3, 0 };
        send_keys(ModCtrl, 'f');
        CHECK(getview(EDIT)->selection.beg == 6);
        CHECK(getview(EDIT)->selection.end == 9);
    }
    
    TEST(ctrl+f should wrap around to beginning of buffer) {
        setup_view(EDIT, "foobarbazfoo", CRLF, 0);
        getview(EDIT)->selection = (Sel){ 9, 12, 0 };
        send_keys(ModCtrl, 'f');
        CHECK(getview(EDIT)->selection.beg == 0);
        CHECK(getview(EDIT)->selection.end == 3);
    }
    
    /* Mouse Input Handling
     *************************************************************************/
    
    /* Command Execution
     *************************************************************************/
    TEST(Commands starting with : should be executed as sed scripts) {
        setup_view(EDIT, "foo", CRLF, 0);
        getview(EDIT)->selection = (Sel){ .beg = 0, .end = 3 };
        setup_view(TAGS, ":s/foo/bar/", CRLF, 0);
        getview(TAGS)->selection = (Sel){ .beg = 0, .end = 11 };
        send_keys(ModCtrl, 'd');
        #ifdef __MACH__
        CHECK(verify_text(EDIT, "bar\r\n"));
        #else
        CHECK(verify_text(EDIT, "bar"));
        #endif
    }
    
    TEST(Commands starting with | should be take selection as input and replace it with output) {
        setup_view(EDIT, "foo", CRLF, 0);
        getview(EDIT)->selection = (Sel){ .beg = 0, .end = 3 };
        setup_view(TAGS, "|sed -e 's/foo/bar/'", CRLF, 0);
        getview(TAGS)->selection = (Sel){ .beg = 0, .end = 20 };
        send_keys(ModCtrl, 'd');
        #ifdef __MACH__
        CHECK(verify_text(EDIT, "bar\r\n"));
        #else
        CHECK(verify_text(EDIT, "bar"));
        #endif
    }
    
    TEST(Commands starting with ! should execute in the background with no input or output) {
        setup_view(EDIT, "foo", CRLF, 0);
        getview(EDIT)->selection = (Sel){ .beg = 0, .end = 3 };
        setup_view(TAGS, "!ls", CRLF, 0);
        getview(TAGS)->selection = (Sel){ .beg = 0, .end = 3 };
        send_keys(ModCtrl, 'd');
        CHECK(verify_text(EDIT, "foo"));        
    }

#if 0
    TEST(Commands starting with > should execute in the background with selection as input) {
        setup_view(EDIT, "foo", CRLF, 0);
        getview(EDIT)->selection = (Sel){ .beg = 0, .end = 3 };
        setup_view(TAGS, ">cat", CRLF, 0);
        getview(TAGS)->selection = (Sel){ .beg = 0, .end = 4 };
        send_keys(ModCtrl, 'd');
        CHECK(verify_text(EDIT, "foo"));        
    }
#endif

    TEST(Commands starting with < should replace selection with output) {
        setup_view(EDIT, "foo", CRLF, 0);
        getview(EDIT)->selection = (Sel){ .beg = 0, .end = 3 };
        setup_view(TAGS, "<echo bar", CRLF, 0);
        getview(TAGS)->selection = (Sel){ .beg = 0, .end = 9 };
        send_keys(ModCtrl, 'd');
        CHECK(verify_text(EDIT, "bar\r\n"));        
    }
    
    TEST(Commands not starting with a sigil should replace themselves with their output) {
        setup_view(EDIT, "echo foo", CRLF, 0);
        getview(EDIT)->selection = (Sel){ .beg = 0, .end = 8 };
        send_keys(ModCtrl, 'd');
        CHECK(verify_text(EDIT, "foo\r\n"));        
    }
    
    /* Tag Handling
     *************************************************************************/
    TEST(Quit should  quit immediately if buffer is unmodified) {
        setup_view(TAGS, "", CRLF, 0);
        setup_view(EDIT, "", CRLF, 0);
        getbuf(EDIT)->modified = false;
        ExitCode = 42;
        exec("Quit");
        CHECK(ExitCode == 0);
        CHECK(verify_text(TAGS, ""));
    }
    
    TEST(Quit should display error message when quit called with unsaved changes) {
        setup_view(TAGS, "", CRLF, 0);
        setup_view(EDIT, "", CRLF, 0);
        getbuf(EDIT)->modified = true;
        ExitCode = 42;
        usleep(251 * 1000);
        exec("Quit");
        CHECK(ExitCode == 42);
        CHECK(verify_text(TAGS, "File is modified. Repeat action twice in < 250ms to quit."));
    }
    
    TEST(Quit should discard changes if quit executed twice in less than 250 ms) {
        setup_view(TAGS, "", CRLF, 0);
        setup_view(EDIT, "", CRLF, 0);
        getbuf(EDIT)->modified = true;
        ExitCode = 42;
        usleep(251 * 1000);
        exec("Quit");
        CHECK(ExitCode == 42);
        CHECK(verify_text(TAGS, "File is modified. Repeat action twice in < 250ms to quit."));
        exec("Quit");
        CHECK(ExitCode == 0);
        CHECK(verify_text(TAGS, "File is modified. Repeat action twice in < 250ms to quit."));
    }

    TEST(Save should save changes to disk with crlf line endings) {
        setup_view(TAGS, "", CRLF, 0);
        view_init(getview(EDIT), "docs/crlf.txt");
        CHECK(verify_text(EDIT, "this file\r\nuses\r\ndos\r\nline\r\nendings\r\n"));
        exec("Save");
        view_init(getview(EDIT), "docs/crlf.txt");
        CHECK(verify_text(EDIT, "this file\r\nuses\r\ndos\r\nline\r\nendings\r\n"));
    }
    
    TEST(Save should save changes to disk with lf line endings) {
        setup_view(TAGS, "", CRLF, 0);
        view_init(getview(EDIT), "docs/lf.txt");
        CHECK(verify_text(EDIT, "this file\nuses\nunix\nline\nendings\n"));
        exec("Save");
        view_init(getview(EDIT), "docs/lf.txt");
        CHECK(verify_text(EDIT, "this file\nuses\nunix\nline\nendings\n"));
    }

    TEST(Cut and Paste tags should move selection to new location) {
        setup_view(EDIT, "foo\nbar\nbaz\n", CRLF, 0);
        getview(EDIT)->selection = (Sel){ 0, 8, 0 };
        exec("Cut");
        getview(EDIT)->selection = (Sel){ 4, 4, 0 };
        exec("Paste");
        CHECK(getsel(EDIT)->beg == 4);
        CHECK(getsel(EDIT)->end == 12);
        CHECK(verify_text(EDIT, "baz\r\nfoo\r\nbar\r\n"));
    }
    
    TEST(Copy and Paste tags should copy selection to new location) {
        setup_view(EDIT, "foo\nbar\nbaz\n", CRLF, 0);
        getview(EDIT)->selection = (Sel){ 0, 8, 0 };
        exec("Copy");
        getview(EDIT)->selection = (Sel){ 12, 12, 0 };
        exec("Paste");
        CHECK(getsel(EDIT)->beg == 12);
        CHECK(getsel(EDIT)->end == 20);
        CHECK(verify_text(EDIT, "foo\r\nbar\r\nbaz\r\nfoo\r\nbar\r\n"));
    }
    
    TEST(Undo+Redo should undo+redo the previous insert) {
        setup_view(EDIT, "foo", CRLF, 0);
        exec("Undo");
        CHECK(verify_text(EDIT, ""));
        exec("Redo");
        CHECK(verify_text(EDIT, "foo"));
    }
    
    TEST(Undo+Redo should undo+redo the previous delete) {
        setup_view(EDIT, "foo", CRLF, 0);
        getview(EDIT)->selection = (Sel){ 0, 3, 0 };
        send_keys(ModNone, KEY_DELETE);
        exec("Undo");
        CHECK(verify_text(EDIT, "foo"));
        exec("Redo");
        CHECK(verify_text(EDIT, ""));
    }
    
    TEST(Undo+Redo should undo+redo the previous delete with two non-contiguous deletes logged) {
        setup_view(EDIT, "foobarbaz", CRLF, 0);
        getview(EDIT)->selection = (Sel){ 0, 3, 0 };
        send_keys(ModNone, KEY_DELETE);
        getview(EDIT)->selection = (Sel){ 3, 6, 0 };
        send_keys(ModNone, KEY_DELETE);
        CHECK(verify_text(EDIT, "bar"));
        exec("Undo");
        CHECK(verify_text(EDIT, "barbaz"));
        exec("Redo");
        CHECK(verify_text(EDIT, "bar"));
    }
    
    TEST(Undo+Redo should undo+redo the previous delete performed with backspace) {
        setup_view(EDIT, "foo", CRLF, 0);
        getview(EDIT)->selection = (Sel){ 3, 3, 0 };
        for(int i = 0; i < 3; i++)
            send_keys(ModNone, KEY_BACKSPACE);
        CHECK(verify_text(EDIT, ""));
        exec("Undo");
        CHECK(verify_text(EDIT, "foo"));
        exec("Redo");
        CHECK(verify_text(EDIT, ""));
    }

    TEST(Find should find next occurrence of text selected in EDIT view) {
        setup_view(EDIT, "foobarfoo", CRLF, 0);
        getview(EDIT)->selection = (Sel){ 0, 3, 0 };
        exec("Find");
        CHECK(getview(EDIT)->selection.beg == 6);
        CHECK(getview(EDIT)->selection.end == 9);
    }
    
    TEST(Find should find wrap around to beginning of EDIT buffer) {
        setup_view(EDIT, "foobarbazfoo", CRLF, 0);
        getview(EDIT)->selection = (Sel){ 9, 12, 0 };
        exec("Find");
        CHECK(getview(EDIT)->selection.beg == 0);
        CHECK(getview(EDIT)->selection.end == 3);
    }
    
    TEST(Find should find next occurrence of text selected in TAGS view) {
        setup_view(TAGS, "foo", CRLF, 0);
        getview(TAGS)->selection = (Sel){ 0, 3, 0 };
        setup_view(EDIT, "barfoo", CRLF, 0);
        exec("Find");
        CHECK(getview(EDIT)->selection.beg == 3);
        CHECK(getview(EDIT)->selection.end == 6);
    }
    
    TEST(Find should find next occurrence of text selected after tag) {
        setup_view(EDIT, "barfoo", CRLF, 0);
        exec("Find foo");
        CHECK(getview(EDIT)->selection.beg == 3);
        CHECK(getview(EDIT)->selection.end == 6);
    }

#if 0    
    TEST(Find should do nothing if nothing selected) {
        setup_view(TAGS, "Find", CRLF, 0);
        getview(TAGS)->selection = (Sel){ 0, 4, 0 };
        send_keys(ModCtrl, 'f');
    }
#endif

    TEST(Tabs should set indent style to tabs) {
        setup_view(TAGS, "Tabs", CRLF, 0);
        getview(TAGS)->selection = (Sel){ 0, 4, 0 };
        getbuf(EDIT)->expand_tabs = true;
        getbuf(TAGS)->expand_tabs = true;
        send_keys(ModCtrl, 'd');
        CHECK(getbuf(EDIT)->expand_tabs == false);
        CHECK(getbuf(TAGS)->expand_tabs == false);
    }
    
    TEST(Tabs should set indent style to spaces) {
        setup_view(TAGS, "Tabs", CRLF, 0);
        getview(TAGS)->selection = (Sel){ 0, 4, 0 };
        getbuf(EDIT)->expand_tabs = false;
        getbuf(TAGS)->expand_tabs = false;
        send_keys(ModCtrl, 'd');
        CHECK(getbuf(EDIT)->expand_tabs == true);
        CHECK(getbuf(TAGS)->expand_tabs == true);
    }
    
    TEST(Indent should disable copyindent) {
        setup_view(TAGS, "Indent", CRLF, 0);
        getview(TAGS)->selection = (Sel){ 0, 6, 0 };
        getbuf(EDIT)->copy_indent = true;
        getbuf(TAGS)->copy_indent = true;
        send_keys(ModCtrl, 'd');
        CHECK(getbuf(EDIT)->copy_indent == false);
        CHECK(getbuf(TAGS)->copy_indent == false);
    }
    
    TEST(Indent should enable copyindent) {
        setup_view(TAGS, "Indent", CRLF, 0);
        getview(TAGS)->selection = (Sel){ 0, 6, 0 };
        getbuf(EDIT)->copy_indent = false;
        getbuf(TAGS)->copy_indent = false;
        send_keys(ModCtrl, 'd');
        CHECK(getbuf(EDIT)->copy_indent == true);
        CHECK(getbuf(TAGS)->copy_indent == true);
    }
}
