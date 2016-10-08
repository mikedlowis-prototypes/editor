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

FMap fmap(char* path) {
    int fd;
    FMap file;
    struct stat sb;
    if ((fd = open(path, O_RDONLY, 0)) < 0)
        die("could not open file");
    if (fstat(fd, &sb) < 0)
        die("file size could not be determined");
    file.buf = (char*)mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
    file.len = sb.st_size;
    if (file.buf == MAP_FAILED)
        die("memory mapping of file failed");
    return file;
}

void funmap(FMap file) {
    munmap(file.buf, file.len);
}

void buf_load(Buf* buf, char* path) {
    buf->insert_mode = true;
    if (!strcmp(path,"-")) {
        buf_ins(buf, 0, (Rune)'\n');
    } else {
        FMap file = fmap(path);
        int chset = charset(file.buf, file.len);
        if (chset > UTF_8) {
            die("Unsupported character set");
        } else if (chset == BINARY) {
            for (size_t i = 0; i < file.len; i++)
                buf_ins(buf, buf_end(buf), file.buf[i]);
        } else { // UTF-8
            for (size_t i = 0; i < file.len;) {
                Rune r = 0;
                size_t len = 0;
                while (!utf8decode(&r, &len, file.buf[i++]));
                buf_ins(buf, buf_end(buf), r);
            }
        }
        funmap(file);
    }
    buf->insert_mode = false;
}

void buf_save(Buf* buf) {
    if (!buf->path) return;
    unsigned end = buf_end(buf);
    FILE* out = fopen(buf->path, "wb");
    if (!out) return;
    for (unsigned i = 0; i < end; i++)
        fputrune(buf_get(buf, i), out);
    fclose(out);
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
    buf->bufsize  = BufSize;
    buf->bufstart = (Rune*)malloc(buf->bufsize * sizeof(Rune));
    buf->bufend   = buf->bufstart + buf->bufsize;
    buf->gapstart = buf->bufstart;
    buf->gapend   = buf->bufend;
}

void buf_clr(Buf* buf) {
    free(buf->bufstart);
    buf_init(buf);
}

void buf_del(Buf* buf, unsigned off) {
    if (!buf->insert_mode) { return; }
    syncgap(buf, off);
    buf->gapend++;
}

void buf_ins(Buf* buf, unsigned off, Rune rune) {
    if (!buf->insert_mode) { return; }
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
