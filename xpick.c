#include <stdc.h>
#include <x11.h>
#include <utf.h>
#include <edit.h>
#include <vec.h>
#include <ctype.h>

static void redraw(int width, int height);
static void mouse_input(MouseAct act, MouseBtn btn, int x, int y);
static void keyboard_input(int mods, uint32_t key);

typedef struct {
    float score;
    char* string;
    size_t length;
    size_t match_start;
    size_t match_end;
} Choice;

static unsigned Pos = 0;
static Buf Query;
static vec_t Choices = {0};
static size_t ChoiceIdx = 0;
static XFont Font;
static XConfig Config = {
    .redraw       = redraw,
    .handle_key   = keyboard_input,
    .handle_mouse = mouse_input,
    .palette      = COLOR_PALETTE
};

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
    unsigned qpos = 0;
    char* s = find_match_start(&string[offset], buf_get(&Query, qpos));
    char* e = s;
    /* bail if no match for first char */
    if (s == NULL) return 0;
    /* find the end of the match */
    for (unsigned bend = buf_end(&Query); qpos < bend; qpos++)
        if ((e = find_match_start(e, buf_get(&Query, qpos))) == NULL)
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
    for (int i = 0; i < vec_size(&Choices); i++) {
        Choice* choice = (Choice*)vec_at(&Choices, i);
        float qlen = (float)buf_end(&Query);
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

static void draw_runes(size_t x, size_t y, int fg, int bg, UGlyph* glyphs, size_t rlen) {
    XGlyphSpec specs[rlen];
    while (rlen) {
        size_t nspecs = x11_font_getglyphs(specs, (XGlyph*)glyphs, rlen, Font, x, y);
        x11_draw_glyphs(fg, bg, specs, nspecs);
        rlen -= nspecs;
    }
}

static void redraw(int width, int height) {
    /* draw the background colors */
    x11_draw_rect(CLR_BASE03, 0, 0, width, height);
    x11_draw_rect(CLR_BASE02, 0, 0, width, x11_font_height(Font));
    x11_draw_rect(CLR_BASE01, 0, x11_font_height(Font), width, 1);
    /* create the array for the query glyphs */
    int rows = height / x11_font_height(Font) - 1;
    int cols = width  / x11_font_width(Font);
    XGlyph glyphs[cols], *text = glyphs;
    /* draw the query */
    unsigned start = 0, end = buf_end(&Query);
    while (start < end && start < cols)
        (text++)->rune = buf_get(&Query, start++);
    draw_runes(0, 0, CLR_BASE3, CLR_BASE03, (UGlyph*)glyphs, text - glyphs);
    /* Draw the choices */
    size_t off = (ChoiceIdx >= rows ? (ChoiceIdx-rows+1) : 0);
    for (size_t i = 0; i < vec_size(&Choices) && i < rows; i++) {
        Choice* choice = vec_at(&Choices, i+off);
        if (i+off == ChoiceIdx) {
            x11_draw_rect(CLR_BASE1, 0, ((i+1) * x11_font_height(Font))+x11_font_descent(Font), width, x11_font_height(Font));
            x11_draw_utf8(Font, CLR_BASE03, CLR_BASE1, 0, (i+2) * x11_font_height(Font), choice->string);
        } else {
            x11_draw_utf8(Font, CLR_BASE1, CLR_BASE03, 0, (i+2) * x11_font_height(Font), choice->string);
        }
    }
}

static void mouse_input(MouseAct act, MouseBtn btn, int x, int y) {
    (void)act;
    (void)btn;
    (void)x;
    (void)y;
}

static void keyboard_input(int mods, uint32_t key) {
    switch (key) {
        case KEY_LEFT:  break;
        case KEY_RIGHT: break;
        case KEY_UP:
            if (ChoiceIdx > 0) ChoiceIdx--;
            break;
        case KEY_DOWN:
            if (ChoiceIdx+1 < vec_size(&Choices)) ChoiceIdx++;
            break;
        case KEY_ESCAPE:
            ChoiceIdx = SIZE_MAX;
            // fall-through
        case '\n': case '\r':
            x11_deinit();
            break;
        case '\b':
            if (Pos > 0)
                buf_del(&Query, --Pos);
            break;
        case RUNE_ERR:
            break;
        default:
            ChoiceIdx = 0;
            buf_ins(&Query, Pos++, key);
            break;
    }
    score();
}

int main(int argc, char** argv) {
    load_choices();
    /* initialize the filter edit buffer */
    buf_init(&Query);
    /* initialize the display engine */
    x11_init(&Config);
    x11_dialog("pick", Width, Height);
    x11_show();
    Font = x11_font_load(FONTNAME);
    x11_loop();
    /* print out the choice */
    if (vec_size(&Choices) && ChoiceIdx != SIZE_MAX) {
        Choice* choice = (Choice*)vec_at(&Choices, ChoiceIdx);
        puts(choice->string);
    }
    return 0;
}
