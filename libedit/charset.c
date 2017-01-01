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

void filetype(Buf* buf, FMap file) {
    size_t crs = 0, lfs = 0, tabs = 0;
    /* look for a BOM and parse it */
    for (size_t i = 0; i < (sizeof(BOMS)/sizeof(BOMS[0])); i++)
        if (!strncmp((char*)buf, (char*)BOMS[i].seq, BOMS[i].len))
            buf->charset = BOMS[i].type;
    /* look for bytes that are invalid in utf-8 and count tabs, carriage 
       returns,  and line feeds */
    int type = buf->charset;
    for (size_t i = 0; i < file.len; i++) {
        if (type == UTF_8)
            type &= Utf8Valid[(int)file.buf[i]];
        switch(file.buf[i]) {
            case '\r': crs++;  break;
            case '\n': lfs++;  break;
            case '\t': tabs++; break;
        }
    }
    /* setup filetype attributes in the buffer */
    buf->crlf = (crs == lfs);
    buf->charset = type;
    buf->expand_tabs = (tabs == 0);
}
