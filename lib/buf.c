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
static void syncgap(Buf* buf, size_t off);
static int bytes_match(Buf* buf, size_t mbeg, size_t mend, char* str);
static Rune nextrune(Buf* buf, size_t off, int move, bool (*testfn)(Rune));
static void selline(Buf* buf);
static void selblock(Buf* buf, Rune first, Rune last);
static size_t pagealign(size_t sz);

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
    buf->bufstart  = malloc(buf->bufsize);
    buf->bufend    = buf->bufstart + buf->bufsize;
    buf->gapstart  = buf->bufstart;
    buf->gapend    = buf->bufend;
    buf->undo      = NULL;
    buf->redo      = NULL;
    buf->selection = (Sel){0,0,0};
    assert(buf->bufstart);
}

void buf_load(Buf* buf, char* path) {
    if (!path) return;
    /* process the file path and address */
    if (path[0] == '.' && path[1] == '/')
        path += 2;
    buf->path = strdup(path);

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

    /* reset buffer state */
    buf->modified = false;
    buf->modtime  = (uint64_t)sb.st_mtime;
    buf_logclear(buf);
}

void buf_reload(Buf* buf) {
    char* path = buf->path;
    buf_init(buf);
    buf_load(buf, path);
}

void buf_save(Buf* buf) {
    if (0 == buf_end(buf)) return;
    char* wptr;
    long fd, nwrite = 0, towrite = 0;
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

size_t buf_end(Buf* buf) {
    size_t bufsz = buf->bufend - buf->bufstart;
    size_t gapsz = buf->gapend - buf->gapstart;
    return (bufsz - gapsz);
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

/******************************************************************************/

static Sel getsel(Buf* buf) {
    size_t temp;
    Sel sel = buf->selection;
    if (sel.end < sel.beg)
        temp = sel.beg, sel.beg = sel.end, sel.end = temp;
    return sel;
}

static void putb(Buf* buf, char b, Sel* p_sel) {
    syncgap(buf, p_sel->end);
    *(buf->gapstart++) = b;
    p_sel->end = p_sel->end + 1u;
    p_sel->beg = p_sel->end;
}

static char getb(Buf* buf, size_t off) {
    if (off >= buf_end(buf)) return '\n'; // TODO: get rid of this hack
    size_t bsz = (buf->gapstart - buf->bufstart);
    if (off < bsz)
        return *(buf->bufstart + off);
    else
        return *(buf->gapend + (off - bsz));
}

int buf_getrat(Buf* buf, size_t off) {
    size_t rlen = 0;
    Rune rune = 0;
    while (!utf8decode(&rune, &rlen, getb(buf, off++)));
    return rune;
}

void buf_putc(Buf* buf, int c) {
    char utf8buf[UTF_MAX+1] = {0};
    (void)utf8encode(utf8buf, c);
    buf_puts(buf, utf8buf);
}

void buf_puts(Buf* buf, char* s) {
    buf_del(buf);
    while (s && *s) putb(buf, *(s++), &(buf->selection));
}

int buf_getc(Buf* buf) {
    return buf_getrat(buf, buf->selection.end);
}

char* buf_gets(Buf* buf) {
    Sel sel = getsel(buf);
    size_t nbytes = sel.end - sel.beg;
    char* str = malloc(nbytes+1);
    for (size_t i = 0; i < nbytes; i++)
        str[i] = getb(buf, sel.beg + i);
    str[nbytes] = '\0';
    return str;
}

void buf_del(Buf* buf) {
    Sel sel = getsel(buf);
    size_t nbytes = sel.end - sel.beg;
    if (nbytes > 0) {
        //char* str = buf_gets(buf, &sel);
        syncgap(buf, sel.beg);
        buf->gapend += nbytes;
        sel.end = sel.beg;
        buf->selection = sel;
        // update log here
        // free(str);
    }
}

/******************************************************************************/

void buf_undo(Buf* buf) {
}

void buf_redo(Buf* buf) {
}

void buf_logclear(Buf* buf) {
    log_clear(&(buf->redo));
    log_clear(&(buf->undo));
}

void buf_lastins(Buf* buf) {
    Sel sel = getsel(buf);
    // Set selection to last inserted text
    buf->selection = sel;
}

static void log_clear(Log** list) {
    while (*list) {
        Log* deadite = *list;
        *list = (*list)->next;
        if (deadite->data)
            free(deadite->data);
        free(deadite);
    }
}

/******************************************************************************/

bool buf_iseol(Buf* buf, size_t off) {
    Rune r = buf_getrat(buf, off);
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

void buf_selline(Buf* buf) {
    Sel sel = getsel(buf);
    sel.beg = buf_bol(buf, sel.end);
    sel.end = buf_eol(buf, sel.end);
    sel.end = buf_byrune(buf, sel.end, RIGHT);
    buf->selection = sel;
}

void buf_selword(Buf* buf, bool (*isword)(Rune)) {
    Sel sel = getsel(buf);
    for (; isword(buf_getrat(buf, sel.beg-1)); sel.beg--);
    for (; isword(buf_getrat(buf, sel.end));   sel.end++);
    buf->selection = sel;
}

void buf_selall(Buf* buf) {
    buf->selection = (Sel){ .beg = 0, .end = buf_end(buf) };
}

void buf_selctx(Buf* buf, bool (*isword)(Rune)) {
    size_t bol = buf_bol(buf, buf->selection.end);
    Rune r = buf_getc(buf);
    if (r == '(' || r == ')')
        selblock(buf, '(', ')');
    else if (r == '[' || r == ']')
        selblock(buf, '[', ']');
    else if (r == '{' || r == '}')
        selblock(buf, '{', '}');
    else if (r == '<' || r == '>')
        selblock(buf, '<', '>');
    else if (buf->selection.end == bol || r == '\n')
        selline(buf);
    else if (risword(r))
        buf_selword(buf, isword);
    else
        buf_selword(buf, risbigword);
    buf_getcol(buf);
}

size_t buf_byrune(Buf* buf, size_t pos, int count) {
    int move = (count < 0 ? -1 : 1);
    count *= move; // remove the sign if there is one
    for (; count > 0; count--) {
        if ((pos > 0 && move < 0) || (pos < buf_end(buf) && move > 0))
            pos += move;
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
                pos = buf_bol(buf, (buf_bol(buf, pos) - 1));
        } else {
            size_t next = (buf_eol(buf, pos) + 1);
            if (next < buf_end(buf))
                pos = next;
        }
    }
    return pos;
}

bool buf_findstr(Buf* buf, int dir, char* str) {
    size_t len = strlen(str);
    size_t start = buf->selection.beg,
           mbeg = (start + dir),
           mend = (mbeg + len);
    while (mbeg != start) {
        if ((getb(buf, mbeg) == str[0]) &&
            (getb(buf, mend-1) == str[len-1]) &&
            (0 == bytes_match(buf, mbeg, mend, str)))
        {
            buf->selection.beg = mbeg, buf->selection.end = mend;
            return true;
        }
        mbeg += dir, mend += dir;
        if (mend > buf_end(buf))
            mbeg = (dir < 0 ? buf_end(buf)-len : 0), mend = mbeg + len;
    }
    return false;
}

static int bytes_match(Buf* buf, size_t mbeg, size_t mend, char* str) {
    for (; *str; str++, mbeg++) {
        int cmp = *str - getb(buf, mbeg);
        if (cmp != 0) return cmp;
    }
    return 0;
}

void buf_setln(Buf* buf, size_t line) {
    size_t curr = 0, end = buf_end(buf);
    while (line > 1 && curr < end) {
        size_t next = buf_byline(buf, curr, DOWN);
        if (curr == next) break;
        line--, curr = next;
    }
    buf->selection.beg = buf->selection.end = curr;
}

void buf_getcol(Buf* buf) {
    Sel sel = buf->selection; //getsel(buf, NULL);
    size_t pos = sel.end, curr = buf_bol(buf, pos);
    for (sel.col = 0; curr < pos; curr = buf_byrune(buf, curr, 1))
        sel.col += runewidth(sel.col, buf_getrat(buf, curr));
    buf->selection = sel;
}

void buf_setcol(Buf* buf) {
    Sel sel = buf->selection; //getsel(buf, NULL);
    size_t bol = buf_bol(buf, sel.end);
    size_t curr = bol, len = 0, i = 0;
    /* determine the length of the line in columns */
    for (; !buf_iseol(buf, curr); curr++)
        len += runewidth(len, buf_getrat(buf, curr));
    /* iterate over the runes until we reach the target column */
    for (sel.end = bol, i = 0; i < sel.col && i < len;) {
        int width = runewidth(i, buf_getrat(buf, sel.end));
        sel.end = buf_byrune(buf, sel.end, 1);
        if (sel.col >= i && sel.col < (i + width))
            break;
        i += width;
    }
    buf->selection = sel;
}

static Rune nextrune(Buf* buf, size_t off, int move, bool (*testfn)(Rune)) {
    bool ret = false;
    size_t end = buf_end(buf);
    if (move < 0 && off > 0)
        ret = testfn(buf_getrat(buf, off-1));
    else if (move > 0 && off < end)
        ret = testfn(buf_getrat(buf, off+1));
    return ret;
}

/******************************************************************************/

size_t buf_selsz(Buf* buf) {
    Sel sel = getsel(buf);
    return sel.end - sel.beg;
}

void buf_selclr(Buf* buf, int dir) {
    if (dir > 0)
        buf->selection.beg = buf->selection.end;
    else
        buf->selection.end = buf->selection.beg;
}

bool buf_insel(Buf* buf, size_t off) {
    Sel sel = getsel(buf);
    return (off >= sel.beg && off < sel.end);
}

char* buf_fetch(Buf* buf, bool (*isword)(Rune), size_t off) {
    if (!buf_insel(buf, off)) {
        buf->selection = (Sel){ .beg = off, .end = off };
        buf_selword(buf, isword);
    }
    return buf_gets(buf);
}

/******************************************************************************/

static void selline(Buf* buf) {
    Sel sel = getsel(buf);
    sel.beg = buf_bol(buf, sel.end);
    sel.end = buf_eol(buf, sel.end);
    sel.end = buf_byrune(buf, sel.end, RIGHT);
    buf->selection = sel;
}

static void selblock(Buf* buf, Rune first, Rune last) {
    Sel sel = getsel(buf);
    int balance = 0, dir;
    size_t beg, end = sel.end;

    /* figure out which end of the block we're starting at */
    if (buf_getrat(buf, end) == first)
        dir = +1, balance++, beg = end++;
    else if (buf_getrat(buf, end) == last)
        dir = -1, balance--, beg = end--;
    else
        return;

    /* scan for a blanced set of braces */
    while (true) {
        if (buf_getrat(buf, end) == first)
            balance++;
        else if (buf_getrat(buf, end) == last)
            balance--;

        if (balance == 0 || end >= buf_end(buf) || end == 0)
            break;
        else
            end += dir;
    }

    /* bail if we failed to find a block */
    if (balance != 0) return;

    /* update the passed in selection */
    if (end > beg) beg++; else end++;
    buf->selection.beg = beg, buf->selection.end = end;
}

static size_t pagealign(size_t sz) {
    size_t pgsize = sysconf(_SC_PAGE_SIZE), alignmask = pgsize - 1;
    if (sz & alignmask)
        sz += pgsize - (sz & alignmask);
    return sz;
}

