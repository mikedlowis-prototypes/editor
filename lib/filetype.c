#include <stdc.h>
#include <utf.h>
#include <edit.h>

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
    if (crs || lfs)
        buf->crlf = (crs == lfs);
    buf->charset = type;
    buf->expand_tabs = (tabs == 0);
}
