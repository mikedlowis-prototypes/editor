#include <stdc.h>
#include <utf.h>
#include <edit.h>
#include <ctype.h>

typedef size_t (*movefn_t)(Buf* buf, size_t pos, int count);

static void move_selection(View* view, bool extsel, int move, movefn_t bything);
static void move_to(View* view, bool extsel, size_t off);
static bool selection_visible(View* view);
static void find_cursor(View* view, size_t* csrx, size_t* csry);
static void clearrow(View* view, size_t row);
static size_t setcell(View* view, size_t row, size_t col, uint32_t attr, Rune r);
static size_t fill_row(View* view, unsigned row, size_t pos);
static unsigned prev_screen_line(View* view, unsigned bol, unsigned off);
static unsigned scroll_up(View* view);
static unsigned scroll_dn(View* view);
static void sync_center(View* view, size_t csr);
static size_t getoffset(View* view, size_t row, size_t col);

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
    view->sync_needed = true;
    view->sync_center = true;
    /* load the file and jump to the address returned from the load function */
    buf_init(&(view->buffer));
    if (file) buf_load(&(view->buffer), file);
}

void view_reload(View* view) {
    if (view->buffer.path) {
        buf_reload(&(view->buffer));
        view->sync_needed = true;
        view->sync_center = true;
    }
}

size_t view_limitrows(View* view, size_t maxrows, size_t ncols) {
    size_t nrows = 1, pos = 0, col = 0;
    while (nrows < maxrows && pos < buf_end(&(view->buffer))) {
        Rune r = buf_getrat(&(view->buffer), pos++);
        col += runewidth(col, r);
        if (col >= ncols || r == '\n')
            col = 0, nrows++;
    }
    return nrows;
}

void view_resize(View* view, size_t nrows, size_t ncols) {
    size_t off = 0;
    if (view->nrows == nrows && view->ncols == ncols) return;
    /* free the old row data */
    if (view->nrows) {
        off  = view->rows[0]->off;
        for (size_t i = 0; i < view->nrows; i++)
            free(view->rows[i]);
        free(view->rows);
    }
    /* create the new row data */
    view->rows = calloc(nrows, sizeof(Row*));
    for (unsigned i = 0; i < nrows; i++)
        view->rows[i] = calloc(1, sizeof(Row) + (ncols * sizeof(UGlyph)));
    /* update dimensions */
    view->rows[0]->off = off;
    view->nrows = nrows;
    view->ncols = ncols;
}

void view_update(View* view, int clrnor, int clrsel, size_t* csrx, size_t* csry) {
    if (!view->nrows) return;
    view->clrnor = clrnor, view->clrsel = clrsel;
    size_t csr = getsel(view)->end;
    /* scroll the view and reflow the screen lines */
    size_t pos = view->rows[0]->off;
    /* fill the view and scroll if needed */
    for (size_t y = 0; y < view->nrows; y++)
        pos = fill_row(view, y, pos);
    if (view->sync_needed)
        view_scrollto(view, csr);
    /* locate the cursor if visible */
    find_cursor(view, csrx, csry);
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
    size_t off = getoffset(view, row, col);
    if (off != SIZE_MAX)
        view_jumpto(view, extsel, off);
}

void view_selword(View* view, size_t row, size_t col) {
    if (row != SIZE_MAX && col != SIZE_MAX)
        view_setcursor(view, row, col, false);
    buf_selword(&(view->buffer), risbigword, NULL);
}

void view_selprev(View* view) {
    if (!view_selsize(view))
        buf_lastins(&(view->buffer), NULL);
    else
        buf_selclr(&(view->buffer), NULL, RIGHT);
}

void view_select(View* view, size_t row, size_t col) {
    view_setcursor(view, row, col, false);
    buf_selctx(&(view->buffer), risword, NULL);
}

size_t view_selsize(View* view) {
    return buf_selsz(&(view->buffer), NULL);
}

