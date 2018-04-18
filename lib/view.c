#include <stdc.h>
#include <utf.h>
#include <edit.h>
#include <ctype.h>

/* Provided by x11.c */
extern size_t glyph_width(int c);

#define BUF    (&(view->buffer))
#define CSRPOS (view->buffer.selection.end)

typedef size_t (*movefn_t)(Buf* buf, size_t pos, int count);

static void move_selection(View* view, bool extsel, int move, movefn_t bything) {
    view->sync_flags |= CURSOR;
    if (buf_selsz(BUF) && !extsel) {
        buf_selclr(BUF, move);
    } else {
        CSRPOS = bything(BUF, CSRPOS, move);
        if (bything == buf_byline)
            buf_setcol(BUF);
        if (!extsel)
            buf_selclr(BUF, RIGHT);
    }
    /* only update column if not moving vertically */
    if (bything != buf_byline)
        buf_getcol(BUF);
}

static void move_to(View* view, bool extsel, size_t off) {
    Buf* buf = BUF;
    CSRPOS = (off > buf_end(buf) ? buf_end(buf) : off);
    if (!extsel)
        buf_selclr(BUF, RIGHT);
    buf_getcol(buf);
    view->sync_flags |= CURSOR;
}

static bool selection_visible(View* view) {
    if (!view->nrows) return true;
    size_t csr = CSRPOS;
    size_t beg = view->rows[0]->off;
    size_t end = view->rows[view->nrows-1]->off +
                 view->rows[view->nrows-1]->len;
    return (beg <= csr && csr <= end);
}

static void find_cursor(View* view, size_t* csrx, size_t* csry) {
    size_t csr = CSRPOS;
    for (size_t y = 0; y < view->nrows; y++) {
        size_t start = view->rows[y]->off;
        size_t end   = view->rows[y]->off + view->rows[y]->len - 1;
        if (start <= csr && csr <= end) {
            size_t pos = start;
            break;
        }
    }
}

static Sel* getsel(View* view) {
    return &(view->buffer.selection);
}

void view_init(View* view, char* file) {
    if (view->nrows) {
        for (size_t i = 0; i < view->nrows; i++)
            free(view->rows[i]);
        free(view->rows);
        view->nrows = 0;
        view->rows  = NULL;
    }
    view->sync_flags |= (CURSOR|CENTER);
    /* load the file and jump to the address returned from the load function */
    buf_init(BUF);
    if (file) buf_load(BUF, file);
}

void view_reload(View* view) {
    if (view->buffer.path) {
        buf_reload(BUF);
        view->sync_flags |= (CURSOR|CENTER);
    }
}

size_t view_limitrows(View* view, size_t maxrows, size_t ncols) {
    return 1;
}

size_t rune_width(int c, size_t xpos, size_t width) {
    if (c == '\r')
        return 0;
    else if (c == '\n')
        return (width-xpos);
    else if (c == '\t')
        return (glyph_width(c) - (xpos % glyph_width(c)));
    else
        return glyph_width(c);
}

void view_resize(View* view, size_t width, size_t nrows) {
    /* free up the old data */
    if (view->rows) {
        for (size_t i = 0; i < view->nrows; i++)
            free(view->rows[i]);
        free(view->rows);
        view->rows = NULL;
        view->nrows = 0;
    }
    size_t off = 0;
    /* start from beginning of first line and populate row by row */
    for (size_t i = 0; i < nrows; i++) {
        view->nrows++;
        view->rows = realloc(view->rows, sizeof(Row*) * view->nrows);
        view->rows[view->nrows-1] = calloc(1, sizeof(Row));
        view->rows[view->nrows-1]->off = off;

        size_t xpos = 0;
        while (xpos < width) {
            int rune = buf_getrat(&(view->buffer), off);
            size_t rwidth = rune_width(rune, xpos, width);
            xpos += rwidth;
            if (xpos <= width) {
                size_t len = view->rows[view->nrows-1]->len + 1;
                view->rows[view->nrows-1] = realloc(
                    view->rows[view->nrows-1], sizeof(Row) + (len * sizeof(UGlyph)));
                view->rows[view->nrows-1]->len = len;
                view->rows[view->nrows-1]->cols[len-1].rune  = rune;
                view->rows[view->nrows-1]->cols[len-1].width = rwidth;
                off = buf_byrune(&(view->buffer), off, RIGHT);
            }
        }
    }
}

void view_update(View* view, size_t* csrx, size_t* csry) {
}

Row* view_getrow(View* view, size_t row) {
    return (row < view->nrows ? view->rows[row] : NULL);
}

void view_byrune(View* view, int move, bool extsel) {
    move_selection(view, extsel, move, buf_byrune);
}

