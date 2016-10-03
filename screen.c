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
    scrrow->len = 0;
}

void screen_setcell(unsigned row, unsigned col, Rune r)
{
    if (row >= NumRows || col >= NumCols) return;
    Row* scrrow = screen_getrow(row);
    scrrow->cols[col] = r;
    if (col+1 >= scrrow->len)
        scrrow->len = col+1;
}

Rune screen_getcell(unsigned row, unsigned col)
{
    if (row >= NumRows || col >= NumCols) return 0;
    Row* scrrow = screen_getrow(row);
    return scrrow->cols[col];
}
