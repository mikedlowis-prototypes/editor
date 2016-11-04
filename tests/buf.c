#include <atf.h>
#include <stdc.h>
#include <utf.h>
#include <edit.h>

static Buf TestBuf;

static void buf_clr(Buf* buf) {
    //while (buf->undo) {
    //    Log* deadite = buf->undo;
    //    buf->undo = deadite->next;
    //    if (!deadite->insert)
    //        free(deadite->data.del.runes);
    //    free(deadite);
    //}
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
    /* Undo/Redo
     *************************************************************************/
    /* Locking
     *************************************************************************/
    TEST(buf_setlocked should lock the buffer to prevent changes) {
        TestBuf.locked = false;
        if (TestBuf.undo) { free(TestBuf.undo); TestBuf.undo = NULL; }
        buf_setlocked(&TestBuf, true);
        CHECK(TestBuf.locked);
    }

    TEST(buf_setlocked should lock the buffer to prevent changes and lock the last undo op) {
        Log log;
        TestBuf.locked = false;
        TestBuf.undo = &log;
        buf_setlocked(&TestBuf, true);
        CHECK(TestBuf.locked);
        CHECK(TestBuf.undo->locked);
    }

    TEST(buf_setlocked should unlock the buffer) {
        Log log;
        TestBuf.locked = true;
        TestBuf.undo = &log;
        buf_setlocked(&TestBuf, false);
        CHECK(!TestBuf.locked);
    }

    TEST(buf_islocked should return true if locked) {
        TestBuf.locked = true;
        CHECK(buf_locked(&TestBuf));
    }

    TEST(buf_islocked should return false if locked) {
        TestBuf.locked = false;
        CHECK(!buf_locked(&TestBuf));
    }

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
    TEST(buf_eol should return 2 if column 1 of first line) {
        set_buffer_text("ab\ncd");
        CHECK(2 == buf_eol(&TestBuf, 0));
    }

    TEST(buf_eol should return 2 if column 2 of first line) {
        set_buffer_text("ab\ncd");
        CHECK(2 == buf_eol(&TestBuf, 1));
    }

    TEST(buf_eol should return 2 if column 3 of first line) {
        set_buffer_text("ab\ncd");
        CHECK(2 == buf_eol(&TestBuf, 2));
    }

    TEST(buf_eol should return 5 if column 1 of second line) {
        set_buffer_text("ab\ncd");
        CHECK(5 == buf_eol(&TestBuf, 3));
    }

    TEST(buf_eol should return 5 if column 2 of second line) {
        set_buffer_text("ab\ncd");
        CHECK(5 == buf_eol(&TestBuf, 4));
    }

    TEST(buf_eol should return 5 if column 3 of second line) {
        set_buffer_text("ab\ncd");
        CHECK(5 == buf_eol(&TestBuf, 5));
    }

    // Start of Word
    TEST(buf_bow should return input when pointing to whitespace before word) {
        set_buffer_text(" abc ");
        CHECK(0 == buf_bow(&TestBuf, 0));
    }

    TEST(buf_bow should return 1 when first rune of word) {
        set_buffer_text(" abc ");
        CHECK(1 == buf_bow(&TestBuf, 1));
    }

    TEST(buf_bow should return 1 when second rune of word) {
        set_buffer_text(" abc ");
        CHECK(1 == buf_bow(&TestBuf, 2));
    }

    TEST(buf_bow should return 1 when third rune of word) {
        set_buffer_text(" abc ");
        CHECK(1 == buf_bow(&TestBuf, 3));
    }

    TEST(buf_bow should return input when pointing to whitespace after word) {
        set_buffer_text(" abc ");
        CHECK(4 == buf_bow(&TestBuf, 4));
    }

    // End of Word
    TEST(buf_eow should return input when pointing to whitespace before word) {
        set_buffer_text(" abc ");
        CHECK(0 == buf_eow(&TestBuf, 0));
    }

    TEST(buf_eow should return 3 when first rune of word) {
        set_buffer_text(" abc ");
        CHECK(3 == buf_eow(&TestBuf, 1));
    }

    TEST(buf_eow should return 3 when second rune of word) {
        set_buffer_text(" abc ");
        CHECK(3 == buf_eow(&TestBuf, 2));
    }

    TEST(buf_eow should return 3 when third rune of word) {
        set_buffer_text(" abc ");
        CHECK(3 == buf_eow(&TestBuf, 3));
    }

    TEST(buf_eow should return input when pointing to whitespace after word) {
        set_buffer_text(" abc ");
        CHECK(4 == buf_eow(&TestBuf, 4));
    }

    // Scan Left
    TEST(buf_lscan should return location of token to the left) {
        set_buffer_text("a{bc}");
        CHECK(1 == buf_lscan(&TestBuf, 4, '{'));
    }

    TEST(buf_lscan should return input location if token not found) {
        set_buffer_text("{ab}");
        CHECK(3 == buf_lscan(&TestBuf, 3, '['));
    }

    // Scan Right
    TEST(buf_rscan should return location of token to the right) {
        set_buffer_text("{ab}c");
        CHECK(3 == buf_rscan(&TestBuf, 0, '}'));
    }

    TEST(buf_rscan should return input location if token not found) {
        set_buffer_text("{ab}c");
        CHECK(0 == buf_rscan(&TestBuf, 0, ']'));
    }

    // By Rune
    TEST(buf_byrune should do nothing for -1 at beginning of file) {
        set_buffer_text("abc\n");
        CHECK(0 == buf_byrune(&TestBuf, 0, -1));
    }

    TEST(buf_byrune should move to first rune for -1 at second rune of file) {
        set_buffer_text("abc\n");
        CHECK(0 == buf_byrune(&TestBuf, 1, -2));
    }

    TEST(buf_byrune should move to just after last rune for +1 at end of file) {
        set_buffer_text("abc\n");
        CHECK(4 == buf_byrune(&TestBuf, 3, 2));
    }

    TEST(buf_byrune should move to just after last rune for +2 at second to last rune) {
        set_buffer_text("abc\n");
        CHECK(4 == buf_byrune(&TestBuf, 2, 3));
    }

    TEST(buf_byrune should move from blank line to non-blank line for +1) {
        set_buffer_text("ab\n\ncd\n");
        CHECK(4 == buf_byrune(&TestBuf, 3, 1));
    }

    // By Line
    TEST(buf_byline should not move before first line) {
        set_buffer_text("ab\n\ncd\n");
        CHECK(0 == buf_byline(&TestBuf, 0, -1));
    }

    TEST(buf_byline should not move before first line) {
        set_buffer_text("a\nb\nc\nd\n");
        CHECK(0 == buf_byline(&TestBuf, 7, -10));
    }

    TEST(buf_byline should move back multiple lines) {
        set_buffer_text("a\nb\nc\nd\n");
        CHECK(2 == buf_byline(&TestBuf, 7, -2));
    }

    TEST(buf_byline should move back a line) {
        set_buffer_text("abc\ndef");
        CHECK(0 == buf_byline(&TestBuf, 4, -1));
    }

    TEST(buf_byline should move forward a line) {
        set_buffer_text("abc\ndef");
        CHECK(4 == buf_byline(&TestBuf, 2, 1));
    }

    TEST(buf_byline should not move after last line) {
        set_buffer_text("abc\ndef");
        CHECK(6 == buf_byline(&TestBuf, 6, 1));
    }

    TEST(buf_byline should do nothing at end of buffer) {
        set_buffer_text("abc\ndef");
        CHECK(7 == buf_byline(&TestBuf, buf_end(&TestBuf), 1));
    }

    /* Literal Find
     *************************************************************************/
    TEST(buf_find should find next occurrence of the selection) {
        set_buffer_text("foofodfoo");
        unsigned beg = 0, end = 2;
        buf_find(&TestBuf, &beg, &end);
        CHECK(beg == 6);
        CHECK(end == 8);
    }

    TEST(buf_find should wrap around to beginning of file) {
        set_buffer_text("foobarfoo");
        unsigned beg = 6, end = 8;
        buf_find(&TestBuf, &beg, &end);
        CHECK(beg == 0);
        CHECK(end == 2);
    }

    /* Cursor Column Tracking
     *************************************************************************/
    TEST(buf_getcol should return the column associated with the position) {
        set_buffer_text("abcdef");
        CHECK(4 == buf_getcol(&TestBuf, 4));
    }

    TEST(buf_getcol should return the column associated with the position on second line) {
        set_buffer_text("abcdef\nabcdef");
        CHECK(0 == buf_getcol(&TestBuf, 7));
    }

    TEST(buf_getcol should handle tab characters) {
        set_buffer_text("\tabcdef");
        CHECK(4 == buf_getcol(&TestBuf, 1));
    }

    TEST(buf_setcol should set the column to column 1 of second line) {
        set_buffer_text("abc\ndef");
        CHECK(4 == buf_setcol(&TestBuf, 4, 0));
    }

    TEST(buf_setcol should set the column to column 2 of second line) {
        set_buffer_text("abc\ndef");
        CHECK(5 == buf_setcol(&TestBuf, 4, 1));
    }

    TEST(buf_setcol should handle tabs) {
        set_buffer_text("abc\n\tdef");
        CHECK(5 == buf_setcol(&TestBuf, 4, 4));
    }

    TEST(buf_setcol should not set column past the last rune) {
        set_buffer_text("abc\n\tdef");
        CHECK(8 == buf_setcol(&TestBuf, 4, 100));
    }
}
