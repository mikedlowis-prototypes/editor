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

static void selswap(Sel* sel) {
    if (sel->end < sel->beg) {
        size_t temp = sel->beg;
        sel->beg = sel->end;
        sel->end = temp;
    }
}

static size_t num_selected(Sel sel) {
    selswap(&sel);
    return (sel.end - sel.beg);
}

static bool in_selection(Sel sel, size_t off) {
    selswap(&sel);
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

static unsigned scroll_up(View* view) {
    unsigned first = view->rows[0]->off;
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
    return view->rows[0]->off;
}

static unsigned scroll_dn(View* view) {
    unsigned last  = view->rows[view->nrows-1]->off + view->rows[view->nrows-1]->rlen - 1;
    if (last >= buf_end(&(view->buffer)))
        return last;
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

static void sync_view(View* view, size_t csr) {
    unsigned first = view->rows[0]->off;
    unsigned last  = view->rows[view->nrows-1]->off + view->rows[view->nrows-1]->rlen - 1;
    while (csr < first)
        first = scroll_up(view);
    while (csr > last)
        last = scroll_dn(view);
    view->sync_needed = false;
}

static size_t getoffset(View* view, size_t row, size_t col) {
    Row* scrrow = view_getrow(view, row);
    if (!scrrow) return SIZE_MAX;
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
        return buf_end(&(view->buffer));
    return pos;
}

void view_init(View* view, char* file) {
    memset(view, 0, sizeof(View));
    buf_init(&(view->buffer));
    if (file)
        buf_load(&(view->buffer), file);
}

size_t view_limitrows(View* view, size_t maxrows, size_t ncols) {
    size_t nrows = 0;
    size_t pos = 0;
    while (nrows < maxrows && pos < buf_end(&(view->buffer))) {
        for (size_t x = 0; x < ncols;) {
            Rune r = buf_get(&(view->buffer), pos++);
            x += runewidth(x, r);
            if (buf_iseol(&(view->buffer), pos)) {
                nrows++;
                break;
            }
        }
    }
    return (!nrows ? 1 : nrows);
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
    if (off != SIZE_MAX) {
        view->selection.beg = view->selection.end = off;
        view->selection.col = buf_getcol(&(view->buffer), view->selection.end);
        view->sync_needed = true;
    }
}

void view_selext(View* view, size_t row, size_t col) {
    size_t off = getoffset(view, row, col);
    if (off != SIZE_MAX) {
        view->selection.end = off;
        sync_view(view, view->selection.end);
    }
}

static void selbigword(View* view, Sel* sel) {
    Buf* buf = &(view->buffer);
    size_t mbeg = sel->end, mend = sel->end;
    for (; !risblank(buf_get(buf, mbeg-1)); mbeg--);
    for (; !risblank(buf_get(buf, mend));   mend++);
    sel->beg = mbeg, sel->end = mend-1;
}

static void selcontext(View* view, Sel* sel) {
    Buf* buf = &(view->buffer);
    size_t bol = buf_bol(buf, sel->end);
    Rune r = buf_get(buf, sel->end);
    if (sel->end == bol || r == '\n' || r == RUNE_CRLF) {
        sel->beg = bol;
        sel->end = buf_eol(buf, sel->end);
    } else if (risword(r)) {
        sel->beg = buf_bow(buf, sel->end);
        sel->end = buf_eow(buf, sel->end++);
    } else if (r == '(' || r == ')') {
        sel->beg = buf_lscan(buf, sel->end,   '(');
        sel->end = buf_rscan(buf, sel->end++, ')');
        sel->beg++, sel->end--;
    } else if (r == '[' || r == ']') {
        sel->beg = buf_lscan(buf, sel->end,   '[');
        sel->end = buf_rscan(buf, sel->end++, ']');
        sel->beg++, sel->end--;
    } else if (r == '{' || r == '}') {
        sel->beg = buf_lscan(buf, sel->end,   '{');
        sel->end = buf_rscan(buf, sel->end++, '}');
        sel->beg++, sel->end--;
    } else {
        selbigword(view, sel);
    }
}

void view_selword(View* view, size_t row, size_t col) {
    view_setcursor(view, row, col);
    Sel sel = view->selection;
    selbigword(view, &sel);
    sel.end++;
    view->selection = sel;
}

void view_select(View* view, size_t row, size_t col) {
    view_setcursor(view, row, col);
    Sel sel = view->selection;
    selcontext(view, &sel);
    sel.end++;
    view->selection = sel;
}

char* view_fetch(View* view, size_t row, size_t col) {
    char* str = NULL;
    size_t off = getoffset(view, row, col);
    if (off != SIZE_MAX) {
        Sel sel = { .beg = off, .end = off };
        if (in_selection(view->selection, off)) {
            sel = view->selection;
        } else {
            selcontext(view, &sel);
            sel.end++;
        }
        str = view_getstr(view, &sel);
    }
    return str;
}

void view_find(View* view, size_t row, size_t col) {
    size_t off = getoffset(view, row, col);
    if (off != SIZE_MAX) {
        Sel sel = view->selection;
        if (!num_selected(sel) || !in_selection(sel, off)) {
            view_setcursor(view, row, col);
            sel = view->selection;
            selcontext(view, &sel);
            buf_find(&(view->buffer), &sel.beg, &sel.end);
            sel.end++;
        } else {
            buf_find(&(view->buffer), &sel.beg, &sel.end);
        }
        view->selection = sel;
        view->sync_needed = true;
    }
}

void view_insert(View* view, Rune rune) {
    if (rune == '\b') {
        if (num_selected(view->selection))
            view_delete(view);
        else if (view->selection.end > 0)
            buf_del(&(view->buffer), --view->selection.end);
    } else {
        /* ignore non-printable control characters */
        if (!isspace(rune) && rune < 0x20)
            return;
        if (num_selected(view->selection))
            view_delete(view);
        buf_ins(&(view->buffer), view->selection.end++, rune);
    }
    view->selection.beg = view->selection.end;
    view->selection.col = buf_getcol(&(view->buffer), view->selection.end);
    view->sync_needed = true;
}

void view_delete(View* view) {
    Sel sel = view->selection;
    selswap(&sel);
    size_t num = num_selected(view->selection);
    for (size_t i = 0; i < num; i++)
        buf_del(&(view->buffer), sel.beg);
    view->selection.beg = view->selection.end = sel.beg;
    view->selection.col = buf_getcol(&(view->buffer), view->selection.end);
    view->sync_needed = true;
}

void view_bol(View* view) {
    view->selection.beg = view->selection.end = buf_bol(&(view->buffer), view->selection.end);
    view->selection.col = buf_getcol(&(view->buffer), view->selection.end);
    view->sync_needed = true;
}

void view_eol(View* view) {
    view->selection.beg = view->selection.end = buf_eol(&(view->buffer), view->selection.end);
    view->selection.col = buf_getcol(&(view->buffer), view->selection.end);
    view->sync_needed = true;
}

void view_undo(View* view) {
    view->selection.beg = view->selection.end = buf_undo(&(view->buffer), view->selection.end);
    view->selection.col = buf_getcol(&(view->buffer), view->selection.end);
    view->sync_needed = true;
}

void view_redo(View* view) {
    view->selection.beg = view->selection.end = buf_redo(&(view->buffer), view->selection.end);
    view->selection.col = buf_getcol(&(view->buffer), view->selection.end);
    view->sync_needed = true;
}

void view_putstr(View* view, char* str) {
    while (*str) {
        Rune rune = 0;
        size_t length = 0;
        while (!utf8decode(&rune, &length, *str++));
        view_insert(view, rune);
    }
}

char* view_getstr(View* view, Sel* range) {
    Buf* buf = &(view->buffer);
    Sel sel = (range ? *range : view->selection);
    selswap(&sel);
    char utf[UTF_MAX] = {0};
    size_t len = 0;
    char*  str = NULL;
    for (; sel.beg < sel.end; sel.beg++) {
        Rune rune = buf_get(buf, sel.beg);
        if (rune == RUNE_CRLF) {
            str = realloc(str, len + 2);
            str[len + 1] = '\r';
            str[len + 2] = '\n';
            len += 2;
        } else {
            size_t n = utf8encode(utf, rune);
            str = realloc(str, len + n);
            memcpy(str+len, utf, n);
            len += n;
        }
    }
    str = realloc(str, len+1);
    if (str) str[len] = '\0';
    return str;
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
