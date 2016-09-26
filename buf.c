#include <assert.h>
#include "edit.h"

void buf_load(Buf* buf, char* path)
{
    unsigned i = 0;
    FILE* in = fopen(path, "rb");
    int c;
    while (EOF != (c = fgetc(in)))
        buf_ins(buf, i++, (Rune)c);
    fclose(in);
}

void buf_initsz(Buf* buf, size_t sz)
{
    buf->bufsize  = sz;
    buf->bufstart = (Rune*)malloc(buf->bufsize * sizeof(Rune));
    buf->bufend   = buf->bufstart + buf->bufsize;
    buf->gapstart = buf->bufstart;
    buf->gapend   = buf->bufend;
}

static void syncgap(Buf* buf, unsigned off)
{
    //printf("assert: %u <= %u\n", off, buf_end(buf));
    assert(off <= buf_end(buf));
    /* If the buffer is full, resize it before syncing */
    if (0 == (buf->gapend - buf->gapstart)) {
        Buf newbuf;
        buf_initsz(&newbuf, buf->bufsize << 1);

        for (Rune* curr = buf->bufstart; curr < buf->gapstart; curr++)
            *(newbuf.gapstart++) = *(curr);
        for (Rune* curr = buf->gapend; curr < buf->bufend; curr++)
            *(newbuf.gapstart++) = *(curr);

        free(buf->bufstart);
        *buf = newbuf;
    }

    /* Move the gap to the desired offset */
    Rune* newpos = (buf->bufstart + off);
    if (newpos < buf->gapstart) {
        while (newpos < buf->gapstart)
            *(--buf->gapend) = *(--buf->gapstart);
    } else {
        while (newpos > buf->gapstart)
            *(buf->gapstart++) = *(buf->gapend++);
    }
}

void buf_init(Buf* buf)
{
    buf_initsz(buf, BufSize);
}

void buf_clr(Buf* buf)
{
    free(buf->bufstart);
    buf_init(buf);
}

void buf_del(Buf* buf, unsigned off)
{
    syncgap(buf, off);
    buf->gapend++;
}

void buf_ins(Buf* buf, unsigned off, Rune rune)
{
    syncgap(buf, off);
    *(buf->gapstart++) = rune;
}

Rune buf_get(Buf* buf, unsigned off)
{
    if (off >= buf_end(buf)) return (Rune)'\n';
    size_t bsz = (buf->gapstart - buf->bufstart);
    if (off < bsz)
        return *(buf->bufstart + off);
    else
        return *(buf->gapend + (off - bsz));
}

unsigned buf_bol(Buf* buf, unsigned off)
{
    if (off == 0) return 0;
    if (buf_get(buf, off) == '\n' && buf_get(buf, off-1) == '\n') return off;
    for (Rune r; (r = buf_get(buf, off)) != '\n'; off--);
    return off+1;
}

unsigned buf_eol(Buf* buf, unsigned off)
{
    if (off >= buf_end(buf)) return off;
    if (buf_get(buf, off) == '\n') return off;
    for (Rune r; (r = buf_get(buf, off)) != '\n'; off++);
    return off-1;
}

unsigned buf_end(Buf* buf)
{
    size_t bufsz = buf->bufend - buf->bufstart;
    size_t gapsz = buf->gapend - buf->gapstart;
    return (bufsz - gapsz);
}

static unsigned move_back(Buf* buf, unsigned pos) {
    if (pos == 0) return 0;
    if (buf_get(buf, pos)   == '\n' &&
        buf_get(buf, pos-1) == '\n' &&
        buf_get(buf, pos-2) != '\n')
        pos = pos - 2;
    else if (buf_get(buf, pos)   != '\n' &&
             buf_get(buf, pos-1) == '\n' &&
             buf_get(buf, pos-2) != '\n')
        pos = pos - 2;
    else
        pos = pos - 1;
    return pos;
}

static unsigned move_forward(Buf* buf, unsigned pos) {
    if (pos+2 >= buf_end(buf)) return buf_end(buf)-2;
    if (buf_get(buf, pos)   != '\n' &&
        buf_get(buf, pos+1) == '\n')
        pos = pos + 2;
    else
        pos = pos + 1;
    return pos;
}

unsigned buf_byrune(Buf* buf, unsigned pos, int count)
{
    int move = (count < 0 ? -1 : 1);
    count *= move;
    for (; count > 0; count--)
        pos = (move > 0 ? &move_forward : &move_back)(buf, pos);
    return pos;
}

unsigned get_column(Buf* buf, unsigned pos)
{
    Rune r;
    unsigned cols = 0;
    if (buf_get(buf, pos) == '\n') return 0;
    while ((r = buf_get(buf, pos)) != '\n')
        pos--, cols += (r == '\t' ? TabWidth : 1);
    return cols-1;
}

unsigned set_column(Buf* buf, unsigned pos, unsigned col)
{
    Rune r;
    pos = buf_bol(buf, pos);
    while ((r = buf_get(buf, pos)) != '\n' && col) {
        unsigned inc = (r == '\t' ? TabWidth : 1);
        pos++, col -= (inc > col ? col : inc);
    }

    if (buf_get(buf, pos)   == '\n' &&
        buf_get(buf, pos-1) != '\n')
        pos--;
    return pos;
}

unsigned buf_byline(Buf* buf, unsigned pos, int count)
{
    int move = (count < 0 ? -1 : 1);
    count *= move; // remove the sign if there is one
    unsigned col = get_column(buf, pos);
    for (; count > 0; count--)
        pos = (move < 0 ? move_back(buf, buf_bol(buf, pos))
                        : move_forward(buf, buf_eol(buf, pos)));
    return set_column(buf, pos, col);
}
