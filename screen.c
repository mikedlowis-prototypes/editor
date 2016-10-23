#include "edit.h"

static unsigned NumRows = 0;
static unsigned NumCols = 0;
static Row** Rows;

#define ATTR_NORMAL   (CLR_BASE03 << 8 | CLR_BASE0)
#define ATTR_SELECTED (CLR_BASE0  << 8 | CLR_BASE03)

static unsigned fill_row(Buf* buf, unsigned row, unsigned pos) {
    screen_getrow(row)->off = pos;
    screen_clearrow(row);
    for (unsigned x = 0; x < NumCols;) {
        uint32_t attr = (DotBeg <= pos && pos < DotEnd ? ATTR_SELECTED : ATTR_NORMAL);
        Rune r = buf_get(buf, pos++);
        x += screen_setcell(row, x, attr, r);
        if (buf_iseol(buf, pos-1)) break;
    }
    return pos;
}

static void screen_reflow(Buf* buf) {
    unsigned pos = Rows[0]->off;
    for (unsigned y = 0; y < NumRows; y++)
        pos = fill_row(buf, y, pos);
}

void screen_setsize(Buf* buf, unsigned nrows, unsigned ncols) {
    /* free the old row data */
    if (Rows) {
        for (unsigned i = 0; i < NumRows; i++)
            free(Rows[i]);
        free(Rows);
    }
    /* create the new row data */
    Rows = calloc(nrows, sizeof(Row*));
    for (unsigned i = 0; i < nrows; i++)
        Rows[i] = calloc(1, sizeof(Row) + (ncols * sizeof(UGlyph)));
    /* update dimensions */
    NumRows = nrows;
    NumCols = ncols;
    /* populate the screen buffer */
    screen_reflow(buf);
}

unsigned screen_getoff(Buf* buf, unsigned pos, unsigned row, unsigned col) {
    Row* scrrow = screen_getrow(row);
    if (!scrrow) return pos;
    pos = scrrow->off;
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
    if (pos >= buf_end(buf))
        return buf_end(buf)-1;
    return pos;
}

void screen_getsize(unsigned* nrows, unsigned* ncols) {
    *nrows = NumRows, *ncols = NumCols;
}

Row* screen_getrow(unsigned row) {
    return (row < NumRows ? Rows[row] : NULL);
}

void screen_clearrow(unsigned row) {
    Row* scrrow = screen_getrow(row);
    if (!scrrow) return;
    for (unsigned i = 0; i < NumCols; i++)
        scrrow->cols[i].rune = (Rune)' ';
    scrrow->rlen = 0;
    scrrow->len  = 0;
}

unsigned screen_setcell(unsigned row, unsigned col, uint32_t attr, Rune r) {
    if (row >= NumRows || col >= NumCols) return 0;
    Row* scrrow = screen_getrow(row);
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

UGlyph* screen_getglyph(unsigned row, unsigned col, unsigned* scrwidth) {
    if (row >= NumRows || col >= NumCols) return 0;
    Row* scrrow = screen_getrow(row);
    UGlyph* glyph = &(scrrow->cols[col]);
    *scrwidth = 1;
    for (col++; !scrrow->cols[col].rune; col++)
        *scrwidth += 1;
    return glyph;
}

static unsigned prev_screen_line(Buf* buf, unsigned bol, unsigned off) {
    unsigned pos = bol;
    while (true) {
        unsigned x;
        for (x = 0; x < NumCols && (pos + x) < off; x++)
            x += runewidth(x, buf_get(buf, pos+x));
        if ((pos + x) >= off) break;
        pos += x;
    }
    return pos;
}

static void scroll_up(Buf* buf, unsigned csr, unsigned first) {
    while (csr < first) {
        unsigned bol    = buf_bol(buf, first);
        unsigned prevln = (first == bol ? buf_byline(buf, bol, -1) : bol);
        prevln = prev_screen_line(buf, prevln, first);
        /* delete the last row and shift the others */
        free(Rows[NumRows - 1]);
        memmove(&Rows[1], &Rows[0], sizeof(Row*) * (NumRows-1));
        Rows[0] = calloc(1, sizeof(Row) + (NumCols * sizeof(UGlyph)));
        Rows[0]->off = prevln;
        /* fill in row content */
        fill_row(buf, 0, Rows[0]->off);
        first = Rows[0]->off;
    }
}

static void scroll_dn(Buf* buf, unsigned csr, unsigned last) {
    while (csr > last) {
        /* delete the first row and shift the others */
        free(Rows[0]);
        memmove(&Rows[0], &Rows[1], sizeof(Row*) * (NumRows-1));
        Rows[NumRows-1] = calloc(1, sizeof(Row) + (NumCols * sizeof(UGlyph)));
        Rows[NumRows-1]->off = (Rows[NumRows-2]->off + Rows[NumRows-2]->rlen);
        /* fill in row content */
        fill_row(buf, NumRows-1, Rows[NumRows-1]->off);
        last = Rows[NumRows-1]->off + Rows[NumRows-1]->rlen - 1;
    }
}

static void sync_view(Buf* buf, unsigned csr) {
    unsigned first = Rows[0]->off;
    unsigned last  = Rows[NumRows-1]->off + Rows[NumRows-1]->rlen - 1;
    if (csr < first) {
        scroll_up(buf, csr, first);
    } else if (csr > last) {
        scroll_dn(buf, csr, last);
    }
}

void screen_getcoords(Buf* buf, unsigned pos, unsigned* posx, unsigned* posy) {
    for (unsigned y = 0; y < NumRows; y++) {
        unsigned start = Rows[y]->off;
        unsigned end   = Rows[y]->off + Rows[y]->rlen - 1;
        if (start <= pos && pos <= end) {
            unsigned off = start;
            for (unsigned x = 0; x < NumCols;) {
                if (off == pos) {
                    *posy = y, *posx = x;
                    return;
                }
                x += runewidth(x, buf_get(buf,off++));
            }
            break;
        }
    }
}

void screen_update(Buf* buf, unsigned csr, unsigned* csrx, unsigned* csry) {
    /* scroll the view and reflow the screen lines */
    sync_view(buf, csr);
    screen_reflow(buf);
    /* find the cursor on the new screen */
    for (unsigned y = 0; y < NumRows; y++) {
        unsigned start = Rows[y]->off;
        unsigned end   = Rows[y]->off + Rows[y]->rlen - 1;
        if (start <= csr && csr <= end) {
            unsigned pos = start;
            for (unsigned x = 0; x < NumCols;) {
                if (pos == csr) {
                    *csry = y, *csrx = x;
                    break;
                }
                x += runewidth(x, buf_get(buf,pos++));
            }
            break;
        }
    }
}
