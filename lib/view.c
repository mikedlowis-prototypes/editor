#include <stdc.h>
#include <utf.h>
#include <edit.h>
#include <ctype.h>

typedef size_t (*movefn_t)(Buf* buf, size_t pos, int count);

static void move_selection(View* view, bool extsel, Sel* sel, int move, movefn_t bything);
static void move_to(View* view, bool extsel, size_t off);
static void select_context(View* view, bool (*isword)(Rune), Sel* sel);
static void selswap(Sel* sel);
static size_t num_selected(Sel sel);
static bool in_selection(Sel sel, size_t off);
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

void view_init(View* view, char* file, void (*errfn)(char*)) {
    if (view->nrows) {
        for (size_t i = 0; i < view->nrows; i++)
            free(view->rows[i]);
        free(view->rows);
        view->nrows = 0;
        view->rows  = NULL;
    }
    view->selection   = (Sel){ 0 };
    view->sync_needed = true;
    view->sync_center = true;
    view->prev_csr    = 0;
    /* load the file and jump to the address returned from the load function */
    buf_init(&(view->buffer), errfn);
    if (file) {
        size_t pos = buf_load(&(view->buffer), file);
        if (pos > 0) {
            view_jumpto(view, false, pos);
            view_eol(view, false);
            view_selctx(view);
        }
    }
}

void view_reload(View* view) {
    if (view->buffer.path) {
        buf_reload(&(view->buffer));
        view->selection   = (Sel){ 0 };
        view->sync_needed = true;
        view->sync_center = true;
    }
}

