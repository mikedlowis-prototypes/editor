#include <stdc.h>
#include <utf.h>
#include <edit.h>
#include <unistd.h>

int ChildIn = -1, ChildOut = -1;
char Buffer[32768];
char* DataBeg = Buffer;
char* DataEnd = Buffer;

static SyntaxSpan* mkspan(size_t beg, size_t end, size_t clr, SyntaxSpan* span);
static void dump_chunk(Buf* buf, size_t beg, size_t end);

SyntaxDef* colors_find(char* path) {
    char* ext = strrchr(path, '.');
    if (!strcmp(ext,".c") || !strcmp(ext,".h"))
        cmdspawn((char*[]){ "tide-hl", "C", NULL }, &ChildIn, &ChildOut);
    return NULL;
}

int read_byte(void) {
    if (DataBeg >= DataEnd) {
        DataBeg = DataEnd = Buffer;
        long nread = read(ChildOut, Buffer, sizeof(Buffer));
        if (nread <= 0) return EOF;
        DataEnd += nread;
    }
    return *(DataBeg++);
}

size_t read_num(void) {
    int c;
    size_t num = 0;
    while (isdigit(c = read_byte()))
        num = (num * 10) + (c - '0');
    return num;
}

SyntaxSpan* colors_scan(SyntaxDef* syntax, SyntaxSpan* spans, Buf* buf, size_t beg, size_t end) {
    SyntaxSpan* firstspan = spans;
    SyntaxSpan* currspan  = spans;
    /* if the engine died, clear all highlights and quit */
    if (ChildIn < 0)
        return colors_rewind(spans, 0);

    /* commence the highlighting */
    if (buf->path && end-beg) {
        dump_chunk(buf, beg, end);
        size_t b = 0, e = 0, c = 0;
        do {
            b = read_num();
            e = read_num();
            c = read_num();
            if (e > 0) {
                c = (c > 15 ? config_get_int(SynNormal + (c >> 4) - 1) : c) & 0xf;
                currspan = mkspan(beg+b, beg+e, c, currspan);
            }
            //printf("(%lu-%lu) %lu,%lu,%lu\n", beg, end, b, e, c);
            if (!firstspan)
                firstspan = currspan;
        } while (e > 0);
        //printf("done\n");
        //fflush(stdout);
    }
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

static void dump_chunk(Buf* buf, size_t beg, size_t end) {
    size_t len = end - beg, wlen = 0;
    char* wbuf = malloc(64 + len * 6u);
    if (len && wbuf) {
        wlen += sprintf(wbuf, "%lu\n", len);
        for (size_t i = beg; i < end; i++) {
            Rune r = buf_get(buf, i);
            if (r == RUNE_CRLF) {
                wbuf[wlen++] = '\r';
                wbuf[wlen++] = '\n';
            } else if (buf->charset == BINARY) {
                wbuf[wlen++] = (char)r;
            } else {
                wlen += utf8encode((char*)&(wbuf[wlen]), r);
            }
        }
        if (write(ChildIn, wbuf, wlen) < 0) {
            /* child process probably died. shut everything down */
            close(ChildIn),  ChildIn  = -1;
            close(ChildOut), ChildOut = -1;
        }
    }
    free(wbuf);
}
