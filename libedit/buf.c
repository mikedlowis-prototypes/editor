#include <stdc.h>
#include <utf.h>
#include <edit.h>
#include <ctype.h>

unsigned buf_load(Buf* buf, char* path) {
    if (path && path[0] == '.' && path[1] == '/')
        path += 2;
    unsigned off = 0;
    buf->path = stringdup(path);
    char* addr = strrchr(buf->path, ':');
    if (addr) *addr = '\0', addr++;
    if (!strcmp(buf->path,"-")) {
        buf->charset = UTF_8;
        Rune r;
        while (RUNE_EOF != (r = fgetrune(stdin)))
            buf_ins(buf, false, buf_end(buf), r);
    } else {
        FMap file = fmap(buf->path);
        buf->charset = (file.buf ? charset(file.buf, file.len, &buf->crlf) : UTF_8);
        /* load the file contents if it has any */
        if (buf->charset > UTF_8) {
            die("Unsupported character set");
        } else if (buf->charset == BINARY) {
            binload(buf, file);
        } else {
            utf8load(buf, file);
        }
        funmap(file);
        if (addr)
            off = buf_setln(buf, strtoul(addr, NULL, 0));
    }
    buf->modified = false;
    free(buf->undo);
    buf->undo = NULL;
    return off;
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

void log_clear(Log** list) {
    while (*list) {
        Log* deadite = *list;
        *list = (*list)->next;
        if (!deadite->insert)
            free(deadite->data.del.runes);
        free(deadite);
    }
}

void buf_init(Buf* buf) {
    /* cleanup old data if there is any */
    if (buf->bufstart) free(buf->bufstart);
    if (buf->undo) log_clear(&(buf->undo));
    if (buf->redo) log_clear(&(buf->redo));
    
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

static void log_insert(Log** list, unsigned beg, unsigned end) {
    Log* log = *list;
    if (!log || log->locked || !log->insert || (end != log->data.ins.end+1)) {
        Log* newlog  = (Log*)calloc(sizeof(Log), 1);
        newlog->insert = true;
        newlog->data.ins.beg = beg;
        newlog->data.ins.end = end;
        newlog->next = *list;
        if (log) log->locked = true;
        *list = newlog;
    } else if (beg <= log->data.ins.beg) {
        log->data.ins.beg--;
    } else {
        log->data.ins.end++;
    }
}

static void log_delete(Log** list, unsigned off, Rune* r, size_t len) {
    Log* log = *list;
    if (!log || log->locked || log->insert ||
        ((off != log->data.del.off) && (off+1 != log->data.del.off))) {
        Log* newlog = (Log*)calloc(sizeof(Log), 1);
        newlog->insert = false;
        newlog->data.del.off = off;
        newlog->data.del.len = len;
        newlog->data.del.runes = (Rune*)malloc(sizeof(Rune) * len);
        for (size_t i = 0; i < len; i++)
            newlog->data.del.runes[i] = r[i];
        newlog->next = *list;
        if (log) log->locked = true;
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

static void insert(Buf* buf, unsigned off, Rune rune) {
    syncgap(buf, off);
    if (buf->crlf && rune == '\n' && buf_get(buf, off-1) == '\r')
        *(buf->gapstart-1) = RUNE_CRLF;
    else
        *(buf->gapstart++) = rune;
}

static void clear_redo(Buf* buf) {
    Log* log = buf->redo;
    while (log) {
        Log* deadite = log;
        log = deadite->next;
        if (!deadite->insert)
            free(deadite->data.del.runes);
        free(deadite);
    }
    buf->redo = NULL;
}

static unsigned getindent(Buf* buf, unsigned off) {
    off = buf_bol(buf, off);
    for (; off < buf_end(buf) && (' ' == buf_get(buf, off) || '\t' == buf_get(buf, off)); off++);
    return buf_getcol(buf, off) / TabWidth;
}

unsigned buf_ins(Buf* buf, bool fmt, unsigned off, Rune rune) {
    buf->modified = true;
    if (fmt && buf->expand_tabs && rune == '\t') {
        size_t n = (TabWidth - ((off - buf_bol(buf, off)) % TabWidth));
        log_insert(&(buf->undo), off, off+n);
        for(; n > 0; n--) insert(buf, off++, ' ');
    } else {
        log_insert(&(buf->undo), off, off+1);
        insert(buf, off++, rune);
    }
    if (fmt && buf->copy_indent && (rune == '\n' || rune == RUNE_CRLF)) {
        unsigned indent = getindent(buf, off-1);
        for (; indent > 0; indent--)
            off = buf_ins(buf, indent, off, '\t');
    }
    clear_redo(buf);
    return off;
}

static void delete(Buf* buf, unsigned off) {
    syncgap(buf, off);
    buf->gapend++;
}

void buf_del(Buf* buf, unsigned off) {
    buf->modified = true;
    Rune r = buf_get(buf, off);
    log_delete(&(buf->undo), off, &r, 1);
    clear_redo(buf);
    delete(buf, off);
}

unsigned swaplog(Buf* buf, Log** from, Log** to, unsigned pos) {
    /* pop the last action */
    Log* log = *from;
    if (!log) return pos;
    *from = log->next;
    /* invert the log type and move it to the destination */
    Log* newlog = (Log*)calloc(sizeof(Log), 1);
    newlog->locked = true;
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

unsigned buf_setln(Buf* buf, unsigned line) {
    unsigned off = 0;
    while (line > 1 && off < buf_end(buf))
        line--, off = buf_byline(buf, off, DOWN);
    return off;
}

Rune buf_get(Buf* buf, unsigned off) {
    if (off >= buf_end(buf)) return (Rune)'\n';
    size_t bsz = (buf->gapstart - buf->bufstart);
    if (off < bsz)
        return *(buf->bufstart + off);
    else
        return *(buf->gapend + (off - bsz));
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

static int range_match(Buf* buf, unsigned dbeg, unsigned dend, unsigned mbeg, unsigned mend) {
    unsigned n1 = dend-dbeg, n2 = mend-mbeg;
    if (n1 != n2) return n1-n2;
    for (; n1 > 0; n1--, dbeg++, mbeg++) {
        int cmp = buf_get(buf, dbeg) - buf_get(buf, mbeg);
        if (cmp != 0) return cmp;
    }
    return 0;
}

void buf_find(Buf* buf, size_t* beg, size_t* end) {
    unsigned dbeg = *beg, dend = *end;
    unsigned mbeg = dend+1, mend = mbeg + (dend-dbeg);
    while (mend != dbeg) {
        if ((buf_get(buf, mbeg)   == buf_get(buf, dbeg)) &&
            (buf_get(buf, mend-1) == buf_get(buf, dend-1)) &&
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

static Rune* charstorunes(char* str) {
    size_t len = 0;
    Rune* runes = NULL;
    while (str && *str) {
        Rune rune = 0;
        size_t length = 0;
        while (!utf8decode(&rune, &length, *str++));
        runes = realloc(runes, (len + 1) * sizeof(Rune));
        runes[len++] = rune;
    }
    if (runes) {
        runes = realloc(runes, (len + 1) * sizeof(Rune));
        runes[len++] = '\0';
    }
    return runes;
}

static size_t runelen(Rune* runes) {
    size_t len = 0;
    for (; runes[len]; len++);
    return len;
}

static int rune_match(Buf* buf, unsigned mbeg, unsigned mend, Rune* runes) {
    for (; *runes; runes++, mbeg++) {
        int cmp = *runes - buf_get(buf, mbeg);
        if (cmp != 0) return cmp;
    }
    return 0;
}

void buf_findstr(Buf* buf, char* str, size_t* beg, size_t* end) {
    if (!str) return;
    Rune* runes = charstorunes(str);
    size_t rlen = runelen(runes);
    unsigned start = *beg, mbeg = start+1, mend = mbeg + rlen;

    while (mbeg != start) {
        if ((buf_get(buf, mbeg) == runes[0]) &&
            (buf_get(buf, mend-1) == runes[rlen-1]) &&
            (0 == rune_match(buf, mbeg, mend, runes)))
        {
            *beg = mbeg;
            *end = mend;
            break;
        }
        mbeg++, mend++;
        if (mend >= buf_end(buf))
            mbeg = 0, mend = rlen;
    }

    free(runes);
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

unsigned buf_byword(Buf* buf, unsigned off, int count) {
    int move = (count < 0 ? -1 : 1);
    unsigned end = buf_end(buf);
    if (move < 0) {
        for (; off > 0 && !risword(buf_get(buf, off-1)); off--);
        for (; off > 0 && risword(buf_get(buf, off-1)); off--);
    } else {
        for (; off < end && risword(buf_get(buf, off+1)); off++);
        for (; off < end && !risword(buf_get(buf, off+1)); off++);
        off++;
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
    if (log && log->insert) {
        *beg = log->data.ins.beg;
        *end = log->data.ins.end;
    }
}

void buf_loglock(Buf* buf) {
    if (buf->undo)
        buf->undo->locked = true;
}
