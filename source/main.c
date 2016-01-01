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
    Line* first;
    Line* last;
} File;

typedef struct {
    int x;
    int y;
} Pos;

/* Globals
 *****************************************************************************/
File Curr_File = { .name = NULL, .first = NULL, .last = NULL };
Pos Curr = { .x = 0, .y = 0 };
Pos Max = { .x = 0, .y = 0 };

/* Declarations
 *****************************************************************************/
static void setup(void);
static void load(char* fname);
static void edit(void);

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
    while (!feof(file)) {
        Line* line = (Line*)ecalloc(1,sizeof(Line));
        line->text = efreadline(file);
        if (Curr_File.first == NULL) {
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

static void edit(void)
{
    Line* start = Curr_File.first;
    int ch = 0;
    do {
        /* Handle input */
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
                    if(start->next) start = start->next;
                }
                break;
            case KEY_UP:
            case 'k':
                Curr.y--;
                if (Curr.y < 0) {
                    Curr.y = 0;
                    if(start->prev) start = start->prev;
                }
                break;
            case KEY_RIGHT:
            case 'l':
                Curr.x = (Curr.x+1 > Max.x) ? Max.x : Curr.x+1;
                break;
        }
        /* Refresh the screen */
        clear();
        Line* line = start;
        for (int i = 0; (i < Max.y) && line; i++, line = line->next) {
            mvprintw(i, 0, "%s", line->text);
        }
        move(Curr.y, Curr.x);
        refresh();
    } while((ch = getch()) != 'q');
}

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
