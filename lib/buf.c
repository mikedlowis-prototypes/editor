#define _POSIX_C_SOURCE 200809L
#include <stdc.h>
#include <utf.h>
#include <edit.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

static void buf_resize(Buf* buf, size_t sz);
static void log_clear(Log** list);
static void log_insert(Buf* buf, Log** list, size_t beg, size_t end);
static void log_delete(Buf* buf, Log** list, size_t off, char* r, size_t len);
static void syncgap(Buf* buf, size_t off);
static void delete(Buf* buf, size_t off);
static size_t insert(Buf* buf, size_t off, Rune rune);
static int rune_match(Buf* buf, size_t mbeg, size_t mend, Rune* runes);
static void swaplog(Buf* buf, Log** from, Log** to, Sel* sel);
static Rune nextrune(Buf* buf, size_t off, int move, bool (*testfn)(Rune));

void buf_init(Buf* buf) {
    /* cleanup old data if there is any */
    if (buf->bufstart) {
        free(buf->bufstart);
        buf->bufstart = NULL;
        buf_logclear(buf);
    }
    /* reset the state to defaults */
    buf->modified  = false;
    buf->bufsize   = 8192;
    buf->bufstart  = (char*)malloc(buf->bufsize);
    buf->bufend    = buf->bufstart + buf->bufsize;
    buf->gapstart  = buf->bufstart;
    buf->gapend    = buf->bufend;
    buf->undo      = NULL;
    buf->redo      = NULL;
    buf->selection = (Sel){0,0,0};
    assert(buf->bufstart);
}

size_t buf_load(Buf* buf, char* path) {
    /* process the file path and address */
    if (path && path[0] == '.' && path[1] == '/')
        path += 2;
    size_t off = 0;
    buf->path = stringdup(path);
    char* addr = strrchr(buf->path, ':');
    if (addr) *addr = '\0', addr++;

    /* load the contents from the file */
    int fd, nread;
    struct stat sb = {0};
    if (((fd = open(path, O_RDONLY, 0)) >= 0) && (fstat(fd, &sb) >= 0) && (sb.st_size > 0)) {
        /* allocate the buffer in advance */
        free(buf->bufstart);
        buf->bufsize  = pagealign(sb.st_size);
        buf->bufstart = malloc(buf->bufsize);
        buf->bufend   = buf->bufstart + buf->bufsize;
        buf->gapstart = buf->bufstart;
        buf->gapend   = buf->bufend;
        /* Read the file into the buffer */
        while (sb.st_size && (nread = read(fd, buf->gapstart, sb.st_size)) > 0)
            buf->gapstart += nread, sb.st_size -= nread;
    }
    if (fd > 0) close(fd);

    /* jump to address if we got one */
    if (addr)
        off = buf_setln(buf, strtoul(addr, NULL, 0));

    /* reset buffer state */
    buf->modified = false;
    buf->modtime  = modtime(buf->path);
    buf_logclear(buf);
    return off;
}

void buf_reload(Buf* buf) {
    char* path = buf->path;
    buf_init(buf);
    buf_load(buf, path);
}

void buf_save(Buf* buf) {
    if (0 == buf_end(buf)) return;
    /* text files should  always end in a new line. If we detected a
       binary file or at least a non-utf8 file, skip this part. */
    if (!buf_iseol(buf, buf_end(buf)-1))
        buf_insert(buf, false, buf_end(buf), '\n');

    char* wptr;
    long fd, nwrite, towrite;
    if (buf->path && (fd = open(buf->path, O_WRONLY|O_CREAT, 0644)) >= 0) {
        /* write the chunk before the gap */
        wptr = buf->bufstart, towrite = (buf->gapstart - buf->bufstart);
        while (towrite && ((nwrite = write(fd, wptr, towrite)) > 0))
            wptr += nwrite, towrite -= nwrite;
        /* write the chunk after the gap */
        wptr = buf->gapend, towrite = (buf->bufend - buf->gapend);
        while (towrite && ((nwrite = write(fd, wptr, towrite)) > 0))
            wptr += nwrite, towrite -= nwrite;
        close(fd);
        /* report success or failure */
        if (nwrite >= 0)
            buf->modified = false;
        //else
        //    buf->errfn("Failed to write file");
    } else {
        //buf->errfn("Need a filename: SaveAs ");
    }
}

Rune buf_get(Buf* buf, size_t off) {
    if (off >= buf_end(buf)) return (Rune)'\n';
    size_t bsz = (buf->gapstart - buf->bufstart);
    if (off < bsz)
        return *(buf->bufstart + off);
    else
        return *(buf->gapend + (off - bsz));
}

