#include <stdc.h>
#include <x11.h>
#include <utf.h>
#include <edit.h>
#include <win.h>
#include <vec.h>
#include <ctype.h>
#include <shortcuts.h>

typedef struct {
    float score;
    char* string;
    size_t length;
    size_t match_start;
    size_t match_end;
} Choice;

static unsigned Pos = 0;
static vec_t Choices = {0};
static size_t ChoiceIdx = 0;

static char* rdline(FILE* fin) {
    if (feof(fin) || ferror(fin))
        return NULL;
    size_t size  = 256;
    size_t index = 0;
    char*  str   = (char*)malloc(size);
    while (true) {
        char ch = fgetc(fin);
        if (ch == EOF) break;
        str[index++] = ch;
        str[index]   = '\0';
        if (index+1 >= size) {
            size = size << 1;
            str  = realloc(str, size);
        }
        if (ch == '\n') break;
    }
    return str;
}

static int by_score(const void* a, const void* b) {
    Choice* ca = ((Choice*)a);
    Choice* cb = ((Choice*)b);
    if (ca->score < cb->score)
        return 1;
    else if (ca->score > cb->score)
        return -1;
    else
        return strcmp(ca->string, cb->string);
}

static void load_choices(void) {
    char* choice_text;
    Choice choice = {0};
    vec_init(&Choices, sizeof(Choice));
    while ((choice_text = rdline(stdin)) != NULL) {
        choice_text[strlen(choice_text)-1] = '\0';
        if (strlen(choice_text) > 0) {
            choice.string = choice_text;
            choice.length = strlen(choice_text);
            choice.score  = 1.0;
            vec_push_back(&Choices, &choice);
        }
    }
    vec_sort(&Choices, by_score);
}

static char* find_match_start(char *str, int ch) {
    for (; *str; str++)
        if (tolower(*str) == tolower(ch))
            return str;
    return NULL;
}

static bool match(char *string, size_t offset, size_t *start, size_t *end) {
    Buf* buf = win_buf(TAGS);
    unsigned qpos = 0;
    char* s = find_match_start(&string[offset], buf_get(buf, qpos));
    char* e = s;
    /* bail if no match for first char */
    if (s == NULL) return 0;
    /* find the end of the match */
    for (unsigned bend = buf_end(buf); qpos < bend; qpos++)
        if ((e = find_match_start(e, buf_get(buf, qpos))) == NULL)
            return false;
    /* make note of the matching range */
    *start = s - string;
    *end   = e - string;
    /* Less than or equal is used in order to obtain the left-most match. */
    if (match(string, offset + 1, start, end) && (size_t)(e - s) <= *end - *start) {
        *start = s - string;
        *end   = e - string;
    }
    return true;
}

static void score(void) {
    Buf* buf = win_buf(TAGS);
    for (int i = 0; i < vec_size(&Choices); i++) {
        Choice* choice = (Choice*)vec_at(&Choices, i);
        float qlen = (float)buf_end(buf);
        if (match(choice->string, 0, &choice->match_start, &choice->match_end)) {
            float clen = (float)(choice->match_end - choice->match_start);
            choice->score = qlen / clen / (float)(choice->length);
        } else {
            choice->match_start = 0;
            choice->match_end   = 0;
            choice->score       = 0.0f;
        }
    }
    vec_sort(&Choices, by_score);
}

void onmouseleft(WinRegion id, size_t count, size_t row, size_t col) {
}

void onmousemiddle(WinRegion id, size_t count, size_t row, size_t col) {
}

void onmouseright(WinRegion id, size_t count, size_t row, size_t col) {
}

void onscroll(double percent) {
    ChoiceIdx = (size_t)((double)vec_size(&Choices) * percent);
    if (ChoiceIdx >= vec_size(&Choices))
        ChoiceIdx = vec_size(&Choices)-1;
}

void onfocus(bool focused) {
}

