/* Utility Functions
 *****************************************************************************/
/* Memory-mapped file representation */
typedef struct {
    uint8_t* buf; /* memory mapped byte buffer */
    size_t len;   /* length of the buffer */
} FMap;

FMap mmap_readonly(char* path);
FMap mmap_readwrite(char* path, size_t sz);
void mmap_close(FMap file);
uint64_t getmillis(void);
char* stringdup(const char* str);
char* fdgets(int fd);
char* chomp(char* in);
uint64_t modtime(char* path);
char* getcurrdir(void);
char* dirname(char* path);
bool try_chdir(char* fpath);
char* strconcat(char* dest, ...);
bool file_exists(char* path);
char* strmcat(char* first, ...);
int daemonize(void);

/* Buffer management functions
 *****************************************************************************/
/* undo/redo list item */
typedef struct Log {
    struct Log* next;   /* pointer to next operation in the stack */
    bool insert;        /* whether this operation was an insert or delete */
    uint transid;       /* transaction id used to group related edits together */
    union {
        struct {
            size_t beg; /* offset in the file where insertion started */
            size_t end; /* offset in the file where insertion ended */
        } ins;
        struct {
            size_t off;  /* offset in the file where the deletion occurred */
            size_t len;  /* number of runes deleted */
            char* runes; /* array of runes containing deleted content */
        } del;
    } data;
} Log;

/* gap buffer main data structure */
typedef struct {
    char* path;           /* the path to the open file */
    uint64_t modtime;     /* modification time of the opened file */
    int charset;          /* the character set of the buffer */
    int crlf;             /* tracks whether the file uses dos style line endings */
    size_t bufsize;       /* size of the buffer in runes */
    char* bufstart;       /* start of the data buffer */
    char* bufend;         /* end of the data buffer */
    char* gapstart;       /* start of the gap */
    char* gapend;         /* end of the gap */
    Log* undo;            /* undo list */
    Log* redo;            /* redo list */
    bool modified;        /* tracks whether the buffer has been modified */
    bool expand_tabs;     /* tracks current mode */
    bool copy_indent;     /* copy the indent level from the previous line on new lines */
    uint transid;         /* tracks the last used transaction id for log entries */
    void (*errfn)(char*); /* callback for error messages */
} Buf;

/* cursor/selection representation */
typedef struct {
    size_t beg;
    size_t end;
    size_t col;
} Sel;

void buf_init(Buf* buf, void (*errfn)(char*));
size_t buf_load(Buf* buf, char* path);
void buf_reload(Buf* buf);
void buf_save(Buf* buf);
Rune buf_get(Buf* buf, size_t pos);
size_t buf_end(Buf* buf);
size_t buf_insert(Buf* buf, bool indent, size_t off, Rune rune);
size_t buf_delete(Buf* buf, size_t beg, size_t end);
size_t buf_change(Buf* buf, size_t beg, size_t end);
void buf_chomp(Buf* buf);
void buf_undo(Buf* buf, Sel* sel);
void buf_redo(Buf* buf, Sel* sel);
void buf_loglock(Buf* buf);
void buf_logclear(Buf* buf);
bool buf_iseol(Buf* buf, size_t pos);
size_t buf_bol(Buf* buf, size_t pos);
size_t buf_eol(Buf* buf, size_t pos);
size_t buf_bow(Buf* buf, size_t pos);
size_t buf_eow(Buf* buf, size_t pos);
size_t buf_lscan(Buf* buf, size_t pos, Rune r);
size_t buf_rscan(Buf* buf, size_t pos, Rune r);
void buf_getword(Buf* buf, bool (*isword)(Rune), Sel* sel);
void buf_getblock(Buf* buf, Rune beg, Rune end, Sel* sel);
size_t buf_byrune(Buf* buf, size_t pos, int count);
size_t buf_byword(Buf* buf, size_t pos, int count);
size_t buf_byline(Buf* buf, size_t pos, int count);
void buf_findstr(Buf* buf, int dir, char* str, size_t* beg, size_t* end);
void buf_lastins(Buf* buf, size_t* beg, size_t* end);
size_t buf_setln(Buf* buf, size_t line);
size_t buf_getln(Buf* buf, size_t off);
size_t buf_getcol(Buf* buf, size_t pos);
size_t buf_setcol(Buf* buf, size_t pos, size_t col);