size_t buf_end(Buf* buf) {
    size_t bufsz = buf->bufend - buf->bufstart;
    size_t gapsz = buf->gapend - buf->gapstart;
    return (bufsz - gapsz);
}

size_t buf_insert(Buf* buf, bool fmt, size_t off, Rune rune) {
    bool is_eol = (rune == '\n');
    buf->modified = true;
    if (fmt && ExpandTabs && rune == '\t') {
        size_t tabwidth = TabWidth;
        size_t n = (tabwidth - ((off - buf_bol(buf, off)) % tabwidth));
        log_insert(buf, &(buf->undo), off, off+n);
        for(; n > 0; n--) off += insert(buf, off, ' ');
    } else {
        size_t n = insert(buf, off, rune);
        if (n > 0) {
            log_insert(buf, &(buf->undo), off, off+n);
            off += n;
        }
    }
    if (fmt && CopyIndent && is_eol) {
        size_t beg = buf_bol(buf, off-1), end = beg;
        for (; end < buf_end(buf) && (' ' == buf_get(buf, end) || '\t' == buf_get(buf, end)); end++);
        for (; beg < end; beg++)
            off = buf_insert(buf, true, off, buf_get(buf, beg));
    }
    log_clear(&(buf->redo));
    return off;
}

size_t buf_delete(Buf* buf, size_t beg, size_t end) {
    buf->modified = true;
    log_clear(&(buf->redo));
    for (size_t i = end-beg; i > 0; i--) {
        char c = buf_get(buf, beg);
        log_delete(buf, &(buf->undo), beg, &c, 1);
        delete(buf, beg);
    }
    return beg;
}

size_t buf_change(Buf* buf, size_t beg, size_t end) {
    /* delete the range first */
    size_t off = buf_delete(buf, beg, end);
    /* now create a new insert item of length 0 with the same transaction id as
       the delete. This will cause subsequent inserts to be coalesced into the
       same transaction */
    Log* dellog = buf->undo;
    Log* inslog = (Log*)calloc(sizeof(Log), 1);
    inslog->transid = (dellog ? dellog->transid : buf->transid);
    inslog->insert = true;
    inslog->data.ins.beg = beg;
    inslog->data.ins.end = beg;
    inslog->next = dellog;
    buf->undo = inslog;
    return off;
}

void buf_chomp(Buf* buf) {
    size_t end = buf_end(buf);
    if (!end) return;
    Rune r = buf_get(buf, end-1);
    if (r == '\n') {
        delete(buf, end-1);
        if (buf->undo->insert && buf->undo->data.ins.end == end)
            buf->undo->data.ins.end--;
    }
}

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

bool buf_iseol(Buf* buf, size_t off) {
    Rune r = buf_get(buf, off);
    return (r == '\n');
}

size_t buf_bol(Buf* buf, size_t off) {
    for (; !buf_iseol(buf, off-1); off--);
    return off;
}

size_t buf_eol(Buf* buf, size_t off) {
    for (; !buf_iseol(buf, off); off++);
    return off;
}

size_t buf_bow(Buf* buf, size_t off) {
    for (; risword(buf_get(buf, off-1)); off--);
    return off;
}

size_t buf_eow(Buf* buf, size_t off) {
    for (; risword(buf_get(buf, off)); off++);
    return off-1;
}

size_t buf_lscan(Buf* buf, size_t pos, Rune r) {
    size_t off = pos;
    for (; (off > 0) && (r != buf_get(buf, off)); off--);
    return (buf_get(buf, off) == r ? off : pos);
}