char* view_fetch(View* view, size_t row, size_t col, bool (*isword)(Rune)) {
   char* str = NULL;
    size_t off = getoffset(view, row, col);
    if (off != SIZE_MAX) {
        /* str = buf_fetchat(buf, isword, off) */
//        Sel sel = { .beg = off, .end = off };
//        if (buf_insel(&(view->buffer), NULL, off))
//            sel = *(getsel(view));
//        else
//            buf_selword(&(view->buffer), isword, &sel);
//        str = view_getstr(view, &sel);
    }
    return str;
}

bool view_findstr(View* view, int dir, char* str) {
    bool found = buf_findstr(&(view->buffer), NULL, dir, str);
    view->sync_needed = true;
    view->sync_center = true;
    return found;
}

void view_insert(View* view, bool indent, Rune rune) {
    /* ignore non-printable control characters */
    if (!isspace(rune) && (rune >= 0 && rune < 0x20))
        return;
    buf_putc(&(view->buffer), rune, NULL);
    move_to(view, false, getsel(view)->end);
}

void view_delete(View* view, int dir, bool byword) {
    if (!view_selsize(view))
        (byword ? view_byword : view_byrune)(view, dir, true);
    buf_del(&(view->buffer), NULL);
    move_to(view, false, getsel(view)->end);
}

void view_jumpto(View* view, bool extsel, size_t off) {
    move_to(view, extsel, off);
}

void view_bol(View* view, bool extsel) {
    /* determine whether we are jumping to start of content or line */
    Buf* buf = &(view->buffer);
    unsigned bol = buf_bol(buf, getsel(view)->end);
    unsigned boi = bol;
    for (; ' '  == buf_getrat(buf, boi) || '\t' == buf_getrat(buf, boi); boi++);
    unsigned pos = getsel(view)->end;
    pos = (pos == bol || pos > boi ? boi : bol);
    move_to(view, extsel, pos);
}

void view_eol(View* view, bool extsel) {
    move_to(view, extsel, buf_eol(&(view->buffer), getsel(view)->end));
    getsel(view)->col = -1; // Peg cursor to line end
}

void view_bof(View* view, bool extsel) {
    view_jumpto(view, extsel, 0);
}

void view_eof(View* view, bool extsel) {
    view_jumpto(view, extsel, buf_end(&(view->buffer)));
}

void view_setln(View* view, size_t line) {
    buf_setln(&(view->buffer), NULL, line);
    view->sync_center = true;
}

void view_undo(View* view) {
    buf_undo(&(view->buffer), NULL);
    view->sync_needed = true;
    view->sync_center = !selection_visible(view);
}

void view_redo(View* view) {
    buf_redo(&(view->buffer), NULL);
    view->sync_needed = true;
    view->sync_center = !selection_visible(view);
}

void view_putstr(View* view, char* str) {
    buf_puts(&(view->buffer), str, NULL);
}

char* view_getstr(View* view) {
    return buf_gets(&(view->buffer), NULL);
}

char* view_getcmd(View* view) {
    if (!view_selsize(view))
        buf_selctx(&(view->buffer), riscmd, NULL);
    return view_getstr(view);
}

void view_selctx(View* view) {
    if (!view_selsize(view))
        buf_selctx(&(view->buffer), risword, NULL);
}

char* view_getctx(View* view) {
    view_selctx(view);
    return view_getstr(view);
}

void view_scroll(View* view, int move) {
    int dir = (move < 0 ? -1 : 1);
    move *= dir;
    for (int i = 0; i < move; i++) {
        if (dir < 0)
            scroll_up(view);
        else
            scroll_dn(view);
    }
}

void view_scrollpage(View* view, int move) {
    move = (move < 0 ? -1 : 1) * view->nrows;
    view_scroll(view, move);
}

Rune view_getrune(View* view) {
    return buf_getc(&(view->buffer), NULL);
}

