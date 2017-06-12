#include <stdc.h>
#include <utf.h>
#include <edit.h>
#include <config.h>

static bool matches(Buf* buf, size_t* off, char* str);
static SyntaxSpan* mkspan(size_t beg, size_t end, size_t clr, SyntaxSpan* span);

static SyntaxDef Syntaxes[] = {
    {
        .name = "Text",
        .extensions = (char*[]){ 0 },
        .rules = (SyntaxRule[]){
            { .oneol = END,  .beg = "#" },
            {0,0,0}
        }
    },
    {
        .name = "C",
        .extensions = (char*[]){
            ".c", ".h", ".C", ".cpp", ".CPP", ".hpp", ".cc", ".c++", ".cxx", 0 },
        .rules = (SyntaxRule[]){
            { .color = 14, .oneol = END,  .beg = "\"", .end = "\"" },
            { .color = 14, .oneol = END,  .beg = "'", .end = "'" },
            { .color =  2, .oneol = END,  .beg = "//" },
            { .color =  2, .oneol = CONT, .beg = "/*", .end = "*/" },
            {0,0,0}
        }

//        .comments = {
//            .line_beg = "//", .multi_beg = "/*", .multi_end = "*/" }

    },

//    {
//        .name = "Ruby",
//        .extensions = (char*[]){ ".rb", 0 },
//        .comments = { .line_beg = "#" }
//    },
//    {
//        .name = "Shell",
//        .extensions = (char*[]){ ".sh", 0 },
//        .comments = { .line_beg = "#" }
//    }

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

bool apply_rule(SyntaxRule* rule, Buf* buf, size_t* off, int* color) {
    bool ret = false;
    size_t end = buf_end(buf);
    if (matches(buf, off, rule->beg)) {
        ret = true;
        while (*off < end) {
            if ((rule->oneol == END || !rule->end) && buf_iseol(buf, *off))
                break;
            else if (matches(buf, off, rule->end))
                break;
            *off = *off + 1;
        }
        *color = rule->color;
    }
    return ret;
}

SyntaxSpan* colors_scan(SyntaxDef* syntax, SyntaxSpan* spans, Buf* buf, size_t beg, size_t end) {
    SyntaxSpan* firstspan = spans;
    SyntaxSpan* currspan  = spans;

    if (!syntax) return firstspan;
    int color = CLR_Comment;
    for (size_t off = beg; off < end; off++) {
        size_t start = off;
        for (SyntaxRule* rules = syntax->rules; rules && rules->beg; rules++)
            if (apply_rule(rules, buf, &off, &color))
                break;
        if (start != off)
            currspan = mkspan(start, --off, color, currspan);
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

