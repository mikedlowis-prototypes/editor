#include "edit.h"

static const struct {
    int type;
    int len;
    char* seq;
} BOMS[] = {
    { .type = UTF_8,    .len = 3, .seq = (char[]){ 0xEF, 0xBB, 0xBF       }},
    { .type = UTF_16BE, .len = 2, .seq = (char[]){ 0xFE, 0xFF             }},
    { .type = UTF_16LE, .len = 2, .seq = (char[]){ 0xFF, 0xFE             }},
    { .type = UTF_32BE, .len = 4, .seq = (char[]){ 0x00, 0x00, 0xFE, 0xFF }},
    { .type = UTF_32LE, .len = 4, .seq = (char[]){ 0xFF, 0xFE, 0x00, 0x00 }},
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

int charset(const uint8_t* buf, size_t len) {
    /* look for a BOM and parse it */
    for (size_t i = 0; i < (sizeof(BOMS)/sizeof(BOMS[0])); i++)
        if (!strncmp((char*)buf, BOMS[i].seq, BOMS[i].len))
            return BOMS[i].type;
    /* look for bytes that are invalid in utf-8 */
    int type = UTF_8;
    size_t i = 0;
    for (i = 0; type && (i < len); i++)
        type = Utf8Valid[(int)buf[i]];
    return type;
}

void binload(Buf* buf, FMap file) {
    for (size_t i = 0; i < file.len; i++)
        buf_ins(buf, buf_end(buf), file.buf[i]);
}

void binsave(Buf* buf, FILE* file) {
    unsigned end = buf_end(buf);
    for (unsigned i = 0; i < end; i++)
        fputc((int)buf_get(buf, i), file);
}