/* Charset Handling
 *****************************************************************************/
enum {
    BINARY = 0, /* binary encoded file */
    UTF_8,      /* UTF-8 encoded file */

    /* these arent used but are reserved for later */
    UTF_16BE,   /* UTF-16 encoding, big-endian */
    UTF_16LE,   /* UTF-16 encoding, little-endian */
    UTF_32BE,   /* UTF-32 encoding, big-endian */
    UTF_32LE,   /* UTF-32 encoding, little-endian */
};

void filetype(Buf* buf, FMap file);
void utf8load(Buf* buf, FMap file);
void utf8save(Buf* buf, FILE* file);
void binload(Buf* buf, FMap file);
void binsave(Buf* buf, FILE* file);

/* Screen management functions
 *****************************************************************************/
typedef struct {
    uint32_t attr; /* attributes  applied to this cell */
    Rune rune;     /* rune value for the cell */
} UGlyph;

typedef struct {
    size_t off;    /* offset of the first rune in the row */
    size_t rlen;   /* number of runes displayed in the row */
    size_t len;    /* number of screen columns taken up by row */
    UGlyph cols[]; /* row data */
} Row;

typedef struct {
    Buf buffer;         /* the buffer used to populate the view */
    Sel selection;      /* range of currently selected text */
    size_t prev_csr;    /* previous cursor location */
    size_t nrows;       /* number of rows in the view */
    size_t ncols;       /* number of columns in the view */
    Row** rows;         /* array of row data structures */
    int clrnor, clrsel; /* text color pairs for normal and selected text */
    bool sync_needed;   /* whether the view needs to be synced with cursor */
    bool sync_center;   /* cursor should be centered on screen if possible */
} View;

enum {
    LEFT  = -1,
    RIGHT = +1,
    UP    = -1,
    DOWN  = +1
};

void view_init(View* view, char* file, void (*errfn)(char*));
void view_reload(View* view);
size_t view_limitrows(View* view, size_t maxrows, size_t ncols);
void view_resize(View* view, size_t nrows, size_t ncols);
void view_update(View* view, int clrnor, int clrsel, size_t* csrx, size_t* csry);
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
void view_append(View* view, char* str);
char* view_getstr(View* view, Sel* sel);
char* view_getcmd(View* view);
char* view_getctx(View* view);
void view_selctx(View* view);
void view_scroll(View* view, int move);
void view_csrsummon(View* view);
void view_scrollpage(View* view, int move);
void view_setln(View* view, size_t line);
void view_indent(View* view, int dir);
size_t view_selsize(View* view);
void view_selprev(View* view);
void view_setcursor(View* view, size_t row, size_t col, bool extsel);
void view_selextend(View* view, size_t row, size_t col);
void view_selword(View* view, size_t row, size_t col);
void view_select(View* view, size_t row, size_t col);
void view_jumpto(View* view, bool extsel, size_t off);
void view_jumpback(View* view);
void view_scrollto(View* view, size_t csr);
Rune view_getrune(View* view);

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
typedef struct { int fg, bg; } CPair;

extern char TagString[], FontString[];
extern int Palette[16];
extern int WinWidth, WinHeight, LineSpacing, RulerPos, Timeout, TabWidth, ScrollBy,
    ClickTime, MaxScanDist, Syntax, CopyIndent, TrimOnSave, ExpandTabs;
extern CPair Colors[28];

enum { /* Color Variables */
    ClrScrollNor = 0, ClrGutterNor, ClrGutterSel, ClrStatusNor, ClrTagsNor,
    ClrTagsSel, ClrTagsCsr, ClrEditNor, ClrEditSel, ClrEditCsr, ClrEditRul,
    ClrBorders,
    SynNormal, SynComment, SynConstant, SynString, SynChar, SynNumber,
    SynBoolean, SynFloat, SynVariable, SynFunction, SynKeyword, SynOperator,
    SynPreProc, SynType, SynStatement, SynSpecial
};
