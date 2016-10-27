#define _GNU_SOURCE
#include <string.h>
#include <assert.h>
#include <wchar.h>

#include "edit.h"

void buf_load(Buf* buf, char* path) {
    buf_setlocked(buf, false);
    if (!strcmp(path,"-")) {
        buf->charset = UTF_8;
        Rune r;
        while (RUNE_EOF != (r = fgetrune(stdin)))
            buf_ins(buf, buf_end(&Buffer), r);
    } else {
        FMap file = fmap(path);
        buf->path = strdup(path);
        buf->charset = (file.buf ? charset(file.buf, file.len, &buf->crlf) : UTF_8);
        /* load the file contents if it has any */
        if (buf->charset > UTF_8) {
            die("Unsupported character set");
        } else if (buf->charset == BINARY) {
            binload(buf, file);
        } else {
            utf8load(buf, file);
        }
        /* new files should have a newline in the buffer */
        if (!file.buf)
            buf_ins(buf, 0, (Rune)'\n');
        funmap(file);
    }
    buf_setlocked(buf, true);
    buf->modified = false;
}

void buf_save(Buf* buf) {
    if (!buf->path) return;
    FILE* file = fopen(buf->path, "wb");
    if (!file) return;
    if (buf->charset == BINARY)
        binsave(buf, file);
    else
        utf8save(buf, file);
    fclose(file);
    buf->modified = false;
}

void buf_resize(Buf* buf, size_t sz) {
    /* allocate the new buffer and gap */
    Buf copy = *buf;
    copy.bufsize  = sz;
    copy.bufstart = (Rune*)malloc(copy.bufsize * sizeof(Rune));
    copy.bufend   = copy.bufstart + copy.bufsize;
    copy.gapstart = copy.bufstart;
    copy.gapend   = copy.bufend;
    /* copy the data from the old buffer to the new one */
    for (Rune* curr = buf->bufstart; curr < buf->gapstart; curr++)
        *(copy.gapstart++) = *(curr);
    for (Rune* curr = buf->gapend; curr < buf->bufend; curr++)
        *(copy.gapstart++) = *(curr);
    /* free the buffer and commit the changes */
    free(buf->bufstart);
    *buf = copy;
}

