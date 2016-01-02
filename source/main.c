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
static File Curr_File = { .name = NULL, .first = NULL, .last = NULL };
static Pos Curr = { .x = 0, .y = 0 };
static Pos Max  = { .x = 0, .y = 0 };

/* Declarations
 *****************************************************************************/
static void setup(void);
static void load(char* fname);
static void input(int ch);
static void edit(void);

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
        if (Curr_File.first == NULL) {
            Curr_File.start = line;
            Curr_File.first = line;
            Curr_File.last  = line;
        } else {
            Curr_File.last->next = line;
            line->prev = Curr_File.last;
            Curr_File.last = line;
        }
    }
    fclose(file);
}

static void input(int ch)
{
    switch (ch) {
        case KEY_LEFT:
        case 'h':
            Curr.x = (Curr.x-1 < 0) ? 0 : Curr.x-1;
            break;

        case KEY_DOWN:
        case 'j':
            Curr.y++;
            if (Curr.y > Max.y) {
                Curr.y = Max.y;
                if (Curr_File.start->next) {
                    Curr_File.start = Curr_File.start->next;
                    ScreenDirty = true;
                }
            }
            break;

        case KEY_UP:
        case 'k':
            Curr.y--;
            if (Curr.y < 0) {
                Curr.y = 0;
                if (Curr_File.start->prev) {
                    Curr_File.start = Curr_File.start->prev;
                    ScreenDirty = true;
                }
            }
            break;

        case KEY_RIGHT:
        case 'l':
            Curr.x = (Curr.x+1 > Max.x) ? Max.x : Curr.x+1;
            break;
    }
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
                mvprintw(i, 0, "%s", line->text);
            }
            refresh();
            ScreenDirty = false;
        }
        /* Place the cursor */
        move(Curr.y, Curr.x);
    } while((ch = getch()) != 'q');
}

