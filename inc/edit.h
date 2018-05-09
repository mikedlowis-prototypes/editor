/* Buffer management functions
 *****************************************************************************/
/* undo/redo list item */
typedef struct Log {
    struct Log* next; /* pointer to next operation in the stack */
    size_t beg;       /* beginning of affected region */
    size_t end;       /* end of affected region*/
    char* data;       /* pointer to deleted character data */
} Log;

/* cursor/selection representation */
typedef struct {
    size_t beg;
    size_t end;
    size_t col;
} Sel;

/* gap buffer main data structure */
typedef struct {
    char* path;       /* the path to the open file */
    uint64_t modtime; /* modification time of the opened file */
    size_t bufsize;   /* size of the buffer in runes */
    char* bufstart;   /* start of the data buffer */
    char* bufend;     /* end of the data buffer */
    char* gapstart;   /* start of the gap */
    char* gapend;     /* end of the gap */
    Log* undo;        /* undo list */
    Log* redo;        /* redo list */
    bool modified;    /* tracks whether the buffer has been modified */
    Sel selection;    /* the currently selected text */
} Buf;

void buf_init(Buf* buf);
void buf_load(Buf* buf, char* path);
void buf_reload(Buf* buf);
void buf_save(Buf* buf);
size_t buf_end(Buf* buf);

int buf_getrat(Buf* buf, size_t off);
void buf_putc(Buf* buf, int c);
void buf_puts(Buf* buf, char* s);
int buf_getc(Buf* buf);
char* buf_gets(Buf* buf);
void buf_del(Buf* buf);

void buf_undo(Buf* buf);
void buf_redo(Buf* buf);
void buf_logclear(Buf* buf);
void buf_lastins(Buf* buf);

bool buf_iseol(Buf* buf, size_t pos);
size_t buf_bol(Buf* buf, size_t pos);
size_t buf_eol(Buf* buf, size_t pos);

void buf_selword(Buf* buf, bool (*isword)(Rune));
void buf_selall(Buf* buf);
void buf_selctx(Buf* buf, bool (*isword)(Rune));

size_t buf_byrune(Buf* buf, size_t pos, int count);
size_t buf_byword(Buf* buf, size_t pos, int count);
size_t buf_byline(Buf* buf, size_t pos, int count);
bool buf_findstr(Buf* buf, int dir, char* str);

void buf_setln(Buf* buf, size_t line);
void buf_getcol(Buf* buf);
void buf_setcol(Buf* buf);

size_t buf_selsz(Buf* buf);
void buf_selclr(Buf* buf, int dir);
bool buf_insel(Buf* buf, size_t off);
char* buf_fetch(Buf* buf, bool (*isword)(Rune), size_t off);

/* Screen management functions
 *****************************************************************************/
typedef struct {
    size_t off;   /* offset of the rune in the buffer */
    size_t width; /* width of the glyph on screen */
    Rune rune;    /* rune value for the cell */
} UGlyph;

typedef struct {
    size_t off;    /* offset of the first rune in the row */
    size_t len;    /* number of runes displayed in the row */
    UGlyph cols[]; /* row data */
} Row;

typedef struct {
    enum {
        CURSOR = (1 << 0),
        CENTER = (1 << 1),
    } sync_flags;
    Buf buffer;   /* the buffer used to populate the view */
    size_t index;
    size_t width;
    size_t nvisible;
    size_t nrows; /* number of rows and columns in the view */
    Row** rows;   /* array of row data structures */
} View;

enum {
    LEFT  = -1,
    RIGHT = +1,
    UP    = -1,
    DOWN  = +1
};

void view_init(View* view, char* file);
void view_reload(View* view);
size_t view_limitrows(View* view, size_t maxrows);
void view_resize(View* view, size_t width, size_t nrows);
void view_update(View* view, size_t* csrx, size_t* csry);
Row* view_getrow(View* view, size_t row);
void view_byrune(View* view, int move, bool extsel);
void view_byword(View* view, int move, bool extsel);
void view_byline(View* view, int move, bool extsel);
char* view_fetch(View* view, size_t row, size_t col, bool (*isword)(Rune));
bool view_findstr(View* view, int dir, char* str);
void view_insert(View* view, bool indent, Rune rune);
void view_delete(View* view, int dir, bool byword);
void view_bol(View* view, bool extsel);
void view_eol(View* view, bool extsel);
void view_bof(View* view, bool extsel);
void view_eof(View* view, bool extsel);
void view_undo(View* view);
void view_redo(View* view);
void view_putstr(View* view, char* str);
char* view_getstr(View* view);
char* view_getcmd(View* view);
char* view_getword(View* view);
char* view_getctx(View* view);
void view_selctx(View* view);
void view_scroll(View* view, int move);
void view_scrollpage(View* view, int move);
void view_setln(View* view, size_t line);
size_t view_selsize(View* view);
void view_selprev(View* view);
void view_setcursor(View* view, size_t row, size_t col, bool extsel);
void view_selextend(View* view, size_t row, size_t col);
void view_selword(View* view, size_t row, size_t col);
void view_select(View* view, size_t row, size_t col);
void view_jumpto(View* view, bool extsel, size_t off);
void view_scrollto(View* view, size_t csr);
Rune view_getrune(View* view);
void view_selectall(View* view);
void view_selectobj(View* view, bool (*istype)(Rune));

/* Command Executions
 *****************************************************************************/
typedef struct Job Job;

typedef void (*jobfn_t)(Job* job);

struct Job {
    Job *next;
    int pid, fd;
    void *data;
    void (*writefn)(Job *job);
    void (*readfn)(Job *job);
};

bool job_poll(int ms);
void job_spawn(int fd, jobfn_t readfn, jobfn_t writefn, void* data);
void job_create(char** cmd, jobfn_t readfn, jobfn_t writefn, void* data);
void job_start(char** cmd, char* data, size_t ndata, View* dest);

/* Configuration Data
 *****************************************************************************/
enum { OFF = 0, ON = 1 };

enum { /* Color Names */
    Red, Green, Yellow, Blue, Purple, Aqua, Orange,
    EditBg, EditFg, EditSel, EditCsr,
    TagsBg, TagsFg, TagsSel, TagsCsr,
    ScrollBg, ScrollFg,
    VerBdr, HorBdr,
    ClrCount
};

extern char *TagString, *FontString;
extern int Palette[ClrCount];
extern int WinWidth, WinHeight, ScrollWidth, LineSpacing, Timeout, TabWidth, ScrollBy,
    ClickTime, MaxScanDist, Syntax, CopyIndent, TrimOnSave, ExpandTabs, DosLineFeed;