size_t buf_rscan(Buf* buf, size_t pos, Rune r) {
    size_t off = pos;
    size_t end = buf_end(buf);
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
    size_t beg, end = sel->end;

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

size_t buf_byrune(Buf* buf, size_t pos, int count) {
    int move = (count < 0 ? -1 : 1);
    count *= move; // remove the sign if there is one
    for (; count > 0; count--) {
        if (move < 0) {
            if (pos > 0) pos--;
        } else {
            if (pos < buf_end(buf)) pos++;
        }
    }
    return pos;
}

size_t buf_byword(Buf* buf, size_t off, int count) {
    int move = (count < 0 ? -1 : 1);

    while (nextrune(buf, off, move, risblank))
        off = buf_byrune(buf, off, move);

    if (nextrune(buf, off, move, risword)) {
        while (nextrune(buf, off, move, risword))
            off = buf_byrune(buf, off, move);
        if (move > 0)
            off = buf_byrune(buf, off, move);
    } else {
        off = buf_byrune(buf, off, move);
    }

    return off;
}

size_t buf_byline(Buf* buf, size_t pos, int count) {
    int move = (count < 0 ? -1 : 1);
    count *= move; // remove the sign if there is one
    for (; count > 0; count--) {
        if (move < 0) {
            if (pos > buf_eol(buf, 0))
                pos = buf_bol(buf, buf_bol(buf, pos)-1);
        } else {
            size_t next = buf_eol(buf, pos)+1;
            if (next < buf_end(buf))
                pos = next;
        }
    }
    return pos;
}

void buf_findstr(Buf* buf, int dir, char* str, size_t* beg, size_t* end) {
    if (!str) return;
    Rune* runes = charstorunes(str);
    size_t rlen = rstrlen(runes);
    size_t start = *beg, mbeg = start+dir, mend = mbeg + rlen;
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

void buf_lastins(Buf* buf, size_t* beg, size_t* end) {
    Log* log = buf->undo;
    size_t opbeg = *end, opend = *end;
    if (log && log->insert)
        opbeg = log->data.ins.end, opend = log->data.ins.end;

    size_t delsize = 0;
    for (; log; log = log->next) {
        if (log->insert) {
            size_t ibeg = log->data.ins.beg,
                     iend = log->data.ins.end - delsize;
            if (iend < ibeg || ibeg > opbeg || iend < opbeg) break;
            if (ibeg < opbeg && iend > opend) break;
            opbeg = ibeg, delsize = 0;
        } else {
            /* bail if the delete doesnt overlap */
            if(log->data.del.off != opbeg) break;
            delsize = log->data.del.len;
        }
    }
    *beg = opbeg, *end = opend;
}

size_t buf_setln(Buf* buf, size_t line) {
    size_t curr = 0, end = buf_end(buf);
    while (line > 1 && curr < end) {
        size_t next = buf_byline(buf, curr, DOWN);
        if (curr == next) break;
        line--, curr = next;
    }
    return curr;
}

size_t buf_getln(Buf* buf, size_t off) {
    size_t line = 1, curr = 0, end = buf_end(buf);
    while (curr < off && curr < end-1) {
        size_t next = buf_byline(buf, curr, DOWN);
        if (curr == next) break;
        line++, curr = next;
    }
    return line;
}

size_t buf_getcol(Buf* buf, size_t pos) {
    size_t curr = buf_bol(buf, pos);
    size_t col = 0;
    for (; curr < pos; curr = buf_byrune(buf, curr, 1))
        col += runewidth(col, buf_get(buf, curr));
    return col;
}

size_t buf_setcol(Buf* buf, size_t pos, size_t col) {
    size_t bol  = buf_bol(buf, pos);
    size_t curr = bol;
    size_t len  = 0;
    size_t i    = 0;
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

static void log_clear(Log** list) {
    while (*list) {
        Log* deadite = *list;
        *list = (*list)->next;
        if (!deadite->insert)
            free(deadite->data.del.runes);
        free(deadite);
    }
}

static void log_insert(Buf* buf, Log** list, size_t beg, size_t end) {
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

static void log_delete(Buf* buf, Log** list, size_t off, char* r, size_t len) {
    Log* log = *list;
    bool locked = (!log || log->transid != buf->transid);
    if (locked || log->insert || ((off != log->data.del.off) && (off+1 != log->data.del.off))) {
        buf_loglock(buf);
        Log* newlog = (Log*)calloc(sizeof(Log), 1);
        newlog->transid = buf->transid;
        newlog->insert = false;
        newlog->data.del.off = off;
        newlog->data.del.len = len;
        newlog->data.del.runes = (char*)malloc(len);
        for (size_t i = 0; i < len; i++)
            newlog->data.del.runes[i] = r[i];
        newlog->next = *list;
        *list = newlog;
    } else if (off == log->data.del.off) {
        log->data.del.len++;
        log->data.del.runes = (char*)realloc(log->data.del.runes, log->data.del.len);
        log->data.del.runes[log->data.del.len-1] = *r;
    } else {
        size_t bytes = sizeof(Rune) * log->data.del.len;
        log->data.del.len++;
        log->data.del.off--;
        log->data.del.runes = (char*)realloc(log->data.del.runes, bytes);
        memmove(log->data.del.runes+1, log->data.del.runes, bytes);
        log->data.del.runes[0] = *r;
    }
}

static void syncgap(Buf* buf, size_t off) {
    assert(off <= buf_end(buf));
    /* If the buffer is full, resize it before syncing */
    if (0 == (buf->gapend - buf->gapstart))
        buf_resize(buf, buf->bufsize << 1);
    /* Move the gap to the desired offset */
    char* newpos = (buf->bufstart + off);
    if (newpos < buf->gapstart)
        while (newpos < buf->gapstart)
            *(--buf->gapend) = *(--buf->gapstart);
    else
        while (newpos > buf->gapstart)
            *(buf->gapstart++) = *(buf->gapend++);
}

static void buf_resize(Buf* buf, size_t sz) {
    /* allocate the new buffer and gap */
    Buf copy = *buf;
    copy.bufsize  = sz;
    copy.bufstart = (char*)malloc(copy.bufsize);
    copy.bufend   = copy.bufstart + copy.bufsize;
    copy.gapstart = copy.bufstart;
    copy.gapend   = copy.bufend;
    /* copy the data from the old buffer to the new one */
    for (char* curr = buf->bufstart; curr < buf->gapstart; curr++)
        *(copy.gapstart++) = *(curr);
    for (char* curr = buf->gapend; curr < buf->bufend; curr++)
        *(copy.gapstart++) = *(curr);
    /* free the buffer and commit the changes */
    free(buf->bufstart);
    memcpy(buf, &copy, sizeof(Buf));
}

static void delete(Buf* buf, size_t off) {
    syncgap(buf, off);
    buf->gapend++;
}

static size_t insert(Buf* buf, size_t off, Rune rune) {
    syncgap(buf, off);
    *(buf->gapstart++) = rune;
    return 1;
}

static int rune_match(Buf* buf, size_t mbeg, size_t mend, Rune* runes) {
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
        newlog->data.del.runes = (char*)malloc(n);
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

static Rune nextrune(Buf* buf, size_t off, int move, bool (*testfn)(Rune)) {
    bool ret = false;
    size_t end = buf_end(buf);
    if (move < 0 && off > 0)
        ret = testfn(buf_get(buf, off-1));
    else if (move > 0 && off < end)
        ret = testfn(buf_get(buf, off+1));
    return ret;
}

/******************************************************************************/

static Sel getsel(Buf* buf, Sel* p_sel) {
    size_t temp;
    Sel sel = (p_sel ? *p_sel : buf->selection);
    if (sel.end < sel.beg)
        temp = sel.beg, sel.beg = sel.end, sel.end = temp;
    return sel;
}

static void setsel(Buf* buf, Sel* p_sel, Sel* p_newsel) {
    if (p_sel)
        *p_sel = *p_newsel;
    else
        buf->selection = *p_newsel;
}

static void putb(Buf* buf, char b, Sel* p_sel) {
    syncgap(buf, p_sel->end);
    *(buf->gapstart++) = b;
    p_sel->end = p_sel->end + 1u;
    p_sel->beg = p_sel->end;
}

static char getb(Buf* buf, size_t off) {
    size_t bsz = (buf->gapstart - buf->bufstart);
    if (off < bsz)
        return *(buf->bufstart + off);
    else
        return *(buf->gapend + (off - bsz));
}

void buf_putc(Buf* buf, int c, Sel* p_sel) {
    char utf8buf[UTF_MAX+1] = {0};
    (void)utf8encode(utf8buf, c);
    buf_puts(buf, utf8buf, p_sel);
}

void buf_puts(Buf* buf, char* s, Sel* p_sel) {
    buf_del(buf, p_sel);
    Sel sel = getsel(buf, p_sel);
    while (s && *s) putb(buf, *(s++), &sel);
    setsel(buf, p_sel, &sel);
}

int buf_getc(Buf* buf, Sel* p_sel) {
    Sel sel = getsel(buf, p_sel);
    size_t rlen = 0;
    Rune rune = 0;
    while (!utf8decode(&rune, &rlen, getb(buf, sel.end++)));
    return rune;
}

char* buf_gets(Buf* buf, Sel* p_sel) {
    Sel sel = getsel(buf, p_sel);
    size_t nbytes = sel.end - sel.beg;
    char* str = malloc(nbytes+1);
    for (size_t i = 0; i < nbytes; i++)
        str[i] = getb(buf, sel.beg + i);
    str[nbytes] = '\0';
    return str;
}

void buf_del(Buf* buf, Sel* p_sel) {
    Sel sel = getsel(buf, p_sel);
    size_t nbytes = sel.end - sel.beg;
    if (nbytes > 0) {
        //char* str = buf_gets(buf, &sel);
        syncgap(buf, sel.beg);
        buf->gapend += nbytes;
        // update log here
        // free(str);
    }
}
