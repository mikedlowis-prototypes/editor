/* Includes
 *****************************************************************************/
static void cleanup(void);
#define CLEANUP_HOOK cleanup
#include <util.h>
#include <ncurses.h>

/* Type Definitions
 *****************************************************************************/
typedef struct Line {
    struct Line* prev;
    struct Line* next;
    size_t length;
    char* text;
} Line;

typedef struct {
    char* name;
    Line* start;
    Line* first;
    Line* last;
} File;

typedef struct {
    int x;
    int y;
} Pos;

/* Globals
 *****************************************************************************/
static bool ScreenDirty = true;
static File CurrFile = { .name = NULL, .start = NULL, .first = NULL, .last = NULL };
static Pos Curr = { .x = 0, .y = 0 };
static Pos Max  = { .x = 0, .y = 0 };
static Line* Loc;

/* Macros
 *****************************************************************************/
#define line_length()    (Loc->length)
/* determine the cursor's index within a line:
    if line is long enough, use desired cursor position
    if line is empty (1 character: just a newline character), cursor set to index 0
    if line is not empty but shorter than cursor position, set cursor one character before the trailing newline
*/
#define curr_line_idx()  (line_length() < 2 ? 0 : ((line_length()-2 < Curr.x) ? line_length()-2 : Curr.x))

/* Declarations
 *****************************************************************************/
static void setup(void);
static void load(char* fname);
static void edit(void);
static void input(int ch);
static void cursor_left(void);
static void cursor_down(void);
static void cursor_up(void);
static void cursor_right(void);
static void cursor_home(void);
static void cursor_end(void);

/* Main Routine
 *****************************************************************************/
int main(int argc, char** argv)
{
    atexit(cleanup);
    setup();
    if (argc > 1) {
        load(argv[1]);
        edit();
    } else {
        die("no filename provided");
    }
    return EXIT_SUCCESS;
}

/* Definitions
 *****************************************************************************/
static void setup(void)
{
    initscr();
    getmaxyx(stdscr, Max.y, Max.x);
    raw();
    keypad(stdscr, true);
    noecho();
}

static void cleanup(void)
{
    /* shutdown ncurses */
    endwin();
    /* free up malloced data */
    free(CurrFile.name);
    Line* line = CurrFile.first;
    while (line) {
        Line* deadite = line;
        line = line->next;
        free(deadite->text);
        free(deadite);
    }
}

static void load(char* fname)
{
    FILE* file = efopen(fname, "r");
    CurrFile.name = estrdup(fname);
    while (!feof(file)) {
        Line* line = (Line*)ecalloc(1,sizeof(Line));
        line->text = efreadline(file);
        line->length = strlen(line->text);
        if (CurrFile.first == NULL) {
            CurrFile.start = line;
            CurrFile.first = line;
            CurrFile.last  = line;
            Loc = line;
        } else {
            CurrFile.last->next = line;
            line->prev = CurrFile.last;
            CurrFile.last = line;
        }
    }
    fclose(file);
}

static void edit(void)
{
    int ch = 0, x = 0;
    do {
        /* Handle input */
        input(ch);
        int x = curr_line_idx();
        int r_margin = 5; /* number of characters to keep between cursor and right edge of screen */
        /* note: Max.x stores the size of the window: max allowed index = Max.x-1-r_margin */
        int maxx = Max.x - 1 - r_margin;
        int h_offset = (x > maxx) ? x - maxx : 0;
        if (x > maxx) x = maxx;
        /* Refresh the screen */
        if (ScreenDirty) {
            clear();
            Line* line = CurrFile.start;
            for (int i = 0; (i < Max.y) && line; i++, line = line->next) {
                mvprintw(i, 0, "%.*s", Max.x, (line->length-1 > h_offset) ? &(line->text[h_offset]) : "");
            }
            refresh();
            //ScreenDirty = false; /* always consider screen dirty; TODO: only redraw when necessary */
        }
        /* Place the cursor */
        move(Curr.y, x);
    } while((ch = getch()) != 'q');
}

static void input(int ch)
{
    switch (ch) {
        case KEY_LEFT:
        case 'h':
            cursor_left();
            break;

        case KEY_DOWN:
        case 'j':
            cursor_down();
            break;

        case KEY_UP:
        case 'k':
            cursor_up();
            break;

        case KEY_RIGHT:
        case 'l':
            cursor_right();
            break;

        case KEY_HOME:
        case '0':
            cursor_home();
            break;

        case KEY_END:
        case '$':
            cursor_end();
            break;
    }
}

static void cursor_left(void)
{
    //if(Curr.x > Max.x)
        ScreenDirty = true;
    /* decrease Curr.x then fix if < 0
    don't test if 0 before setting so behavior on blank lines feels more natural */
    Curr.x = curr_line_idx() - 1;
    if (Curr.x < 0) Curr.x = 0;
}

static void cursor_down(void)
{
    if (Loc->next) {
        Curr.y++;
        if (Curr.y >= Max.y) {
            Curr.y = Max.y-1;
            if (CurrFile.start->next) {
                CurrFile.start = CurrFile.start->next;
                ScreenDirty = true;
            }
        }
        Loc = Loc->next;
    }
}

static void cursor_up(void)
{
    if (Loc->prev) {
        Curr.y--;
        if (Curr.y < 0) {
            Curr.y = 0;
            if (CurrFile.start->prev) {
                CurrFile.start = CurrFile.start->prev;
                ScreenDirty = true;
            }
        }
        Loc = Loc->prev;
    }
}

static void cursor_right(void)
{
    if(line_length() > Curr.x + 2)
        Curr.x++;
    //if(Curr.x > Max.x)
    ScreenDirty = true;
}

static void cursor_home(void)
{
    Curr.x = 0;
    ScreenDirty = true;
}

static void cursor_end(void)
{
    Curr.x = line_length() - 2;
    if(Curr.x < 0) Curr.x = 0;
    ScreenDirty = true;
}

