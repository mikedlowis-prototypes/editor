/* Includes
 *****************************************************************************/
#include <util.h>

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

/* Globals
 *****************************************************************************/
File Curr_File = { .name = NULL, .first = NULL, .last = NULL };

/* Declarations
 *****************************************************************************/
static void setup(void);
static void cleanup(void);
static void load(char* fname);
static void edit(void);

/* Definitions
 *****************************************************************************/
static void setup(void)
{
}

static void cleanup(void)
{
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
    Line* line = Curr_File.first;
    while (line) {
        printf("%s", line->text);
        line = line->next;
    }
}

/* Main Routine
 *****************************************************************************/
int main(int argc, char** argv)
{
    setup();
    if (argc > 1) {
        load(argv[1]);
        edit();
    } else {
        die("no filename provided");
    }
    cleanup();
    return EXIT_SUCCESS;
}
