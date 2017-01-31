#define _POSIX_C_SOURCE 200809L
#include <stdc.h>
#include <utf.h>
#include <edit.h>
#include <ctype.h>
#include <unistd.h>

static void buf_resize(Buf* buf, size_t sz);

static void log_clear(Log** list) {
    while (*list) {
        Log* deadite = *list;
        *list = (*list)->next;
        if (!deadite->insert)
            free(deadite->data.del.runes);
        free(deadite);
    }
}

static void log_insert(Buf* buf, Log** list, unsigned beg, unsigned end) {
    Log* log = *list;
    bool locked = (!log || log->transid != buf->transid);
    if (locked || !log->insert || (end != log->data.ins.end+1)) {
        buf_loglock(buf);
        Log* newlog  = (Log*)calloc(sizeof(Log), 1);
        newlog->transid = buf->transid;
        newlog->insert = true;
        newlog->data.ins.beg = beg;
        newlog->data.ins.end = end;
        newlog->next = *list;
        *list = newlog;
    } else if (beg < log->data.ins.beg) {
        log->data.ins.beg--;
    } else {
        log->data.ins.end++;
    }
}

static void log_delete(Buf* buf, Log** list, unsigned off, Rune* r, size_t len) {
    Log* log = *list;
    bool locked = (!log || log->transid != buf->transid);
    if (locked || log->insert || ((off != log->data.del.off) && (off+1 != log->data.del.off))) {
        buf_loglock(buf);
        Log* newlog = (Log*)calloc(sizeof(Log), 1);
        newlog->transid = buf->transid;
        newlog->insert = false;
        newlog->data.del.off = off;
        newlog->data.del.len = len;
        newlog->data.del.runes = (Rune*)malloc(sizeof(Rune) * len);
        for (size_t i = 0; i < len; i++)
            newlog->data.del.runes[i] = r[i];
        newlog->next = *list;
        *list = newlog;
    } else if (off == log->data.del.off) {
        log->data.del.len++;
        log->data.del.runes = (Rune*)realloc(log->data.del.runes, sizeof(Rune) * log->data.del.len);
        log->data.del.runes[log->data.del.len-1] = *r;
    } else {
        size_t bytes = sizeof(Rune) * log->data.del.len;
        log->data.del.len++;
        log->data.del.off--;
        log->data.del.runes = (Rune*)realloc(log->data.del.runes, bytes + sizeof(Rune));
        memmove(log->data.del.runes+1, log->data.del.runes, bytes);
        log->data.del.runes[0] = *r;
    }
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

static void buf_resize(Buf* buf, size_t sz) {
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

static void delete(Buf* buf, unsigned off) {
    syncgap(buf, off);
    buf->gapend++;
}

static unsigned insert(Buf* buf, unsigned off, Rune rune) {
    unsigned rcount = 1;
    syncgap(buf, off);
    if (buf->crlf && rune == '\n' && buf_get(buf, off-1) == '\r') {
        rcount = 0;
        *(buf->gapstart-1) = RUNE_CRLF;
    } else if (buf->crlf && rune == '\n') {
        *(buf->gapstart++) = RUNE_CRLF;
    } else {
        *(buf->gapstart++) = rune;
    }
    return rcount;
}

static int range_match(Buf* buf, unsigned dbeg, unsigned dend, unsigned mbeg, unsigned mend) {
    unsigned n1 = dend-dbeg, n2 = mend-mbeg;
    if (n1 != n2) return n1-n2;
    for (; n1 > 0; n1--, dbeg++, mbeg++) {
        int cmp = buf_get(buf, dbeg) - buf_get(buf, mbeg);
        if (cmp != 0) return cmp;
    }
    return 0;
}

static int rune_match(Buf* buf, unsigned mbeg, unsigned mend, Rune* runes) {
    for (; *runes; runes++, mbeg++) {
        int cmp = *runes - buf_get(buf, mbeg);
        if (cmp != 0) return cmp;
    }
    return 0;
}

static void swaplog(Buf* buf, Log** from, Log** to, Sel* sel) {
    /* pop the last action */
    Log* log = *from;
    if (!log) return;
    *from = log->next;
    /* invert the log type and move it to the destination */
    Log* newlog = (Log*)calloc(sizeof(Log), 1);
    newlog->transid = log->transid;
    if (log->insert) {
        sel->beg = sel->end = log->data.ins.beg;
        newlog->insert = false;
        size_t n = (log->data.ins.end - log->data.ins.beg);
        newlog->data.del.off   = log->data.ins.beg;
        newlog->data.del.len   = n;
        newlog->data.del.runes = (Rune*)malloc(n * sizeof(Rune));
        for (size_t i = 0; i < n; i++) {
            newlog->data.del.runes[i] = buf_get(buf, log->data.ins.beg);
            delete(buf, log->data.ins.beg);
        }
    } else {
        newlog->insert = true;
        sel->beg = newlog->data.ins.beg = log->data.del.off;
        newlog->data.ins.end = newlog->data.ins.beg;
        for (size_t i = log->data.del.len; i > 0; i--) {
            newlog->data.ins.end += insert(buf, newlog->data.ins.beg, log->data.del.runes[i-1]);
        }
        sel->end = newlog->data.ins.end;
    }
    newlog->next = *to;
    *to = newlog;
}

/*****************************************************************************/

void buf_init(Buf* buf) {
    /* cleanup old data if there is any */
    if (buf->bufstart) free(buf->bufstart);
    buf_logclear(buf);

    /* reset the state to defaults */
    buf->modified    = false;
    buf->expand_tabs = true;
    buf->copy_indent = true;
    buf->charset     = DEFAULT_CHARSET;
    buf->crlf        = DEFAULT_CRLF;
    buf->bufsize     = BufSize;
    buf->bufstart    = (Rune*)malloc(buf->bufsize * sizeof(Rune));
    buf->bufend      = buf->bufstart + buf->bufsize;
    buf->gapstart    = buf->bufstart;
    buf->gapend      = buf->bufend;
    buf->undo        = NULL;
    buf->redo        = NULL;
}

unsigned buf_load(Buf* buf, char* path) {
    /* process the file path and address */
    if (path && path[0] == '.' && path[1] == '/')
        path += 2;
    unsigned off = 0;
    buf->path = stringdup(path);
    char* addr = strrchr(buf->path, ':');
    if (addr) *addr = '\0', addr++;

    /* load the file and determine the character set */
    FMap file = mmap_readonly(buf->path);
    filetype(buf, file);
    if (buf->charset > UTF_8)
        die("Unsupported character set");

    /* read the file contents into the buffer */
    for (size_t i = 0; i < file.len;) {
        Rune r;
        if (buf->charset == BINARY) {
            r = file.buf[i++];
        } else {
            size_t len = 0;
            while (!utf8decode(&r, &len, file.buf[i++]));
        }
        buf_insert(buf, false, buf_end(buf), r);
    }
    
    /* jump to address if we got one */
    if (addr)
        off = buf_setln(buf, strtoul(addr, NULL, 0));

    /* reset buffer state */
    buf->modified = false;
    buf_logclear(buf);
    return off;
}

void buf_save(Buf* buf) {
    size_t wrlen = 0;
    if (!buf->path) return;
    FMap file = mmap_readwrite(buf->path, buf_end(buf) * UTF_MAX);
    for (unsigned i = 0, end = buf_end(buf); i < end; i++) {
        Rune r = buf_get(buf, i);
        if (r == RUNE_CRLF) {
            file.buf[wrlen++] = '\r';
            file.buf[wrlen++] = '\n';
        } else if (buf->charset == BINARY) {
            file.buf[wrlen++] = (char)r;
        } else {
            wrlen += utf8encode((char*)&(file.buf[wrlen]), r);
        }
    }
    mmap_close(file);
    truncate(buf->path, wrlen);
    buf->modified = false;
}

/*****************************************************************************/

Rune buf_get(Buf* buf, unsigned off) {
    if (off >= buf_end(buf)) return (Rune)'\n';
    size_t bsz = (buf->gapstart - buf->bufstart);
    if (off < bsz)
        return *(buf->bufstart + off);
    else
        return *(buf->gapend + (off - bsz));
}

unsigned buf_end(Buf* buf) {
    size_t bufsz = buf->bufend - buf->bufstart;
    size_t gapsz = buf->gapend - buf->gapstart;
    return (bufsz - gapsz);
}

/*****************************************************************************/

unsigned buf_insert(Buf* buf, bool fmt, unsigned off, Rune rune) {
    buf->modified = true;
    if (fmt && buf->expand_tabs && rune == '\t') {
        size_t n = (TabWidth - ((off - buf_bol(buf, off)) % TabWidth));
        log_insert(buf, &(buf->undo), off, off+n);
        for(; n > 0; n--) off += insert(buf, off, ' ');
    } else {
        size_t n = insert(buf, off, rune);
        if (n > 0) {
            log_insert(buf, &(buf->undo), off, off+n);
            off += n;
        }
    }
    if (fmt && buf->copy_indent && (rune == '\n' || rune == RUNE_CRLF)) {
        unsigned beg = buf_bol(buf, off-1), end = beg;
        for (; end < buf_end(buf) && (' ' == buf_get(buf, end) || '\t' == buf_get(buf, end)); end++);
        for (; beg < end; beg++)
            off = buf_insert(buf, true, off, buf_get(buf, beg));
    }
    log_clear(&(buf->redo));
    return off;
}

unsigned buf_delete(Buf* buf, unsigned beg, unsigned end) {
    buf->modified = true;
    log_clear(&(buf->redo));
    for (unsigned i = end-beg; i > 0; i--) {
        Rune r = buf_get(buf, beg);
        log_delete(buf, &(buf->undo), beg, &r, 1);
        delete(buf, beg);
    }
    return beg;
}

unsigned buf_change(Buf* buf, unsigned beg, unsigned end) {
    /* delete the range first */
    unsigned off = buf_delete(buf, beg, end);
    /* now create a new insert item of length 0 witht he same transaction id as
       the delete. This will cause subsequent inserts to be coalesced into the
       same transaction */
    Log* dellog = buf->undo;
    Log* inslog = (Log*)calloc(sizeof(Log), 1);
    inslog->transid = dellog->transid;
    inslog->insert = true;
    inslog->data.ins.beg = beg;
    inslog->data.ins.end = beg;
    inslog->next = dellog;
    buf->undo = inslog;
    return off;
}

/*****************************************************************************/

void buf_undo(Buf* buf, Sel* sel) {
    if (!buf->undo) return;
    uint transid = buf->undo->transid;
    while (buf->undo && (buf->undo->transid == transid))
        swaplog(buf, &(buf->undo), &(buf->redo), sel);
}

void buf_redo(Buf* buf, Sel* sel) {
    if (!buf->redo) return;
    uint transid = buf->redo->transid;
    while (buf->redo && (buf->redo->transid == transid))
        swaplog(buf, &(buf->redo), &(buf->undo), sel);
}

void buf_loglock(Buf* buf) {
    Log* log = buf->undo;
    if (log && log->transid == buf->transid)
        buf->transid++;
}

void buf_logclear(Buf* buf) {
    log_clear(&(buf->redo));
    log_clear(&(buf->undo));
}

/*****************************************************************************/

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

unsigned buf_lscan(Buf* buf, unsigned pos, Rune r) {
    unsigned off = pos;
    for (; (off > 0) && (r != buf_get(buf, off)); off--);
    return (buf_get(buf, off) == r ? off : pos);
}

unsigned buf_rscan(Buf* buf, unsigned pos, Rune r) {
    unsigned off = pos;
    unsigned end = buf_end(buf);
    for (; (off < end) && (r != buf_get(buf, off)); off++);
    return (buf_get(buf, off) == r ? off : pos);
}

void buf_getword(Buf* buf, bool (*isword)(Rune), Sel* sel) {
    for (; isword(buf_get(buf, sel->beg-1)); sel->beg--);
    for (; isword(buf_get(buf, sel->end));   sel->end++);
    sel->end--;
}

void buf_getblock(Buf* buf, Rune first, Rune last, Sel* sel) {
    int balance = 0, dir;
    unsigned beg = sel->end, end = sel->end, off;

    /* figure out which end of the block we're starting at */
    if (buf_get(buf, end) == first)
        dir = +1, balance++, beg = end++;
    else if (buf_get(buf, end) == last)
        dir = -1, balance--, beg = end--;
    else
        return;

    /* scan for a blanced set of braces */
    while (true) {
        if (buf_get(buf, end) == first)
            balance++;
        else if (buf_get(buf, end) == last)
            balance--;

        if (balance == 0 || end >= buf_end(buf) || end == 0)
            break;
        else
            end += dir;
    }

    /* bail if we failed to find a block */
    if (balance != 0) return;

    /* update the passed in selection */
    if (end > beg) beg++, end--;
    sel->beg = beg, sel->end = end;
}

/*****************************************************************************/

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

unsigned buf_byword(Buf* buf, unsigned off, int count) {
    int move = (count < 0 ? -1 : 1);
    unsigned end = buf_end(buf);
    if (move < 0) {
        for (; off > 0 && !risword(buf_get(buf, off-1)); off--);
        for (; off > 0 && risword(buf_get(buf, off-1)); off--);
    } else {
        for (; off < end && risword(buf_get(buf, off+1)); off++);
        for (; off < end && !risword(buf_get(buf, off+1)); off++);
        if (off < buf_end(buf)) off++;
    }
    return off;
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

/*****************************************************************************/

void buf_find(Buf* buf, int dir, size_t* beg, size_t* end) {
    unsigned dbeg = *beg, dend = *end;
    unsigned mbeg = dbeg+dir, mend = dend+dir;
    unsigned mlen = dend - dbeg;
    while (true) {
        if ((buf_get(buf, mbeg)   == buf_get(buf, dbeg)) &&
            (buf_get(buf, mend-1) == buf_get(buf, dend-1)) &&
            (0 == range_match(buf, dbeg, dend, mbeg, mend)))
        {
            *beg = mbeg;
            *end = mend;
            break;
        }
        mbeg += dir, mend += dir;
        if (mend > buf_end(buf))
            mbeg = (dir < 0 ? buf_end(buf)-mlen : 0), mend = mbeg+mlen;
    }
}

void buf_findstr(Buf* buf, int dir, char* str, size_t* beg, size_t* end) {
    if (!str) return;
    Rune* runes = charstorunes(str);
    size_t rlen = rstrlen(runes);
    unsigned start = *beg, mbeg = start+dir, mend = mbeg + rlen;
    while (mbeg != start) {
        if ((buf_get(buf, mbeg) == runes[0]) &&
            (buf_get(buf, mend-1) == runes[rlen-1]) &&
            (0 == rune_match(buf, mbeg, mend, runes)))
        {
            *beg = mbeg;
            *end = mend;
            break;
        }
        mbeg += dir, mend += dir;
        if (mend > buf_end(buf))
            mbeg = (dir < 0 ? buf_end(buf)-rlen : 0), mend = mbeg+rlen;
    }
    free(runes);
}

/*****************************************************************************/

unsigned buf_setln(Buf* buf, unsigned line) {
    unsigned off = 0;
    while (line > 1 && off < buf_end(buf))
        line--, off = buf_byline(buf, off, DOWN);
    return off;
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

void buf_lastins(Buf* buf, size_t* beg, size_t* end) {
    Log* log = buf->undo;
    unsigned opbeg = *end, opend = *end;
    if (log && log->insert)
        opbeg = log->data.ins.end, opend = log->data.ins.end;
    
    unsigned delsize = 0;
    //printf("start: %u-%u\n", opbeg, opend);
    for (; log; log = log->next) {
        if (log->insert) {
            unsigned ibeg = log->data.ins.beg,
                     iend = log->data.ins.end - delsize;
            //printf("ins: %u-%u\n", ibeg, iend);
            if (iend < ibeg || ibeg > opbeg || iend < opbeg) break;
            if (ibeg < opbeg && iend > opend) break;
            opbeg = ibeg, delsize = 0;
        } else {
            //printf("del: %u-%u\n", log->data.del.off, log->data.del.off+log->data.del.len);
            /* bail if the delete doesnt overlap */
            if(log->data.del.off != opbeg) break;
            delsize = log->data.del.len;
        }
    }
    //printf("finish: %u-%u\n\n", opbeg, opend);
    *beg = opbeg, *end = opend;
}