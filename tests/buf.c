#include "atf.h"
#include "../buf.c"

static Buf TestBuf;

static void set_buffer_text(char* str) {
    int i = 0;
    buf_clr(&TestBuf);
    for (Rune* curr = TestBuf.bufstart; curr < TestBuf.bufend; curr++)
        *curr = '-';
    TestBuf.insert_mode = true;
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
    /* Init and Clear
     *************************************************************************/
    TEST(buf_initsz should init the buffer to the given size) {
        buf_initsz(&TestBuf, 1024);
        CHECK(TestBuf.bufsize == 1024);
        CHECK(TestBuf.bufstart == TestBuf.gapstart);
        CHECK(TestBuf.bufend == TestBuf.gapend);
        CHECK(TestBuf.bufend - TestBuf.bufstart == 1024);
        CHECK(TestBuf.gapend - TestBuf.gapstart == 1024);
        free(TestBuf.bufstart);
    }

    TEST(buf_init should init the buffer to the default size) {
        buf_init(&TestBuf);
        CHECK(TestBuf.bufsize == BufSize);
        CHECK(TestBuf.bufstart == TestBuf.gapstart);
        CHECK(TestBuf.bufend == TestBuf.gapend);
        CHECK(TestBuf.bufend - TestBuf.bufstart == BufSize);
        CHECK(TestBuf.gapend - TestBuf.gapstart == BufSize);
    }

    TEST(buf_clr should clear the buffer) {
        buf_initsz(&TestBuf, 1024);
        buf_clr(&TestBuf);
        CHECK(TestBuf.bufsize == BufSize);
        CHECK(TestBuf.bufstart == TestBuf.gapstart);
        CHECK(TestBuf.bufend == TestBuf.gapend);
        CHECK(TestBuf.bufend - TestBuf.bufstart == BufSize);
        CHECK(TestBuf.gapend - TestBuf.gapstart == BufSize);
    }

    /* Insertions
     *************************************************************************/
    TEST(buf_ins should insert at 0 in empty buf) {
        buf_clr(&TestBuf);
        buf_ins(&TestBuf, 0, 'a');
        CHECK(buf_text_eq("a"));
    }

    TEST(buf_ins should insert at 0) {
        buf_clr(&TestBuf);
        buf_ins(&TestBuf, 0, 'b');
        buf_ins(&TestBuf, 0, 'a');
        CHECK(buf_text_eq("ab"));
    }

    TEST(buf_ins should insert at 1) {
        buf_clr(&TestBuf);
        buf_ins(&TestBuf, 0, 'a');
        buf_ins(&TestBuf, 1, 'b');
        CHECK(buf_text_eq("ab"));
    }

    TEST(buf_ins should insert at 1) {
        buf_clr(&TestBuf);
        buf_ins(&TestBuf, 0, 'a');
        buf_ins(&TestBuf, 1, 'c');
        buf_ins(&TestBuf, 1, 'b');
        CHECK(buf_text_eq("abc"));
    }

    TEST(buf_ins should sentence in larger text) {
        set_buffer_text(
            "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Aliquam elementum eros quis venenatis. "
        );

        buf_ins(&TestBuf, 5, ' ');
        buf_ins(&TestBuf, 6, 'a');

        CHECK(buf_text_eq(
            "Lorem a ipsum dolor sit amet, consectetur adipiscing elit. Aliquam elementum eros quis venenatis. "
        ));
    }

    /* Deletions
     *************************************************************************/

    /* Beginning and End of line
     *************************************************************************/
    TEST(buf_bol should move to first non newline character of line) {
        set_buffer_text("\nabc\n");
        CHECK(1 == buf_bol(&TestBuf, 3));
    }

    TEST(buf_bol should do nothing for blank line) {
        set_buffer_text("\n\n");
        CHECK(1 == buf_bol(&TestBuf, 1));
    }

    TEST(buf_eol should move to last non newline character of line) {
        set_buffer_text("\nabc\n");
        CHECK(3 == buf_eol(&TestBuf, 1));
    }

    TEST(buf_eol should do nothing for blank line) {
        set_buffer_text("\n\n");
        CHECK(1 == buf_eol(&TestBuf, 1));
    }

    /* Movement by Rune
     *************************************************************************/
    TEST(buf_byrune should do nothing for -1 at beginning of file)
    {
        set_buffer_text("abc\n");
        CHECK(0 == buf_byrune(&TestBuf, 0, -1));
    }

    TEST(buf_byrune should do nothing for -2 at beginning of file)
    {
        set_buffer_text("abc\n");
        CHECK(0 == buf_byrune(&TestBuf, 0, -2));
    }

    TEST(buf_byrune should do nothing for +1 at end of file)
    {
        set_buffer_text("abc\n");
        CHECK(3 == buf_byrune(&TestBuf, 3, 1));
    }

    TEST(buf_byrune should do nothing for +2 at end of file)
    {
        set_buffer_text("abc\n");
        CHECK(3 == buf_byrune(&TestBuf, 3, 2));
    }

    //TEST(buf_byrune should skip newlines for -1)
    //{
    //    set_buffer_text("ab\ncd\n");
    //    CHECK(1 == buf_byrune(&TestBuf, 3, -1));
    //}

    //TEST(buf_byrune should skip newlines for +1)
    //{
    //    set_buffer_text("ab\ncd\n");
    //    CHECK(3 == buf_byrune(&TestBuf, 1, 1));
    //}

    //TEST(buf_byrune should not skip blank lines for -1)
    //{
    //    set_buffer_text("ab\n\ncd\n");
    //    CHECK(3 == buf_byrune(&TestBuf, 4, -1));
    //}

    //TEST(buf_byrune should not skip blank lines for +1)
    //{
    //    set_buffer_text("ab\n\ncd\n");
    //    CHECK(3 == buf_byrune(&TestBuf, 1, 1));
    //}

    //TEST(buf_byrune should move from blank line to non-blank line for -1)
    //{
    //    set_buffer_text("ab\n\ncd\n");
    //    CHECK(1 == buf_byrune(&TestBuf, 3, -1));
    //}

    TEST(buf_byrune should move from blank line to non-blank line for +1)
    {
        set_buffer_text("ab\n\ncd\n");
        CHECK(4 == buf_byrune(&TestBuf, 3, 1));
    }

    /* Movement by Line
     *************************************************************************/

}