void view_scrollto(View* view, size_t csr) {
    if (!view->nrows) return;
    unsigned first = view->rows[0]->off;
    unsigned last  = view->rows[view->nrows-1]->off + view->rows[view->nrows-1]->rlen - 1;
    while (csr < first)
        first = scroll_up(view);
    while (csr > last && last < buf_end(&(view->buffer)))
        last = scroll_dn(view);
    view->sync_needed = false;
    if (view->sync_center) {
        sync_center(view, csr);
        view->sync_center = false;
    }
}

void view_selectall(View* view) {
    buf_selall(&(view->buffer), NULL);
    view->sync_needed = true;
}

void view_selectobj(View* view, bool (*istype)(Rune)) {
    buf_selword(&(view->buffer), istype, NULL);
    view->sync_needed = true;
}

static void move_selection(View* view, bool extsel, int move, movefn_t bything) {
    view->sync_needed = true;
    if (buf_selsz(&(view->buffer), NULL) && !extsel) {
        buf_selclr(&(view->buffer), NULL, move);
    } else {
        Sel* sel = getsel(view);
        sel->end = bything(&(view->buffer), sel->end, move);
        if (bything == buf_byline)
            buf_setcol(&(view->buffer), NULL);
        if (!extsel) buf_selclr(&(view->buffer), sel, move);
    }
    /* only update column if not moving vertically */
    if (bything != buf_byline)
        buf_getcol(&(view->buffer), NULL);
}

static void move_to(View* view, bool extsel, size_t off) {
    Buf* buf = &(view->buffer);
    getsel(view)->end = (off > buf_end(buf) ? buf_end(buf) : off);
    if (!extsel)
        getsel(view)->beg = getsel(view)->end;
    buf_getcol(buf, NULL);
    view->sync_needed = true;
}

static bool selection_visible(View* view) {
    if (!view->nrows) return true;
    size_t csr = getsel(view)->end;
    size_t beg = view->rows[0]->off;
    size_t end = view->rows[view->nrows-1]->off +
                 view->rows[view->nrows-1]->rlen;
    return (beg <= csr && csr <= end);
}

static void find_cursor(View* view, size_t* csrx, size_t* csry) {
    size_t csr = getsel(view)->end;
    for (size_t y = 0; y < view->nrows; y++) {
        size_t start = view->rows[y]->off;
        size_t end   = view->rows[y]->off + view->rows[y]->rlen - 1;
        if (start <= csr && csr <= end) {
            size_t pos = start;
            for (size_t x = 0; x < view->ncols;) {
                if (pos == csr) {
                    *csry = y, *csrx = x;
                    break;
                }
                x += runewidth(x, buf_getrat(&(view->buffer), pos++));
            }
            break;
        }
    }
}

static void clearrow(View* view, size_t row) {
    Row* scrrow = view_getrow(view, row);
    if (!scrrow) return;
    for (size_t i = 0; i < view->ncols; i++)
        scrrow->cols[i].rune = (Rune)' ';
    scrrow->rlen = 0;
    scrrow->len  = 0;
}

static size_t setcell(View* view, size_t row, size_t col, uint32_t attr, Rune r) {
    if (row >= view->nrows || col >= view->ncols) return 0;
    Row* scrrow = view_getrow(view, row);
    int ncols = runewidth(col, r);
    /* write the rune to the screen buf */
    scrrow->cols[col].attr = attr;
    if (r == '\t')
        scrrow->cols[col].rune = ' ';
    else
        scrrow->cols[col].rune = r;
    /* Update lengths */
    scrrow->rlen += 1;
    for (int i = 1; i < ncols; i++) {
        scrrow->cols[col].attr = attr;
        scrrow->cols[col+i].rune = '\0';
    }
    if ((col + ncols) > scrrow->len)
        scrrow->len = col + ncols;
    return ncols;
}

