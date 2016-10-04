#include "edit.h"

static unsigned NumRows = 0;
static unsigned NumCols = 0;
static Row** Rows;

void screen_setsize(unsigned nrows, unsigned ncols)
{
    if (Rows) free(Rows);
    Rows = calloc(nrows, sizeof(Row*));
    for (unsigned i = 0; i < nrows; i++)
        Rows[i] = calloc(1, sizeof(Row) + (ncols * sizeof(Rune)));
    NumRows = nrows;
    NumCols = ncols;
}

void screen_getsize(unsigned* nrows, unsigned* ncols)
{
    *nrows = NumRows, *ncols = NumCols;
}

void screen_clear(void)
{
    for (unsigned i = 0; i < NumRows; i++)
        screen_clearrow(i);
}

Row* screen_getrow(unsigned row)
{
    return (row < NumRows ? Rows[row] : NULL);
}

void screen_clearrow(unsigned row)
{
    Row* scrrow = screen_getrow(row);
    if (!scrrow) return;
    for (unsigned i = 0; i < NumCols; i++)
        scrrow->cols[i] = (Rune)' ';
    scrrow->rlen = 0;
    scrrow->len  = 0;
}

void screen_setrowoff(unsigned row, unsigned off)
{
    screen_getrow(row)->off = off;
}

unsigned screen_setcell(unsigned row, unsigned col, Rune r)
{
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

Rune screen_getcell(unsigned row, unsigned col)
{
    if (row >= NumRows || col >= NumCols) return 0;
    Row* scrrow = screen_getrow(row);
    return scrrow->cols[col];
}
