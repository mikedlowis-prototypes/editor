/* Utility Functions
 *****************************************************************************/
typedef struct {
    uint8_t* buf; /* memory mapped byte buffer */
    size_t len;   /* length of the buffer */
} FMap;

FMap fmap(char* path);
void funmap(FMap file);
uint64_t getmillis(void);
bool risword(Rune r);
bool risblank(Rune r);
char* stringdup(const char* str);
char* fdgets(int fd);
char* chomp(char* in);

/* Buffer management functions
 *****************************************************************************/
typedef struct Log {
    struct Log* next;   /* pointer to next operation in the stack */
    bool locked;        /* whether new operations can be coalesced or not */
    bool insert;        /* whether this operation was an insert or delete */
    union {
        struct {
            size_t beg; /* offset in the file where insertion started */
            size_t end; /* offset in the file where insertion ended */
        } ins;
        struct {
            size_t off;  /* offset in the file where the deletion occurred */
            size_t len;  /* number of runes deleted */
            Rune* runes; /* array of runes containing deleted content */
        } del;
    } data;
} Log;

typedef struct buf {
    char* path;       /* the path to the open file */
    int charset;      /* the character set of the buffer */
    int crlf;         /* tracks whether the file uses dos style line endings */
    size_t bufsize;   /* size of the buffer in runes */
    Rune* bufstart;   /* start of the data buffer */
    Rune* bufend;     /* end of the data buffer */
    Rune* gapstart;   /* start of the gap */
    Rune* gapend;     /* end of the gap */
    Log* undo;        /* undo list */
    Log* redo;        /* redo list */
    bool modified;    /* tracks whether the buffer has been modified */
    bool expand_tabs; /* tracks current mode */
    bool copy_indent; /* copy the indent level from the previous line on new lines */
} Buf;

typedef struct {
    size_t beg;
    size_t end;
    size_t col;
} Sel;

unsigned buf_load(Buf* buf, char* path);
void buf_save(Buf* buf);
void buf_init(Buf* buf);
unsigned buf_ins(Buf* buf, bool indent, unsigned off, Rune rune);
void buf_del(Buf* buf, unsigned pos);
unsigned buf_undo(Buf* buf, unsigned pos);
unsigned buf_redo(Buf* buf, unsigned pos);
unsigned buf_setln(Buf* buf, unsigned line);
Rune buf_get(Buf* buf, unsigned pos);
bool buf_iseol(Buf* buf, unsigned pos);
unsigned buf_bol(Buf* buf, unsigned pos);
unsigned buf_eol(Buf* buf, unsigned pos);
unsigned buf_bow(Buf* buf, unsigned pos);
unsigned buf_eow(Buf* buf, unsigned pos);
unsigned buf_lscan(Buf* buf, unsigned pos, Rune r);
unsigned buf_rscan(Buf* buf, unsigned pos, Rune r);
void buf_find(Buf* buf, size_t* beg, size_t* end);
void buf_findstr(Buf* buf, char* str, size_t* beg, size_t* end);
unsigned buf_end(Buf* buf);
unsigned buf_byrune(Buf* buf, unsigned pos, int count);
unsigned buf_byword(Buf* buf, unsigned pos, int count);
unsigned buf_byline(Buf* buf, unsigned pos, int count);
unsigned buf_getcol(Buf* buf, unsigned pos);
unsigned buf_setcol(Buf* buf, unsigned pos, unsigned col);
void buf_lastins(Buf* buf, size_t* beg, size_t* end);
void buf_loglock(Buf* buf);

/* Charset Handling
 *****************************************************************************/
enum {
    BINARY = 0, /* binary encoded file */
    UTF_8,      /* UTF-8 encoded file */
    UTF_16BE,   /* UTF-16 encoding, big-endian */
    UTF_16LE,   /* UTF-16 encoding, little-endian */
    UTF_32BE,   /* UTF-32 encoding, big-endian */
    UTF_32LE,   /* UTF-32 encoding, little-endian */
};

int charset(const uint8_t* buf, size_t len, int* crlf);
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
    bool sync_needed; /* determines whether the view needs to be synced with cursor */
    size_t nrows;     /* number of rows in the view */
    size_t ncols;     /* number of columns in the view */
    Row** rows;       /* array of row data structures */
    Buf buffer;       /* the buffer used to populate the view */
    Sel selection;    /* range of currently selected text */
} View;