static size_t fill_row(View* view, unsigned row, size_t pos) {
    view_getrow(view, row)->off  = pos;
    clearrow(view, row);
    for (size_t x = 0; x < view->ncols;) {
        uint32_t attr = (buf_insel(&(view->buffer), NULL, pos) ? view->clrsel : view->clrnor);
        Rune r = buf_getrat(&(view->buffer), pos++);
        x += setcell(view, row, x, attr, r);
        if (buf_iseol(&(view->buffer), pos-1)) {
            break;
        }
    }
    return pos;
}

static unsigned prev_screen_line(View* view, unsigned bol, unsigned off) {
    unsigned pos = bol;
    while (true) {
        unsigned x;
        for (x = 0; x < view->ncols && (pos + x) < off; x++)
            x += runewidth(x, buf_getrat(&(view->buffer), pos+x));
        if ((pos + x) >= off) break;
        pos += x;
    }
    return pos;
}

static unsigned scroll_up(View* view) {
    size_t first   = view->rows[0]->off;
    size_t bol     = buf_bol(&(view->buffer), first);
    size_t prevln  = (first == bol ? buf_byline(&(view->buffer), bol, UP) : bol);
    if (!first) return first;
    prevln = prev_screen_line(view, prevln, first);
    /* delete the last row and shift the others */
    free(view->rows[view->nrows - 1]);
    memmove(&view->rows[1], &view->rows[0], sizeof(Row*) * (view->nrows-1));
    view->rows[0] = calloc(1, sizeof(Row) + (view->ncols * sizeof(UGlyph)));
    view->rows[0]->off = prevln;
    /* fill in row content */
    fill_row(view, 0, view->rows[0]->off);
    return view->rows[0]->off;
}

static unsigned scroll_dn(View* view) {
    size_t last = view->rows[view->nrows-1]->off + view->rows[view->nrows-1]->rlen - 1;
    if (last >= buf_end(&(view->buffer))) return last;
    /* delete the first row and shift the others */
    if (view->nrows > 1) {
        free(view->rows[0]);
        memmove(&view->rows[0], &view->rows[1], sizeof(Row*) * (view->nrows-1));
        view->rows[view->nrows-1] = calloc(1, sizeof(Row) + (view->ncols * sizeof(UGlyph)));
        view->rows[view->nrows-1]->off = (view->rows[view->nrows-2]->off + view->rows[view->nrows-2]->rlen);
        /* fill in row content */
        fill_row(view, view->nrows-1, view->rows[view->nrows-1]->off);
    } else {
        view->rows[0]->off += view->rows[0]->rlen;
        fill_row(view, 0, view->rows[0]->off);
    }
    return view->rows[view->nrows-1]->off + view->rows[view->nrows-1]->rlen - 1;
}

static void sync_center(View* view, size_t csr) {
    /* determine the screenline containing the cursor */
    size_t scrln = 0;
    for (; scrln < view->nrows; scrln++) {
        unsigned first = view->rows[scrln]->off;
        unsigned last  = first + view->rows[scrln]->rlen - 1;
        if (csr >= first && csr <= last)
            break;
    }
    /* find the middle row and scroll until the cursor is on that row */
    unsigned midrow = view->nrows / 2;
    int move = (scrln - midrow);
    unsigned count = (move < 0 ? -move : move);
    for (; count > 0; count--)
        (move < 0 ? scroll_up : scroll_dn)(view);
}

static size_t getoffset(View* view, size_t row, size_t col) {
    Row* scrrow = view_getrow(view, row);
    if (!scrrow) return SIZE_MAX;
    size_t pos = scrrow->off;
    if (col >= scrrow->len) {
        pos = (scrrow->off + scrrow->rlen - 1);
    } else {
        /* multi column runes are followed by \0 slots so if we clicked on a \0
           slot, slide backwards to the real rune. */
        for (; !scrrow->cols[col].rune && col > 0; col--);
        /* now lets count the number of runes up to the one we clicked on */
        for (unsigned i = 0; i < col; i++)
            if (scrrow->cols[i].rune)
                pos++;
    }
    if (pos >= buf_end(&(view->buffer)))
        return buf_end(&(view->buffer));
    return pos;
}
