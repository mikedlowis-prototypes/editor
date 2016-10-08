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

int charset(const char* buf, size_t len) {
    /* look for a BOM and parse it */
    for (size_t i = 0; i < (sizeof(BOMS)/sizeof(BOMS[0])); i++)
        if (!strncmp(buf, BOMS[i].seq, BOMS[i].len))
            return BOMS[i].type;
    /* look for bytes that are invalid in utf-8 */
    int type = UTF_8;
    for (size_t i = 0; type && (i < len); i++)
        type = Utf8Valid[(int)buf[i]];
    return type;
}
