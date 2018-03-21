#include <stdc.h>
#include <utf.h>
#include <edit.h>
#include <unistd.h>
#include <ctype.h>

static int ChildIn = -1, ChildOut = -1;
static char Buffer[32768];
static char* DataBeg = Buffer;
static char* DataEnd = Buffer;

static SyntaxSpan* mkspan(size_t beg, size_t end, size_t clr, SyntaxSpan* span);
static void write_chunk(Buf* buf, size_t beg, size_t end);
static int read_byte(void);
static int read_num(void);

void colors_init(char* path) {
    if (Syntax)
        exec_spawn((char*[]){ "tide-hl.rb", path, NULL }, &ChildIn, &ChildOut);
}

SyntaxSpan* colors_scan(SyntaxSpan* spans, Buf* buf, size_t beg, size_t end) {
    SyntaxSpan* firstspan = spans;
    SyntaxSpan* currspan  = spans;
    /* if the engine died, clear all highlights and quit */
    if (ChildIn < 0 || !buf->path)
        return colors_rewind(spans, 0);
#if 0
    /* commence the highlighting */
    if (beg < end) {
        write_chunk(buf, beg, end);
        size_t b = 0, e = 0, c = 0;
        do {
            b = read_num();
            e = read_num();
            c = read_num();
            if (e > 0 && c > 0) {
                c = (c > 15 ? config_get_int(SynNormal + (c >> 4) - 1) : c) & 0xf;
                currspan = mkspan(beg+b, beg+e-1, c, currspan);
            }
            if (!firstspan)
                firstspan = currspan;
        } while (e > 0);
        fflush(stdout);
        DataBeg = DataEnd = Buffer;
    }
#endif
    return firstspan;
}

SyntaxSpan* colors_rewind(SyntaxSpan* spans, size_t first) {
    SyntaxSpan *curr = spans, *next = (spans ? spans->next : NULL);
    while (curr && curr->end > first)
        next = curr, curr = curr->prev;

    if (curr) curr->next = NULL;

    for (SyntaxSpan* span = next; span;) {
        SyntaxSpan* dead = span;
        span = dead->next;
        if (span) span->prev = NULL;
        free(dead);
    }
    return curr;
}

static SyntaxSpan* mkspan(size_t beg, size_t end, size_t clr, SyntaxSpan* span) {
    SyntaxSpan* newspan = malloc(sizeof(SyntaxSpan));
    newspan->beg   = beg;
    newspan->end   = end;
    newspan->color = clr;
    newspan->prev  = span;
    newspan->next  = NULL;
    if (span)
        span->next = newspan;
    return newspan;
}

static void write_chunk(Buf* buf, size_t beg, size_t end) {
    size_t len = end - beg, wlen = 0;
    char* wbuf = malloc(64 + len * 6u);
    if (len && wbuf) {
        wlen += sprintf(wbuf, "%lu\n", len);
        for (size_t i = beg; i < end; i++) {
            Rune r = buf_get(buf, i);
            if (r == RUNE_CRLF) {
                wbuf[wlen++] = '\n';
            } else if (buf->charset == BINARY) {
                wbuf[wlen++] = (char)r;
            } else {
                wlen += utf8encode((char*)&(wbuf[wlen]), r);
            }
        }
        long nwrite = write(ChildIn, wbuf, wlen);
        if (nwrite < 0) {
            /* child process probably died. shut everything down */
            close(ChildIn),  ChildIn  = -1;
            close(ChildOut), ChildOut = -1;
        }
    }
    free(wbuf);
}

static int read_byte(void) {
    if (DataBeg >= DataEnd) {
        DataBeg = DataEnd = Buffer;
        long nread = read(ChildOut, Buffer, sizeof(Buffer));
        if (nread <= 0) return EOF;
        DataEnd += nread;
    }
    return *(DataBeg++);
}

static int read_num(void) {
    int c, num = 0;
    while (isdigit(c = read_byte()))
        num = (num * 10) + (c - '0');
    return num;
}