enum {
    LEFT  = -1,
    RIGHT = +1,
    UP    = -1,
    DOWN  = +1
};

void view_init(View* view, char* file);
size_t view_limitrows(View* view, size_t maxrows, size_t ncols);
void view_resize(View* view, size_t nrows, size_t ncols);
void view_update(View* view, size_t* csrx, size_t* csry);
Row* view_getrow(View* view, size_t row);
void view_byrune(View* view, int move, bool extsel);
void view_byword(View* view, int move, bool extsel);
void view_byline(View* view, int move, bool extsel);
void view_setcursor(View* view, size_t row, size_t col);
void view_selext(View* view, size_t row, size_t col);
void view_selword(View* view, size_t row, size_t col);
void view_selprev(View* view);
void view_select(View* view, size_t row, size_t col);
char* view_fetch(View* view, size_t row, size_t col);
void view_find(View* view, size_t row, size_t col);
void view_findstr(View* view, char* str);
void view_insert(View* view, Rune rune);
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
void view_scroll(View* view, int move);
void view_scrollpage(View* view, int move);

/* Command Executions
 *****************************************************************************/
int cmdrun(char** cmd, char** err);
char* cmdread(char** cmd, char** err);
void cmdwrite(char** cmd, char* text, char** err);
char* cmdwriteread(char** cmd, char* text, char** err);

/* Color Scheme Handling
 *****************************************************************************/
/* color indexes for the colorscheme */
enum ColorId {
    CLR_BASE03  = 0,
    CLR_BASE02  = 1,
    CLR_BASE01  = 2,
    CLR_BASE00  = 3,
    CLR_BASE0   = 4,
    CLR_BASE1   = 5,
    CLR_BASE2   = 6,
    CLR_BASE3   = 7,
    CLR_YELLOW  = 8,
    CLR_ORANGE  = 9,
    CLR_RED     = 10,
    CLR_MAGENTA = 11,
    CLR_VIOLET  = 12,
    CLR_BLUE    = 13,
    CLR_CYAN    = 14,
    CLR_GREEN   = 15,
    CLR_COUNT   = 16
};

/* Global State
 *****************************************************************************/
typedef struct {
    int mods;
    Rune key;
    void (*action)(void);
} KeyBinding;

typedef struct {
    uint64_t time;
    uint8_t count;
    bool pressed;
    int region;
} ButtonState;

typedef struct {
    size_t x;
    size_t y;
    size_t height;
    size_t width;
    bool warp_ptr;
    View view;
} Region;

typedef struct {
    char* tag;
    union {
        void (*noarg)(void);
        void (*arg)(char* arg);
    } action;
} Tag;

/* Configuration
 *****************************************************************************/
enum {
    Width       = 640,  /* default window width */
    Height      = 480,  /* default window height */
    TabWidth    = 4,    /* maximum number of spaces used to represent a tab */
    ScrollLines = 4,    /* number of lines to scroll by for mouse wheel scrolling */
    BufSize     = 8192, /* default buffer size */
};

/* choose the font to  use for xft */
#ifdef __MACH__
#define FONTNAME "Monaco:size=10:antialias=true:autohint=true"
#else
#define FONTNAME "Liberation Mono:size=10:antialias=true:autohint=true"
#endif
#define DEFAULT_COLORSCHEME DARK
#define DEFAULT_CRLF 1
#define DEFAULT_CHARSET UTF_8
#define COLOR_PALETTE \
    {                 \
        0xff002b36,   \
        0xff073642,   \
        0xff586e75,   \
        0xff657b83,   \
        0xff839496,   \
        0xff93a1a1,   \
        0xffeee8d5,   \
        0xfffdf6e3,   \
        0xffb58900,   \
        0xffcb4b16,   \
        0xffdc322f,   \
        0xffd33682,   \
        0xff6c71c4,   \
        0xff268bd2,   \
        0xff2aa198,   \
        0xff859900    \
    }
#define DEFAULT_TAGS "Quit Save Undo Redo Cut Copy Paste | Find "
