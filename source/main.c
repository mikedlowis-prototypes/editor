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

typedef struct {
    Line* line;
    int offset;
} FilePos;

/* Globals
 *****************************************************************************/
static bool ScreenDirty = true;
static File CurrFile = { .name = NULL, .start = NULL, .first = NULL, .last = NULL };
static Pos Curr = { .x = 0, .y = 0 };
static Pos Max  = { .x = 0, .y = 0 };
static FilePos Loc = { .line = NULL, .offset = 0 };

/* Macros
 *****************************************************************************/
#define line_length()    (Loc.line->length)
#define visible_length() (Loc.line->length-2 - Loc.offset)

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
            Loc.line = line;
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
        /* Refresh the screen */
        if (ScreenDirty) {
            clear();
            Line* line = CurrFile.start;
            for (int i = 0; (i < Max.y) && line; i++, line = line->next) {
                if (line->length > Loc.offset) {
                    mvprintw(i, 0, "%s", &(line->text[Loc.offset]));
                }
            }
            refresh();
            ScreenDirty = false;
        }
        /* Place the cursor */
        if (line_length() <= 1)
            x = 0;
        else if (Curr.x > visible_length())
            x = visible_length();
        else
            x = Curr.x;
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
    Curr.x--;
    if (Curr.x < 0) {
        Curr.x = 0;
        Loc.offset--;
        if (Loc.offset < 0)
            Loc.offset = 0;
        ScreenDirty = true;
    } else if (Curr.x > visible_length()) {
        Curr.x = visible_length()-1;
    }
}

static void cursor_down(void)
{
    if (Loc.line->next) {
        Curr.y++;
        if (Curr.y >= Max.y) {
            Curr.y = Max.y-1;
            if (CurrFile.start->next) {
                CurrFile.start = CurrFile.start->next;
                ScreenDirty = true;
            }
        }
        Loc.line = Loc.line->next;
    }
}

static void cursor_up(void)
{
    if (Loc.line->prev) {
        Curr.y--;
        if (Curr.y < 0) {
            Curr.y = 0;
            if (CurrFile.start->prev) {
                CurrFile.start = CurrFile.start->prev;
                ScreenDirty = true;
            }
        }
        Loc.line = Loc.line->prev;
    }
}

static void cursor_right(void)
{
    if ((line_length() > 1) && (Curr.x < visible_length()))
    {
        Curr.x++;
        if (Curr.x >= Max.x) {
            Curr.x = Max.x-1;
            Loc.offset++;
            if (Loc.offset > visible_length())
                Loc.offset = visible_length();
            ScreenDirty = true;
        }
    }
}

static void cursor_home(void)
{
    Curr.x = 0;
    Loc.offset = 0;
    ScreenDirty = true;
}

static void cursor_end(void)
{
    Curr.x = ((line_length() <= 1) ? 0 : visible_length());
    Loc.offset = (line_length() > Max.x) ? line_length()-1 - Max.x : Loc.offset;
    ScreenDirty = true;
}