void onupdate(void) {
    win_setregion(TAGS);
    win_settext(EDIT, "");
    View* view = win_view(EDIT);
    view->selection = (Sel){0,0,0};
    Sel selection = (Sel){0,0,0};

    score();
    unsigned off = (ChoiceIdx >= view->nrows ? ChoiceIdx-view->nrows+1 : 0);
    for (int i = 0; (i < vec_size(&Choices)) && (i < view->nrows); i++) {
        unsigned beg = view->selection.end;
        Choice* choice = (Choice*)vec_at(&Choices, i+off);
        for (char* str = choice->string; str && *str; str++)
            view_insert(view, false, *str);
        view_insert(view, false, '\n');
        if (ChoiceIdx == i+off) {
            selection.beg = view->selection.end-1;
            selection.end = beg;
        }
    }
    view->selection = selection;
}

void onlayout(void) {
    /* update scroll bar */
    View* view = win_view(EDIT);
    unsigned off = (ChoiceIdx >= view->nrows ? ChoiceIdx-view->nrows+1 : 0);
    double visible = (double)(win_view(EDIT)->nrows);
    double choices = (double)vec_size(&Choices);
    double current = (double)off;
    if (choices > visible)
        win_setscroll(current/choices, visible/choices);
    else
        win_setscroll(0.0, 1.0);
}

void onshutdown(void) {
    x11_deinit();
}

/* Main Routine
 *****************************************************************************/
static void onerror(char* msg) {

}

static void accept(void) {
    x11_deinit();
}

static void reject(void) {
    ChoiceIdx = SIZE_MAX;
    x11_deinit();
}

static void select_up(void) {
    if (ChoiceIdx > 0) ChoiceIdx--;
}

static void select_dn(void) {
    if (ChoiceIdx < vec_size(&Choices)-1) ChoiceIdx++;
}

static KeyBinding Bindings[] = {
    { ModAny, '\b',           backspace    },
    { ModAny, '\n',           accept       },
    { ModCtrl, 'u',           del_to_bol   },
    { ModCtrl, 'k',           del_to_eol   },
    { ModCtrl, 'w',           del_to_bow   },
    { ModCtrl, 'a',           cursor_bol   },
    { ModCtrl, 'e',           cursor_eol   },
    { ModCtrl, 'x',           cut          },
    { ModCtrl, 'c',           copy         },
    { ModCtrl, 'v',           paste        },
    { ModCtrl, 'z',           undo         },
    { ModCtrl, 'y',           redo         },
    { ModAny,  KEY_ESCAPE,    reject       },
    { ModAny,  KEY_DELETE,    delete       },
    { ModAny,  KEY_BACKSPACE, backspace    },
    { ModAny,  KEY_UP,        select_up    },
    { ModAny,  KEY_DOWN,      select_dn    },
    { ModAny,  KEY_LEFT,      cursor_left  },
    { ModAny,  KEY_RIGHT,     cursor_right },
    { ModAny,  KEY_HOME,      cursor_home  },
    { ModAny,  KEY_END,       cursor_end   },
    { 0, 0, 0 }
};

#ifndef TEST
int main(int argc, char** argv) {
    char* title = getenv("XPICKTITLE");
    load_choices();
    if (vec_size(&Choices) > 1) {
        win_dialog("xpick", onerror);
        win_setkeys(Bindings);
        win_settext(STATUS, (title ? title : "xpick"));
        if (argc >= 2) {
            for (char* str = argv[1]; *str; str++)
                buf_insert(win_buf(TAGS), false, Pos++, *str);
            score();
            view_eof(win_view(TAGS), NULL);
        }
        win_loop();
    }
    /* print out the choice */
    if (vec_size(&Choices) && ChoiceIdx != SIZE_MAX) {
        Choice* choice = (Choice*)vec_at(&Choices, ChoiceIdx);
        puts(choice->string);
    }
    return 0;
}
#endif

