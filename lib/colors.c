#include <stdc.h>
#include <utf.h>
#include <edit.h>
#include <config.h>

static SyntaxDef Syntaxes[] = {
    {
        .name = "Text",
        .extensions = (char*[]){ 0 },
        .comments = { .line_beg = "#" }
    },
    {
        .name = "C",
        .extensions = (char*[]){
            ".c", ".h", ".C", ".cpp", ".CPP", ".hpp", ".cc", ".c++", ".cxx", 0 },
        .comments = {
            .line_beg = "//", .multi_beg = "/*", .multi_end = "*/" }
    },
    {
        .name = "Ruby",
        .extensions = (char*[]){ ".rb", 0 },
        .comments = { .line_beg = "#" }
    },
    {
        .name = "Shell",
        .extensions = (char*[]){ ".sh", 0 },
        .comments = { .line_beg = "#" }
    }
};

SyntaxDef* colors_find(char* path) {
    char* ext = strrchr(path, '.');
    for (int i = 0; i < nelem(Syntaxes); i++) {
        char** synext = Syntaxes[i].extensions;
        for (; ext && *synext; synext++) {
            if (!strcmp(*synext, ext))
                return &Syntaxes[i];
        }
    }
    return &Syntaxes[0];
}

static bool matches(Buf* buf, size_t* off, char* str) {
    size_t curr = *off;
    if (str) {
        while (*str && *str == buf_get(buf, curr))
            curr++, str++;
        if (*str == '\0') {
            *off = curr;
            return true;
        }
    }
    return false;
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

SyntaxSpan* colors_scan(SyntaxDef* syntax, SyntaxSpan* spans, Buf* buf, size_t beg, size_t end) {
    SyntaxSpan* firstspan = spans;
    SyntaxSpan* currspan  = spans;

    if (!syntax) return firstspan;
    for (size_t off = beg; off < end; off++) {
        size_t start = off;
        if (matches(buf, &off, syntax->comments.line_beg))
            for (; off < end && !buf_iseol(buf, off); off++);
        else if (matches(buf, &off, syntax->comments.multi_beg))
            for (; off < end && !matches(buf, &off, syntax->comments.multi_end); off++);
        if (start != off)
            currspan = mkspan(start, --off, CLR_Comment, currspan);
        if (!firstspan && currspan)
            firstspan = currspan;
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

