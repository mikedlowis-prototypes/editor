#include <stdc.h>
#include <utf.h>
#include <edit.h>

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

static bool selected(View* view, size_t pos) {
    Sel sel = view->selection;
    if (sel.end < sel.beg) {
        size_t temp = sel.beg;
        sel.beg = sel.end;
        sel.end = temp;
    }
    return (sel.beg <= pos && pos < sel.end);
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

void view_init(View* view, char* file) {
    memset(view, 0, sizeof(View));
    buf_init(&(view->buffer));
    if (file)
        buf_load(&(view->buffer), file);
}

void view_resize(View* view, size_t nrows, size_t ncols) {
    if (view->nrows == nrows && view->ncols == ncols)  return;
    /* free the old row data */
    if (view->nrows) {
        for (unsigned i = 0; i < view->nrows; i++)
            free(view->rows[i]);
        free(view->rows);
    }
    /* create the new row data */
    view->rows = calloc(nrows, sizeof(Row*));
    for (unsigned i = 0; i < nrows; i++)
        view->rows[i] = calloc(1, sizeof(Row) + (ncols * sizeof(UGlyph)));
    /* update dimensions */
    view->nrows = nrows;
    view->ncols = ncols;
    /* populate the screen buffer */
    reflow(view);
}

void view_update(View* view, size_t* csrx, size_t* csry) {
    size_t csr = view->selection.beg;
    /* scroll the view and reflow the screen lines */
    //sync_view(buf, csr);
    reflow(view);
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
}

void view_byline(View* view, int move) {
    Sel sel = view->selection;
    sel.beg = sel.end = buf_byline(&(view->buffer), sel.end, move);
    sel.beg = sel.end = buf_setcol(&(view->buffer), sel.end, sel.col);
    view->selection = sel;
}


//void view_update(View* view, size_t crsr, size_t* csrx, size_t* csry) {
//
//}
//
//size_t view_getoff(View* view, size_t pos, size_t row, size_t col) {
//    return 0;
//}
//
//void view_setsize(View* view, size_t nrows, size_t ncols) {
//
//}
//
//void view_getsize(View* view, size_t* nrows, size_t* ncols) {
//
//}
//
//void view_clearrow(View* view, size_t row) {
//
//}
//
//size_t view_setcell(View* view, size_t row, size_t col, uint32_t attr, Rune r) {
//    return 0;
//}
//
//UGlyph* view_getglyph(View* view, size_t row, size_t col, size_t* scrwidth) {
//    return NULL;
//}