void view_byword(View* view, int move, bool extsel) {
    move_selection(view, extsel, move, buf_byword);
}

void view_byline(View* view, int move, bool extsel) {
    move_selection(view, extsel, move, buf_byline);
}

void view_setcursor(View* view, size_t row, size_t col, bool extsel) {
}

void view_selword(View* view, size_t row, size_t col) {
    if (row != SIZE_MAX && col != SIZE_MAX)
        view_setcursor(view, row, col, false);
    buf_selword(BUF, risbigword);
}

void view_selprev(View* view) {
    if (!view_selsize(view))
        buf_lastins(BUF);
    else
        buf_selclr(BUF, RIGHT);
}

void view_select(View* view, size_t row, size_t col) {
    view_setcursor(view, row, col, false);
    buf_selctx(BUF, risword);
}

size_t view_selsize(View* view) {
    return buf_selsz(BUF);
}

char* view_fetch(View* view, size_t row, size_t col, bool (*isword)(Rune)) {
    char* str = NULL;
//    size_t off = getoffset(view, row, col);
//    if (off != SIZE_MAX)
//        str = buf_fetch(BUF, isword, off);
    return str;
}

bool view_findstr(View* view, int dir, char* str) {
    bool found = buf_findstr(BUF, dir, str);
    view->sync_flags |= (CURSOR|CENTER);
    return found;
}

void view_insert(View* view, bool indent, Rune rune) {
    /* ignore non-printable control characters */
    if (!isspace(rune) && (rune >= 0 && rune < 0x20))
        return;
    buf_putc(BUF, rune);
    move_to(view, false, CSRPOS);
}

void view_delete(View* view, int dir, bool byword) {
    if (!view_selsize(view))
        (byword ? view_byword : view_byrune)(view, dir, true);
    buf_del(BUF);
    move_to(view, false, CSRPOS);
}

void view_jumpto(View* view, bool extsel, size_t off) {
    move_to(view, extsel, off);
}

void view_bol(View* view, bool extsel) {
    /* determine whether we are jumping to start of content or line */
    Buf* buf = BUF;
    unsigned bol = buf_bol(buf, CSRPOS);
    unsigned boi = bol;
    for (; ' '  == buf_getrat(buf, boi) || '\t' == buf_getrat(buf, boi); boi++);
    unsigned pos = CSRPOS;
    pos = (pos == bol || pos > boi ? boi : bol);
    move_to(view, extsel, pos);
}

void view_eol(View* view, bool extsel) {
    move_to(view, extsel, buf_eol(BUF, CSRPOS));
    getsel(view)->col = -1; // Peg cursor to line end
}

void view_bof(View* view, bool extsel) {
    view_jumpto(view, extsel, 0);
}

void view_eof(View* view, bool extsel) {
    view_jumpto(view, extsel, buf_end(BUF));
}

void view_setln(View* view, size_t line) {
    buf_setln(BUF, line);
    view->sync_flags |= CENTER;
}

void view_undo(View* view) {
    buf_undo(BUF);
    view->sync_flags |= CURSOR;
    if (!selection_visible(view)) view->sync_flags |= CENTER;
}

void view_redo(View* view) {
    buf_redo(BUF);
    view->sync_flags |= CURSOR;
    if (!selection_visible(view)) view->sync_flags |= CENTER;
}

void view_putstr(View* view, char* str) {
    buf_puts(BUF, str);
}

char* view_getstr(View* view) {
    return buf_gets(BUF);
}

char* view_getcmd(View* view) {
    if (!view_selsize(view))
        buf_selctx(BUF, riscmd);
    return view_getstr(view);
}

void view_selctx(View* view) {
    if (!view_selsize(view))
        buf_selctx(BUF, risword);
}

char* view_getctx(View* view) {
    view_selctx(view);
    return view_getstr(view);
}

void view_scroll(View* view, int move) {
    int dir = (move < 0 ? -1 : 1);
    move *= dir;
    for (int i = 0; i < move; i++) {
//        if (dir < 0)
//            scroll_up(view);
//        else
//            scroll_dn(view);
    }
}

void view_scrollpage(View* view, int move) {
    move = (move < 0 ? -1 : 1) * view->nrows;
    view_scroll(view, move);
}

Rune view_getrune(View* view) {
    return buf_getc(BUF);
}

void view_scrollto(View* view, size_t csr) {
}

void view_selectall(View* view) {
    buf_selall(BUF);
    view->sync_flags |= CURSOR;
}

void view_selectobj(View* view, bool (*istype)(Rune)) {
    buf_selword(BUF, istype);
    view->sync_flags |= CURSOR;
}


