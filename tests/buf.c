#include "atf.h"
#include "edit.h"

static Buf TestBuf;

static void buf_clr(Buf* buf) {
    free(buf->bufstart);
    buf_init(buf);
}

static void set_buffer_text(char* str) {
    int i = 0;
    buf_clr(&TestBuf);
    TestBuf.crlf = 1;
    for (Rune* curr = TestBuf.bufstart; curr < TestBuf.bufend; curr++)
        *curr = '-';
    TestBuf.locked = false;
    while (*str)
        buf_ins(&TestBuf, i++, (Rune)*str++);
}

static bool buf_text_eq(char* str) {
    for (unsigned i = 0; i < buf_end(&TestBuf); i++) {
        if ((Rune)*(str++) != buf_get(&TestBuf, i))
            return false;
    }
    return true;
}

TEST_SUITE(BufferTests) {
    /* Initializing
     *************************************************************************/
    /* Loading
     *************************************************************************/
    /* Saving
     *************************************************************************/
    /* Resizing
     *************************************************************************/
    /* Insertions
     *************************************************************************/
    /* Deletions
     *************************************************************************/
    /* Undo/Redo
     *************************************************************************/
    /* Locking
     *************************************************************************/

    /* Accessors
     *************************************************************************/
    // buf_get
    TEST(buf_get should return newline for indexes outside the buffer) {
        set_buffer_text("test");
        CHECK('\n' == buf_get(&TestBuf, 4));
    }

    TEST(buf_get should indexed character before the gap) {
        set_buffer_text("ac");
        buf_ins(&TestBuf, 1, 'b');
        CHECK('a' == buf_get(&TestBuf, 0));
    }

    TEST(buf_get should indexed character after the gap) {
        set_buffer_text("ac");
        buf_ins(&TestBuf, 1, 'b');
        CHECK('c' == buf_get(&TestBuf, 2));
    }

    TEST(buf_end should return the index just after the last rune in the buffer) {
        set_buffer_text("abc");
        CHECK(3 == buf_end(&TestBuf));
    }

    TEST(buf_iseol should return true if index points to a newline rune) {
        set_buffer_text("abc\ncba");
        CHECK(buf_iseol(&TestBuf, 3));
    }

    TEST(buf_iseol should return true if index points to a crlf rune) {
        CHECK(!"Test causes an assertion in the syncgap function. no idea why");
        set_buffer_text("abc\r\ncba");
        CHECK(buf_iseol(&TestBuf, 0));
    }

    TEST(buf_iseol should return false if index does not point to a newline rune) {
        set_buffer_text("abc\ncba");
        CHECK(!buf_iseol(&TestBuf, 2));
    }

    /* Movements
     *************************************************************************/
    // Start of Line
    TEST(buf_bol should return 0 if column 1 of first line) {
        set_buffer_text("ab\ncd");
        CHECK(0 == buf_bol(&TestBuf, 2));
    }

    TEST(buf_bol should return 0 if column 2 of first line) {
        set_buffer_text("ab\ncd");
        CHECK(0 == buf_bol(&TestBuf, 1));
    }

    TEST(buf_bol should return 0 if index points to newline) {
        set_buffer_text("ab\ncd");
        CHECK(0 == buf_bol(&TestBuf, 0));
    }

    TEST(buf_bol should return 3 if column 1 of second line) {
        set_buffer_text("ab\ncd");
        CHECK(3 == buf_bol(&TestBuf, 3));
    }

    TEST(buf_bol should return 3 if column 2 of second line) {
        set_buffer_text("ab\ncd");
        CHECK(3 == buf_bol(&TestBuf, 4));
    }

    TEST(buf_bol should return input if index is outside buffer) {
        set_buffer_text("ab\ncd");
        CHECK(6 == buf_bol(&TestBuf, 6));
    }

    // End of Line
    // Start of Word
    // End of Word
    // Scan Left
    // Scan Right
    // By Rune
    // By Line

    /* Literal Find
     *************************************************************************/
    /* Cursor Column Tracking
     *************************************************************************/



    ///* Insertions
    // *************************************************************************/
    //TEST(buf_ins should insert at 0 in empty buf) {
    //    buf_clr(&TestBuf);
    //    buf_ins(&TestBuf, 0, 'a');
    //    CHECK(buf_text_eq("a"));
    //}

    //TEST(buf_ins should insert at 0) {
    //    buf_clr(&TestBuf);
    //    buf_ins(&TestBuf, 0, 'b');
    //    buf_ins(&TestBuf, 0, 'a');
    //    CHECK(buf_text_eq("ab"));
    //}

    //TEST(buf_ins should insert at 1) {
    //    buf_clr(&TestBuf);
    //    buf_ins(&TestBuf, 0, 'a');
    //    buf_ins(&TestBuf, 1, 'b');
    //    CHECK(buf_text_eq("ab"));
    //}

    //TEST(buf_ins should insert at 1) {
    //    buf_clr(&TestBuf);
    //    buf_ins(&TestBuf, 0, 'a');
    //    buf_ins(&TestBuf, 1, 'c');
    //    buf_ins(&TestBuf, 1, 'b');
    //    CHECK(buf_text_eq("abc"));
    //}

    //TEST(buf_ins should sentence in larger text) {
    //    set_buffer_text(
    //        "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Aliquam elementum eros quis venenatis. "
    //    );

    //    buf_ins(&TestBuf, 5, ' ');
    //    buf_ins(&TestBuf, 6, 'a');

    //    CHECK(buf_text_eq(
    //        "Lorem a ipsum dolor sit amet, consectetur adipiscing elit. Aliquam elementum eros quis venenatis. "
    //    ));
    //}

    ///* Deletions
    // *************************************************************************/

    ///* Beginning and End of line
    // *************************************************************************/
    //TEST(buf_bol should move to first non newline character of line) {
    //    set_buffer_text("\nabc\n");
    //    CHECK(1 == buf_bol(&TestBuf, 3));
    //}

    //TEST(buf_bol should do nothing for blank line) {
    //    set_buffer_text("\n\n");
    //    CHECK(1 == buf_bol(&TestBuf, 1));
    //}

    //TEST(buf_eol should move to last character of line) {
    //    set_buffer_text("\nabc\n");
    //    CHECK(4 == buf_eol(&TestBuf, 1));
    //}

    //TEST(buf_eol should do nothing for blank line) {
    //    set_buffer_text("\n\n");
    //    CHECK(1 == buf_eol(&TestBuf, 1));
    //}

    ///* Movement by Rune
    // *************************************************************************/
    //TEST(buf_byrune should do nothing for -1 at beginning of file)
    //{
    //    set_buffer_text("abc\n");
    //    CHECK(0 == buf_byrune(&TestBuf, 0, -1));
    //}

    //TEST(buf_byrune should do nothing for -2 at beginning of file)
    //{
    //    set_buffer_text("abc\n");
    //    CHECK(0 == buf_byrune(&TestBuf, 0, -2));
    //}

    //TEST(buf_byrune should move to just after last rune for +1 at end of file)
    //{
    //    set_buffer_text("abc\n");
    //    CHECK(4 == buf_byrune(&TestBuf, 3, 1));
    //}

    //TEST(buf_byrune should move to just after last rune for +2 at end of file)
    //{
    //    set_buffer_text("abc\n");
    //    CHECK(4 == buf_byrune(&TestBuf, 3, 2));
    //}

    ////TEST(buf_byrune should skip newlines for -1)
    ////{
    ////    set_buffer_text("ab\ncd\n");
    ////    CHECK(1 == buf_byrune(&TestBuf, 3, -1));
    ////}

    ////TEST(buf_byrune should skip newlines for +1)
    ////{
    ////    set_buffer_text("ab\ncd\n");
    ////    CHECK(3 == buf_byrune(&TestBuf, 1, 1));
    ////}

    ////TEST(buf_byrune should not skip blank lines for -1)
    ////{
    ////    set_buffer_text("ab\n\ncd\n");
    ////    CHECK(3 == buf_byrune(&TestBuf, 4, -1));
    ////}

    ////TEST(buf_byrune should not skip blank lines for +1)
    ////{
    ////    set_buffer_text("ab\n\ncd\n");
    ////    CHECK(3 == buf_byrune(&TestBuf, 1, 1));
    ////}

    ////TEST(buf_byrune should move from blank line to non-blank line for -1)
    ////{
    ////    set_buffer_text("ab\n\ncd\n");
    ////    CHECK(1 == buf_byrune(&TestBuf, 3, -1));
    ////}

    //TEST(buf_byrune should move from blank line to non-blank line for +1)
    //{
    //    set_buffer_text("ab\n\ncd\n");
    //    CHECK(4 == buf_byrune(&TestBuf, 3, 1));
    //}

    ///* Movement by Line
    // *************************************************************************/

}