size_t view_limitrows(View* view, size_t maxrows, size_t ncols) {
    size_t nrows = 1, pos = 0, col = 0;
    while (nrows < maxrows && pos < buf_end(&(view->buffer))) {
        Rune r = buf_get(&(view->buffer), pos++);
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
    size_t csr = view->selection.end;
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
    move_selection(view, extsel, &(view->selection), move, buf_byrune);
}

void view_byword(View* view, int move, bool extsel) {
    move_selection(view, extsel, &(view->selection), move, buf_byword);
}

void view_byline(View* view, int move, bool extsel) {
    move_selection(view, extsel, &(view->selection), move, buf_byline);
}

void view_setcursor(View* view, size_t row, size_t col, bool extsel) {
    size_t off = getoffset(view, row, col);
    if (off != SIZE_MAX)
        view_jumpto(view, extsel, off);
}

void view_selword(View* view, size_t row, size_t col) {
    buf_loglock(&(view->buffer));
    if (row != SIZE_MAX && col != SIZE_MAX)
        view_setcursor(view, row, col, false);
    Sel sel = view->selection;
    buf_getword(&(view->buffer), risbigword, &(sel));
    sel.end++;
    view->selection = sel;
}

void view_selprev(View* view) {
    if (!num_selected(view->selection)) {
        buf_loglock(&(view->buffer));
        Sel sel = view->selection;
        buf_lastins(&(view->buffer), &sel.beg, &sel.end);
        view->selection = sel;
    } else {
        view->selection.beg = view->selection.end;
    }
}

void view_select(View* view, size_t row, size_t col) {
    buf_loglock(&(view->buffer));
    view_setcursor(view, row, col, false);
    Sel sel = view->selection;
    select_context(view, risword, &sel);
    view->selection = sel;
}

size_t view_selsize(View* view) {
    return num_selected(view->selection);
}

char* view_fetch(View* view, size_t row, size_t col, bool (*isword)(Rune)) {
   char* str = NULL;
    size_t off = getoffset(view, row, col);
    if (off != SIZE_MAX) {
        Sel sel = { .beg = off, .end = off };
        if (in_selection(view->selection, off)) {
            sel = view->selection;
        } else {
            buf_getword(&(view->buffer), isword, &sel);
            sel.end++;
        }
        str = view_getstr(view, &sel);
    }
    return str;
}

bool view_findstr(View* view, int dir, char* str) {
    Sel sel = view->selection;
    size_t prev = sel.end;
    buf_findstr(&(view->buffer), dir, str, &sel.beg, &sel.end);
    bool found = (0 != memcmp(&sel, &(view->selection), sizeof(Sel)));
    view->selection   = sel;
    view->sync_needed = true;
    view->sync_center = true;
    if (found) view->prev_csr = prev;
    return found;
}

void view_insert(View* view, bool indent, Rune rune) {
    /* ignore non-printable control characters */
    if (!isspace(rune) && (rune >= 0 && rune < 0x20))
        return;
    if (num_selected(view->selection)) {
        Sel sel = view->selection;
        selswap(&sel);
        sel.beg = sel.end = buf_change(&(view->buffer), sel.beg, sel.end);
        view->selection = sel;
    }
    unsigned newpos = buf_insert(&(view->buffer), indent, view->selection.end, rune);
    move_to(view, false, newpos);
}

void view_delete(View* view, int dir, bool byword) {
    Sel* sel = &(view->selection);
    if (sel->beg == sel->end)
        (byword ? view_byword : view_byrune)(view, dir, true);
    selswap(sel);
    unsigned newpos = buf_delete(&(view->buffer), sel->beg, sel->end);
    move_to(view, false, newpos);
}

void view_jumpto(View* view, bool extsel, size_t off) {
    view->prev_csr = view->selection.end;
    move_to(view, extsel, off);
}

void view_jumpback(View* view) {
    view_jumpto(view, false, view->prev_csr);
    view->sync_center = true;
}

void view_bol(View* view, bool extsel) {
    /* determine whether we are jumping to start of content or line */
    Buf* buf = &(view->buffer);
    unsigned bol = buf_bol(buf, view->selection.end);
    unsigned boi = bol;
    for (; ' ' == buf_get(buf, boi) || '\t' == buf_get(buf, boi); boi++);
    unsigned pos = view->selection.end;
    pos = (pos == bol || pos > boi ? boi : bol);
    move_to(view, extsel, pos);
}

void view_eol(View* view, bool extsel) {
    move_to(view, extsel, buf_eol(&(view->buffer), view->selection.end));
    view->selection.col = -1; // Peg cursor to line end
}

void view_bof(View* view, bool extsel) {
    view_jumpto(view, extsel, 0);
}

void view_eof(View* view, bool extsel) {
    view_jumpto(view, extsel, buf_end(&(view->buffer)));
}

void view_setln(View* view, size_t line) {
    view_jumpto(view, false, buf_setln(&(view->buffer), line));
    view->sync_center = true;
}

void view_undo(View* view) {
    view->prev_csr = view->selection.end;
    buf_undo(&(view->buffer), &(view->selection));
    view->sync_needed = true;
    view->sync_center = !selection_visible(view);
}

void view_redo(View* view) {
    view->prev_csr = view->selection.end;
    buf_redo(&(view->buffer), &(view->selection));
    view->sync_needed = true;
    view->sync_center = !selection_visible(view);
}

void view_putstr(View* view, char* str) {
    selswap(&(view->selection));
    unsigned beg = view->selection.beg;
    buf_loglock(&(view->buffer));
    while (*str) {
        Rune rune = 0;
        size_t length = 0;
        while (!utf8decode(&rune, &length, *str++));
        view_insert(view, false, rune);
    }
    buf_loglock(&(view->buffer));
    view->selection.beg = beg;
}

void view_append(View* view, char* str) {
    size_t end = buf_end(&(view->buffer));
    if (view->selection.end != end)
        view->selection = (Sel){ .beg = end, .end = end };
    if (!num_selected(view->selection) && !buf_iseol(&(view->buffer), view->selection.end-1)) {
        buf_insert(&(view->buffer), false, view->selection.end++, '\n');
        view->selection.beg++;
    }
    unsigned beg = view->selection.beg;
    view_putstr(view, str);
    view->selection.beg = beg;
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
        size_t n = utf8encode(utf, rune);
        str = realloc(str, len + n);
        memcpy(str+len, utf, n);
        len += n;
    }
    if (str) {
        str = realloc(str, len+1);
        str[len] = '\0';
    }
    return str;
}

char* view_getcmd(View* view) {
    Sel sel = view->selection;
    if (!num_selected(sel))
        select_context(view, riscmd, &sel);
    return view_getstr(view, &sel);
}

