#include <stdc.h>
#include <utf.h>
#include <edit.h>

static const struct {
    int type;
    int len;
    uchar* seq;
} BOMS[] = {
    { .type = UTF_8,    .len = 3, .seq = (uchar[]){ 0xEFu, 0xBBu, 0xBFu        }},
    { .type = UTF_16BE, .len = 2, .seq = (uchar[]){ 0xFEu, 0xFFu               }},
    { .type = UTF_16LE, .len = 2, .seq = (uchar[]){ 0xFFu, 0xFEu               }},
    { .type = UTF_32BE, .len = 4, .seq = (uchar[]){ 0x00u, 0x00u, 0xFEu, 0xFFu }},
    { .type = UTF_32LE, .len = 4, .seq = (uchar[]){ 0xFFu, 0xFEu, 0x00u, 0x00u }},
};

static const char Utf8Valid[256] = {
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,
};

int charset(const uint8_t* buf, size_t len, int* crlf) {
    size_t crs = 0;
    size_t lfs = 0;
    /* look for a BOM and parse it */
    for (size_t i = 0; i < (sizeof(BOMS)/sizeof(BOMS[0])); i++)
        if (!strncmp((char*)buf, (char*)BOMS[i].seq, BOMS[i].len))
            return BOMS[i].type;
    /* look for bytes that are invalid in utf-8 */
    int type = UTF_8;
    size_t i = 0;
    for (i = 0; i < len; i++) {
        type &= Utf8Valid[(int)buf[i]];
        switch(buf[i]) {
            case '\r': crs++; break;
            case '\n': lfs++; break;
        }
    }
    /* report back the linefeed mode */
    *crlf = (crs == lfs);
    return type;
}

void binload(Buf* buf, FMap file) {
    for (size_t i = 0; i < file.len; i++)
        buf_ins(buf, false, buf_end(buf), file.buf[i]);
}

void binsave(Buf* buf, FILE* file) {
    unsigned end = buf_end(buf);
    for (unsigned i = 0; i < end; i++) {
        Rune r = buf_get(buf, i);
        if (r == RUNE_CRLF) {
            fputc('\r', file);
            fputc('\n', file);
        } else {
            fputc((int)r, file);
        }
    }
}
