#include <stdc.h>
#include <utf.h>
#include <edit.h>
#include <ctype.h>

#define ATTR_NORMAL   (CLR_BASE03 << 8 | CLR_BASE0)
#define ATTR_SELECTED (CLR_BASE0  << 8 | CLR_BASE03)

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
    if (r == '\t' || r == '\n' || r == RUNE_CRLF)
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

static bool in_selection(Sel sel, size_t off) {
    if (sel.end < sel.beg) {
        size_t temp = sel.beg;
        sel.beg = sel.end;
        sel.end = temp;
    }
    return (sel.beg <= off && off < sel.end);
}

static bool selected(View* view, size_t pos) {
    return in_selection(view->selection, pos);
}

static size_t fill_row(View* view, unsigned row, size_t pos) {
    view_getrow(view, row)->off = pos;
    clearrow(view, row);
    for (size_t x = 0; x < view->ncols;) {
        uint32_t attr = (selected(view, pos) ? ATTR_SELECTED : ATTR_NORMAL);
        Rune r = buf_get(&(view->buffer), pos++);
        x += setcell(view, row, x, attr, r);
        if (buf_iseol(&(view->buffer), pos-1)) break;
    }
    return pos;
}

static void reflow(View* view) {
    size_t pos = view->rows[0]->off;
    for (size_t y = 0; y < view->nrows; y++)
        pos = fill_row(view, y, pos);
}

static unsigned prev_screen_line(View* view, unsigned bol, unsigned off) {
    unsigned pos = bol;
    while (true) {
        unsigned x;
        for (x = 0; x < view->ncols && (pos + x) < off; x++)
            x += runewidth(x, buf_get(&(view->buffer), pos+x));
        if ((pos + x) >= off) break;
        pos += x;
    }
    return pos;
}

static void scroll_up(View* view, unsigned csr, unsigned first) {
    while (csr < first) {
        unsigned bol    = buf_bol(&(view->buffer), first);
        unsigned prevln = (first == bol ? buf_byline(&(view->buffer), bol, -1) : bol);
        prevln = prev_screen_line(view, prevln, first);
        /* delete the last row and shift the others */
        free(view->rows[view->nrows - 1]);
        memmove(&view->rows[1], &view->rows[0], sizeof(Row*) * (view->nrows-1));
        view->rows[0] = calloc(1, sizeof(Row) + (view->ncols * sizeof(UGlyph)));
        view->rows[0]->off = prevln;
        /* fill in row content */
        fill_row(view, 0, view->rows[0]->off);
        first = view->rows[0]->off;
    }
}

static void scroll_dn(View* view, unsigned csr, unsigned last) {
    while (csr > last) {
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
        last = view->rows[view->nrows-1]->off + view->rows[view->nrows-1]->rlen - 1;
    }
}

static void sync_view(View* view, size_t csr) {
    unsigned first = view->rows[0]->off;
    unsigned last  = view->rows[view->nrows-1]->off + view->rows[view->nrows-1]->rlen - 1;
    if (csr < first) {
        scroll_up(view, csr, first);
    } else if (csr > last) {
        scroll_dn(view, csr, last);
    }
    view->sync_needed = false;
}

static size_t getoffset(View* view, size_t row, size_t col) {
    Row* scrrow = view_getrow(view, row);
    assert(scrrow);
    size_t pos = scrrow->off;
    if (col > scrrow->len) {
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
        return buf_end(&(view->buffer))-1;
    return pos;
}

void view_init(View* view, char* file) {
    memset(view, 0, sizeof(View));
    buf_init(&(view->buffer));
    if (file)
        buf_load(&(view->buffer), file);
}

void view_resize(View* view, size_t nrows, size_t ncols) {
    size_t off = 0;
    if (view->nrows == nrows && view->ncols == ncols)  return;
    /* free the old row data */
    if (view->nrows) {
        off = view->rows[0]->off;
        for (unsigned i = 0; i < view->nrows; i++)
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

void view_update(View* view, size_t* csrx, size_t* csry) {
    size_t csr = view->selection.end;
    /* scroll the view and reflow the screen lines */
    reflow(view);
    if (view->sync_needed)
        sync_view(view, csr);
    /* find the cursor on the new screen */
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
                x += runewidth(x, buf_get(&(view->buffer),pos++));
            }
            break;
        }
    }
}

Row* view_getrow(View* view, size_t row) {
    return (row < view->nrows ? view->rows[row] : NULL);
}

void view_byrune(View* view, int move) {
    Sel sel = view->selection;
    sel.beg = sel.end = buf_byrune(&(view->buffer), sel.end, move);
    sel.col = buf_getcol(&(view->buffer), sel.end);
    view->selection = sel;
    view->sync_needed = true;
}

void view_byline(View* view, int move) {
    Sel sel = view->selection;
    sel.beg = sel.end = buf_byline(&(view->buffer), sel.end, move);
    sel.beg = sel.end = buf_setcol(&(view->buffer), sel.end, sel.col);
    view->selection = sel;
    view->sync_needed = true;
}

void view_setcursor(View* view, size_t row, size_t col) {
    size_t off = getoffset(view, row, col);
    if (!in_selection(view->selection, off)) {
        view->selection.beg = view->selection.end = off;
        view->selection.col = buf_getcol(&(view->buffer), view->selection.end);
        sync_view(view, view->selection.end);
    }
}

void view_selext(View* view, size_t row, size_t col) {
    view->selection.end = getoffset(view, row, col);
    sync_view(view, view->selection.end);
}

void view_insert(View* view, Rune rune) {
    if (rune == '\b') {
        if (view->selection.end > 0)
            buf_del(&(view->buffer), --view->selection.end);
    } else {
        /* ignore non-printable control characters */
        if (!isspace(rune) && rune < 0x20)
            return;
        buf_ins(&(view->buffer), view->selection.end++, rune);
    }
    view->selection.beg = view->selection.end;
    view->selection.col = buf_getcol(&(view->buffer), view->selection.end);
    view->sync_needed = true;
}

void view_delete(View* view) {
    //if (SelEnd == buf_end(&Buffer)) return;
    //size_t n = SelEnd - SelBeg;
    //for (size_t i = 0; i < n; i++)
    //    buf_del(&Buffer, SelBeg);
    //SelEnd = SelBeg;
    //TargetCol = buf_getcol(&Buffer, SelEnd);
    //view->sync_needed = true;
}
