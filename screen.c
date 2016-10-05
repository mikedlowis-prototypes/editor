#include "edit.h"

static unsigned NumRows = 0;
static unsigned NumCols = 0;
static Row** Rows;

void screen_setsize(unsigned nrows, unsigned ncols) {
    if (Rows) free(Rows);
    Rows = calloc(nrows, sizeof(Row*));
    for (unsigned i = 0; i < nrows; i++)
        Rows[i] = calloc(1, sizeof(Row) + (ncols * sizeof(Rune)));
    NumRows = nrows;
    NumCols = ncols;
}

void screen_getsize(unsigned* nrows, unsigned* ncols) {
    *nrows = NumRows, *ncols = NumCols;
}

void screen_clear(void) {
    for (unsigned i = 0; i < NumRows; i++)
        screen_clearrow(i);
}

Row* screen_getrow(unsigned row) {
    return (row < NumRows ? Rows[row] : NULL);
}

void screen_clearrow(unsigned row) {
    Row* scrrow = screen_getrow(row);
    if (!scrrow) return;
    for (unsigned i = 0; i < NumCols; i++)
        scrrow->cols[i] = (Rune)' ';
    scrrow->rlen = 0;
    scrrow->len  = 0;
}

void screen_setrowoff(unsigned row, unsigned off) {
    screen_getrow(row)->off = off;
}

unsigned screen_setcell(unsigned row, unsigned col, Rune r) {
    if (row >= NumRows || col >= NumCols) return 0;
    Row* scrrow = screen_getrow(row);
    /* write the rune to the screen buf */
    unsigned ncols = 1;
    if (r == '\t') {
        scrrow->cols[col] = ' ';
        ncols = (TabWidth - (col % TabWidth));
    } else if (r != '\n') {
        scrrow->cols[col] = r;
    }
    /* Update lengths */
    scrrow->rlen += 1;
    if ((col + ncols) > scrrow->len)
        scrrow->len = col + ncols;
    return ncols;
}

Rune screen_getcell(unsigned row, unsigned col) {
    if (row >= NumRows || col >= NumCols) return 0;
    Row* scrrow = screen_getrow(row);
    return scrrow->cols[col];
}

static unsigned linelen(Buf* buf, unsigned csr) {
    unsigned eol = buf_eol(buf, csr);
    unsigned bol = buf_bol(buf, csr);
    return (eol - bol);
}

static void fill_row(Buf* buf, unsigned row, unsigned pos) {
    for (unsigned x = 0; x < NumCols;) {
        Rune r = buf_get(buf, pos++);
        x += screen_setcell(row, x, r);
        if (r == '\n') break;
    }
}

static void scroll_up(Buf* buf, unsigned csr, unsigned first) {
    while (csr < first) {
        /* delete the last row and shift the others */
        free(Rows[NumRows - 1]);
        memmove(&Rows[2], &Rows[1], sizeof(Row*) * (NumRows-2));
        Rows[1] = calloc(1, sizeof(Row) + (NumCols * sizeof(Rune)));
        Rows[1]->off = buf_byline(buf, Rows[2]->off, -1);
        /* fill in row content */
        fill_row(buf, 1, Rows[1]->off);
        first = Rows[1]->off;
    }
}

static void scroll_dn(Buf* buf, unsigned csr, unsigned last) {
    while (csr > last) {
        /* delete the first row and shift the others */
        free(Rows[1]);
        memmove(&Rows[1], &Rows[2], sizeof(Row*) * (NumRows-2));
        Rows[NumRows-1] = calloc(1, sizeof(Row) + (NumCols * sizeof(Rune)));
        Rows[NumRows-1]->off = (Rows[NumRows-2]->off + Rows[NumRows-2]->rlen);
        /* fill in row content */
        fill_row(buf, NumRows-1, Rows[NumRows-1]->off);
        last = Rows[NumRows-1]->off + Rows[NumRows-1]->rlen - 1;
    }
}

static void sync_view(Buf* buf, unsigned csr) {
    unsigned first = Rows[1]->off;
    unsigned last  = Rows[NumRows-1]->off + Rows[NumRows-1]->rlen - 1;
    if (csr < first) {
        scroll_up(buf, csr, first);
    } else if (csr > last) {
        scroll_dn(buf, csr, last);
    }
}

void screen_update(Buf* buf, unsigned csr, unsigned* csrx, unsigned* csry) {
    sync_view(buf, csr);
    screen_clearrow(0);
    unsigned pos = Rows[1]->off;
    for (unsigned y = 1; y < NumRows; y++) {
        screen_clearrow(y);
        screen_setrowoff(y, pos);
        for (unsigned x = 0; x < NumCols;) {
            if (csr == pos)
                *csrx = x, *csry = y;
            Rune r = buf_get(buf, pos++);
            x += screen_setcell(y,x,r);
            if (r == '\n') break;
        }
    }
}
