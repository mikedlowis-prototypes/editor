#define _GNU_SOURCE
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "edit.h"

typedef struct {
    char* buf;
    size_t len;
} FMap;

static FMap fmap(char* path) {
    int fd;
    FMap file = { .buf = NULL, .len = 0 };
    struct stat sb;
    if (((fd = open(path, O_RDONLY, 0)) < 0) ||
        (fstat(fd, &sb) < 0) ||
        (sb.st_size == 0))
        return file;
    file.buf = (char*)mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
    file.len = sb.st_size;
    if (file.buf == MAP_FAILED)
        die("memory mapping of file failed");
    return file;
}

static void funmap(FMap file) {
    if (file.buf)
        munmap(file.buf, file.len);
}

static void utf8load(Buf* buf, FMap file) {
    for (size_t i = 0; i < file.len;) {
        Rune r = 0;
        size_t len = 0;
        while (!utf8decode(&r, &len, file.buf[i++]));
        buf_ins(buf, buf_end(buf), r);
    }
}

static void utf8save(Buf* buf, FILE* file) {
    unsigned end = buf_end(buf);
    for (unsigned i = 0; i < end; i++)
        fputrune(buf_get(buf, i), file);
}

static void binload(Buf* buf, FMap file) {
    for (size_t i = 0; i < file.len; i++)
        buf_ins(buf, buf_end(buf), file.buf[i]);
}

static void binsave(Buf* buf, FILE* file) {
    unsigned end = buf_end(buf);
    for (unsigned i = 0; i < end; i++)
        fputc((int)buf_get(buf, i), file);
}

void buf_load(Buf* buf, char* path) {
    buf->insert_mode = true;
    if (!strcmp(path,"-")) {
        buf_ins(buf, 0, (Rune)'\n');
    } else {
        FMap file = fmap(path);
        buf->path = strdup(path);
        buf->charset = (file.buf ? charset(file.buf, file.len) : UTF_8);
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
    buf->insert_mode = false;
    buf->modified    = false;
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
    buf->insert_mode = false;
    buf->modified    = false;
    buf->bufsize     = BufSize;
    buf->bufstart    = (Rune*)malloc(buf->bufsize * sizeof(Rune));
    buf->bufend      = buf->bufstart + buf->bufsize;
    buf->gapstart    = buf->bufstart;
    buf->gapend      = buf->bufend;
}

void buf_clr(Buf* buf) {
    free(buf->bufstart);
    buf_init(buf);
}

void buf_del(Buf* buf, unsigned off) {
    if (!buf->insert_mode) { return; }
    buf->modified = true;
    syncgap(buf, off);
    buf->gapend++;
}

void buf_ins(Buf* buf, unsigned off, Rune rune) {
    if (!buf->insert_mode) { return; }
    buf->modified = true;
    syncgap(buf, off);
    *(buf->gapstart++) = rune;
}

Rune buf_get(Buf* buf, unsigned off) {
    if (off >= buf_end(buf)) return (Rune)'\n';
    size_t bsz = (buf->gapstart - buf->bufstart);
    if (off < bsz)
        return *(buf->bufstart + off);
    else
        return *(buf->gapend + (off - bsz));
}

unsigned buf_bol(Buf* buf, unsigned off) {
    for (Rune r; (r = buf_get(buf, off-1)) != '\n'; off--);
    return off;
}

unsigned buf_eol(Buf* buf, unsigned off) {
    for (Rune r; (r = buf_get(buf, off)) != '\n'; off++);
    return off;
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