static void syncgap(Buf* buf, unsigned off) {
    assert(off <= buf_end(buf));
    /* If the buffer is full, resize it before syncing */
    if (0 == (buf->gapend - buf->gapstart))
        buf_resize(buf, buf->bufsize << 1);
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

void buf_init(Buf* buf) {
    buf->locked   = true;
    buf->modified = false;
    buf->charset  = DEFAULT_CHARSET;
    buf->crlf     = DEFAULT_CRLF;
    buf->bufsize  = BufSize;
    buf->bufstart = (Rune*)malloc(buf->bufsize * sizeof(Rune));
    buf->bufend   = buf->bufstart + buf->bufsize;
    buf->gapstart = buf->bufstart;
    buf->gapend   = buf->bufend;
    buf->undo     = NULL;
    buf->redo     = NULL;
}

static void log_insert(Log** list, unsigned beg, unsigned end) {
    Log* newlog  = (Log*)calloc(sizeof(Log), 1);
    newlog->insert = true;
    newlog->data.ins.beg = beg;
    newlog->data.ins.end = end;
    newlog->next = *list;
    *list = newlog;
}

static void log_delete(Log** list, unsigned off, Rune* r, size_t len) {
    Log* newlog  = (Log*)calloc(sizeof(Log), 1);
    newlog->insert = false;
    newlog->data.del.off = off;
    newlog->data.del.len = len;
    newlog->data.del.runes = (Rune*)malloc(sizeof(Rune) * len);
    for (size_t i = 0; i < len; i++)
        newlog->data.del.runes[i] = r[i];
    newlog->next = *list;
    *list = newlog;
}

static void insert(Buf* buf, unsigned off, Rune rune) {
    syncgap(buf, off);
    if (buf->crlf && rune == '\n' && buf_get(buf, off-1) == '\r')
        *(buf->gapstart-1) = RUNE_CRLF;
    else
        *(buf->gapstart++) = rune;
}

void buf_ins(Buf* buf, unsigned off, Rune rune) {
    if (buf->locked) { return; }
    buf->modified = true;
    log_insert(&(buf->undo), off, off+1);
    insert(buf, off, rune);
}

static void delete(Buf* buf, unsigned off) {
    syncgap(buf, off);
    buf->gapend++;
}

void buf_del(Buf* buf, unsigned off) {
    if (buf->locked) { return; }
    buf->modified = true;
    Rune r = buf_get(buf, off);
    log_delete(&(buf->undo), off, &r, 1);
    delete(buf, off);
}

unsigned swaplog(Buf* buf, Log** from, Log** to, unsigned pos) {
    /* pop the last action */
    Log* log = *from;
    if (!log) return pos;
    *from = log->next;
    /* invert the log type and move it to the destination */
    Log* newlog = (Log*)calloc(sizeof(Log), 1);
    if (log->insert) {
        newlog->insert = false;
        size_t n = (log->data.ins.end - log->data.ins.beg);
        newlog->data.del.off   = log->data.ins.beg;
        newlog->data.del.len   = n;
        newlog->data.del.runes = (Rune*)malloc(n * sizeof(Rune));
        for (size_t i = 0; i < n; i++) {
            newlog->data.del.runes[i] = buf_get(buf, log->data.ins.beg);
            delete(buf, log->data.ins.beg);
        }
        pos = newlog->data.del.off;
    } else {
        newlog->insert = true;
        newlog->data.ins.beg = log->data.del.off;
        newlog->data.ins.end = log->data.del.off + log->data.del.len;
        for (size_t i = log->data.del.len; i > 0; i--) {
            insert(buf, newlog->data.ins.beg, log->data.del.runes[i-1]);
        }
        pos = newlog->data.ins.end;
    }
    newlog->next = *to;
    *to = newlog;
    return pos;
}

unsigned buf_undo(Buf* buf, unsigned pos) {
    return swaplog(buf, &(buf->undo), &(buf->redo), pos);
}

unsigned buf_redo(Buf* buf, unsigned pos) {
    return swaplog(buf, &(buf->redo), &(buf->undo), pos);
}

Rune buf_get(Buf* buf, unsigned off) {
    if (off >= buf_end(buf)) return (Rune)'\n';
    size_t bsz = (buf->gapstart - buf->bufstart);
    if (off < bsz)
        return *(buf->bufstart + off);
    else
        return *(buf->gapend + (off - bsz));
}

void buf_setlocked(Buf* buf, bool locked) {
    if (locked)
        buf->undo->locked =true;
    buf->locked = locked;
}

bool buf_locked(Buf* buf) {
    return buf->locked;
}

bool buf_iseol(Buf* buf, unsigned off) {
    Rune r = buf_get(buf, off);
    return (r == '\n' || r == RUNE_CRLF);
}

unsigned buf_bol(Buf* buf, unsigned off) {
    for (; !buf_iseol(buf, off-1); off--);
    return off;
}

unsigned buf_eol(Buf* buf, unsigned off) {
    for (; !buf_iseol(buf, off); off++);
    return off;
}

unsigned buf_bow(Buf* buf, unsigned off) {
    for (; risword(buf_get(buf, off-1)); off--);
    return off;
}

unsigned buf_eow(Buf* buf, unsigned off) {
    for (; risword(buf_get(buf, off)); off++);
    return off-1;
}

unsigned buf_lscan(Buf* buf, unsigned off, Rune r) {
    for (; r != buf_get(buf, off); off--);
    return off;
}

unsigned buf_rscan(Buf* buf, unsigned off, Rune r) {
    for (; r != buf_get(buf, off); off++);
    return off;
}

int range_match(Buf* buf, unsigned dbeg, unsigned dend, unsigned mbeg, unsigned mend) {
    unsigned n1 = dend-dbeg, n2 = mend-mbeg;
    if (n1 != n2) return n1-n2;
    for (; n1; n1--, dbeg++, mbeg++) {
        int cmp = buf_get(buf, dbeg) - buf_get(buf, mbeg);
        if (cmp != 0) return cmp;
    }
    return 0;
}

void buf_find(Buf* buf, unsigned* beg, unsigned* end) {
    unsigned dbeg = *beg, dend = *end;
    unsigned mbeg = dend+1, mend = mbeg + (dend-dbeg);
    while (mend != dbeg) {
        if ((buf_get(buf, mbeg) == buf_get(buf, dbeg)) &&
            (buf_get(buf, mend) == buf_get(buf, dend)) &&
            (0 == range_match(buf,dbeg,dend,mbeg,mend)))
        {
            *beg = mbeg;
            *end = mend;
            break;
        }
        mbeg++, mend++;
        if (mend >= buf_end(buf)) {
            unsigned n = mend-mbeg;
            mbeg = 0, mend = n;
        }
    }
}

unsigned buf_end(Buf* buf) {
    size_t bufsz = buf->bufend - buf->bufstart;
    size_t gapsz = buf->gapend - buf->gapstart;
    return (bufsz - gapsz);
}

unsigned buf_byrune(Buf* buf, unsigned pos, int count) {
    int move = (count < 0 ? -1 : 1);
    count *= move; // remove the sign if there is one
    for (; count > 0; count--)
        if (move < 0) {
            if (pos > 0) pos--;
        } else {
            if (pos < buf_end(buf)) pos++;
        }
    return pos;
}

unsigned buf_byline(Buf* buf, unsigned pos, int count) {
    int move = (count < 0 ? -1 : 1);
    count *= move; // remove the sign if there is one
    for (; count > 0; count--) {
        if (move < 0) {
            if (pos > buf_eol(buf, 0))
                pos = buf_bol(buf, buf_bol(buf, pos)-1);
        } else {
            unsigned next = buf_eol(buf, pos)+1;
            if (next < buf_end(buf))
                pos = next;
        }
    }
    return pos;
}

unsigned buf_getcol(Buf* buf, unsigned pos) {
    unsigned curr = buf_bol(buf, pos);
    unsigned col = 0;
    for (; curr < pos; curr = buf_byrune(buf, curr, 1))
        col += runewidth(col, buf_get(buf, curr));
    return col;
}

unsigned buf_setcol(Buf* buf, unsigned pos, unsigned col) {
    unsigned bol  = buf_bol(buf, pos);
    unsigned curr = bol;
    unsigned len  = 0;
    unsigned i    = 0;
    /* determine the length of the line in columns */
    for (; !buf_iseol(buf, curr); curr++)
        len += runewidth(len, buf_get(buf, curr));
    /* iterate over the runes until we reach the target column */
    curr = bol, i = 0;
    while (i < col && i < len) {
        int width = runewidth(i, buf_get(buf, curr));
        curr = buf_byrune(buf, curr, 1);
        if (col >= i && col < (i+width))
            break;
        i += width;
    }
    return curr;
}