void view_selctx(View* view) {
    if (!num_selected(view->selection))
        select_context(view, risword, &(view->selection));
}

char* view_getctx(View* view) {
    view_selctx(view);
    return view_getstr(view, NULL);
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

void view_indent(View* view, int dir) {
    Buf* buf = &(view->buffer);
    unsigned indoff = (ExpandTabs ? TabWidth : 1);
    selswap(&(view->selection));
    view->selection.beg = buf_bol(buf, view->selection.beg);
    view->selection.end = buf_eol(buf, view->selection.end);
    unsigned off = buf_bol(buf, view->selection.end);
    if (num_selected(view->selection) == 0) return;

    do {
        if (dir == RIGHT) {
            buf_insert(buf, true, off, '\t');
            view->selection.end += indoff;
        } else if (dir == LEFT) {
            unsigned i = 4;
            for (; i > 0; i--) {
                if (' ' == buf_get(buf, off)) {
                    buf_delete(buf, off, off+1);
                    view->selection.end--;
                } else {
                    break;
                }
            }
            if (i && '\t' == buf_get(buf, off)) {
                buf_delete(buf, off, off+1);
                view->selection.end--;
            }
        }
        off = buf_byline(buf, off, UP);

   } while (off && off >= view->selection.beg);
}

Rune view_getrune(View* view) {
    return buf_get(&(view->buffer), view->selection.end);
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

static void move_selection(View* view, bool extsel, Sel* sel, int move, movefn_t bything) {
    view->sync_needed = true;
    if (num_selected(*sel) && !extsel) {
        selswap(sel);
        if (move == RIGHT || move == DOWN)
            sel->beg = sel->end;
        else
            sel->end = sel->beg;
    } else {
        sel->end = bything(&(view->buffer), sel->end, move);
        if (bything == buf_byline)
            sel->end = buf_setcol(&(view->buffer), sel->end, sel->col);
        if (!extsel) sel->beg = sel->end;
    }
    /* only update column if not moving vertically */
    if (bything != buf_byline)
        sel->col = buf_getcol(&(view->buffer), sel->end);
}

static void move_to(View* view, bool extsel, size_t off) {
    Buf* buf = &(view->buffer);
    view->selection.end = (off > buf_end(buf) ? buf_end(buf) : off);
    if (!extsel)
        view->selection.beg = view->selection.end;
    view->selection.col = buf_getcol(&(view->buffer), view->selection.end);
    view->sync_needed = true;
}

static void select_context(View* view, bool (*isword)(Rune), Sel* sel) {
    Buf* buf = &(view->buffer);
    size_t bol = buf_bol(buf, sel->end);
    Rune r = buf_get(buf, sel->end);
    if (r == '(' || r == ')') {
        buf_getblock(buf, '(', ')', sel);
    } else if (r == '[' || r == ']') {
        buf_getblock(buf, '[', ']', sel);
    } else if (r == '{' || r == '}') {
        buf_getblock(buf, '{', '}', sel);
    } else if (sel->end == bol || r == '\n') {
        sel->beg = bol;
        sel->end = buf_eol(buf, sel->end);
    } else if (risword(r)) {
        buf_getword(buf, isword, sel);
    } else {
        buf_getword(buf, risbigword, sel);
    }
    sel->end = buf_byrune(&(view->buffer), sel->end, RIGHT);
    sel->col = buf_getcol(&(view->buffer), sel->end);
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

static bool selection_visible(View* view) {
    if (!view->nrows) return true;
    size_t csr = view->selection.end;
    size_t beg = view->rows[0]->off;
    size_t end = view->rows[view->nrows-1]->off +
                 view->rows[view->nrows-1]->rlen;
    return (beg <= csr && csr <= end);
}

static void find_cursor(View* view, size_t* csrx, size_t* csry) {
    size_t csr = view->selection.end;
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
        uint32_t attr = (in_selection(view->selection, pos) ? view->clrsel : view->clrnor);
        Rune r = buf_get(&(view->buffer), pos++);
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
            x += runewidth(x, buf_get(&(view->buffer), pos+x));
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
