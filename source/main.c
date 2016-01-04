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
static File Curr_File = { .name = NULL, .start = NULL, .first = NULL, .last = NULL };
static Pos Curr = { .x = 0, .y = 0 };
static Pos Max  = { .x = 0, .y = 0 };
static FilePos Loc = { .line = NULL, .offset = 0 };

/* Declarations
 *****************************************************************************/
static void setup(void);
static void load(char* fname);
static void edit(void);
static void input(int ch);
static void cursorLeft(void);
static void cursorDown(void);
static void cursorUp(void);
static void cursorRight(void);
static void cursorHome(void);
static void cursorEnd(void);

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
    free(Curr_File.name);
    Line* line = Curr_File.first;
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
    Curr_File.name = estrdup(fname);
    while (!feof(file)) {
        Line* line = (Line*)ecalloc(1,sizeof(Line));
        line->text = efreadline(file);
        line->length = strlen(line->text);
        if (Curr_File.first == NULL) {
            Curr_File.start = line;
            Curr_File.first = line;
            Curr_File.last  = line;
            Loc.line = line;
        } else {
            Curr_File.last->next = line;
            line->prev = Curr_File.last;
            Curr_File.last = line;
        }
    }
    fclose(file);
}

static void edit(void)
{
    int ch = 0;
    do {
        /* Handle input */
        input(ch);
        /* Refresh the screen */
        if (ScreenDirty) {
            clear();
            Line* line = Curr_File.start;
            for (int i = 0; (i < Max.y) && line; i++, line = line->next) {
                if (line->length > Loc.offset) {
                    mvprintw(i, 0, "%s", &(line->text[Loc.offset]));
                }
            }
            refresh();
            ScreenDirty = false;
        }
        /* Place the cursor */
        /* Cap the column selection at the end of text on the current line */
        int x = Curr.x;
        if (Loc.line->length <= 1)
            x = 0;
        else if (x >= (Loc.line->length-1 - Loc.offset))
            x = (Loc.line->length-2 - Loc.offset);
        move(Curr.y, x);
    } while((ch = getch()) != 'q');
}

static void input(int ch)
{
    switch (ch) {
        case KEY_LEFT:
        case 'h':
            cursorLeft();
            break;

        case KEY_DOWN:
        case 'j':
            cursorDown();
            break;

        case KEY_UP:
        case 'k':
            cursorUp();
            break;

        case KEY_RIGHT:
        case 'l':
            cursorRight();
            break;

        case KEY_HOME:
        case '0':
            cursorHome();
            break;

        case KEY_END:
        case '$':
            cursorEnd();
            break;
    }
}

static void cursorLeft()
{
    Curr.x--;
    if (Curr.x < 0) {
        Curr.x = 0;
        Loc.offset--;
        if (Loc.offset < 0)
            Loc.offset = 0;
        ScreenDirty = true;
    }
}

static void cursorDown()
{
    Curr.y++;
    if (Curr.y >= Max.y) {
        Curr.y = Max.y-1;
        if (Curr_File.start->next) {
            Curr_File.start = Curr_File.start->next;
            ScreenDirty = true;
        }
    }
    if (Loc.line->next)
        Loc.line = Loc.line->next;
}

static void cursorUp()
{
    Curr.y--;
    if (Curr.y < 0) {
        Curr.y = 0;
        if (Curr_File.start->prev) {
            Curr_File.start = Curr_File.start->prev;
            ScreenDirty = true;
        }
    }
    if (Loc.line->prev)
        Loc.line = Loc.line->prev;
}

static void cursorRight()
{
    Curr.x++;
    if (Curr.x >= Max.x) {
        Curr.x = Max.x-1;
        Loc.offset++;
        if (Loc.offset >= Loc.line->length-1)
            Loc.offset = Loc.line->length-2;
        ScreenDirty = true;
    }
}

static void cursorHome()
{
    if(Curr.x != 0){
        Curr.x = 0;
        ScreenDirty = true;
    }
}

static void cursorEnd()
{
    if (Loc.line->length <= 1)
        Curr.x = 0;
    else
        Curr.x = (Loc.line->length-2 - Loc.offset);
    ScreenDirty = true;
}

