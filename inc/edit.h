/* Utility Functions
 *****************************************************************************/
typedef struct {
    uint8_t* buf; /* memory mapped byte buffer */
    size_t len;   /* length of the buffer */
} FMap;

FMap fmap(char* path);
void funmap(FMap file);
uint32_t getmillis(void);
bool risword(Rune r);
bool risblank(Rune r);
char* stringdup(const char* str);
char* fdgets(int fd);

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
    bool modified;    /* tracks whether the buffer has been modified */
    size_t bufsize;   /* size of the buffer in runes */
    Rune* bufstart;   /* start of the data buffer */
    Rune* bufend;     /* end of the data buffer */
    Rune* gapstart;   /* start of the gap */
    Rune* gapend;     /* end of the gap */
    Log* undo;        /* undo list */
    Log* redo;        /* redo list */
    bool expand_tabs; /* tracks current mode */
} Buf;

typedef struct {
    size_t beg;
    size_t end;
    size_t col;
} Sel;

void buf_load(Buf* buf, char* path);
void buf_save(Buf* buf);
void buf_init(Buf* buf);
void buf_ins(Buf* buf, unsigned pos, Rune);
void buf_del(Buf* buf, unsigned pos);
unsigned buf_undo(Buf* buf, unsigned pos);
unsigned buf_redo(Buf* buf, unsigned pos);
Rune buf_get(Buf* buf, unsigned pos);
bool buf_iseol(Buf* buf, unsigned pos);
unsigned buf_bol(Buf* buf, unsigned pos);
unsigned buf_eol(Buf* buf, unsigned pos);
unsigned buf_bow(Buf* buf, unsigned pos);
unsigned buf_eow(Buf* buf, unsigned pos);
unsigned buf_lscan(Buf* buf, unsigned pos, Rune r);
unsigned buf_rscan(Buf* buf, unsigned pos, Rune r);
void buf_find(Buf* buf, unsigned* beg, unsigned* end);
unsigned buf_end(Buf* buf);
unsigned buf_byrune(Buf* buf, unsigned pos, int count);
unsigned buf_byline(Buf* buf, unsigned pos, int count);
unsigned buf_getcol(Buf* buf, unsigned pos);
unsigned buf_setcol(Buf* buf, unsigned pos, unsigned col);
char* buf_getstr(Buf* buf, unsigned beg, unsigned end);
unsigned buf_putstr(Buf* buf, unsigned beg, unsigned end, char* str);

/*
void buf_load(Buf* buf, char* path);
void buf_save(Buf* buf);
void buf_init(Buf* buf);
void buf_ins(Buf* buf, Sel csr, Rune);
void buf_del(Buf* buf, Sel csr);
Sel buf_undo(Buf* buf, Sel csr);
Sel buf_redo(Buf* buf, Sel csr);
Rune buf_get(Buf* buf, Sel csr);
void buf_setlocked(Buf* buf, bool locked);
bool buf_locked(Buf* buf);
bool buf_iseol(Buf* buf, Sel csr);
Sel buf_bol(Buf* buf, Sel csr);
Sel buf_eol(Buf* buf, Sel csr);
Sel buf_bow(Buf* buf, Sel csr);
Sel buf_eow(Buf* buf, Sel csr);
Sel buf_lscan(Buf* buf, Sel csr, Rune r);
Sel buf_rscan(Buf* buf, Sel csr, Rune r);
Sel buf_find(Buf* buf, Sel csr);
Sel buf_end(Buf* buf);
Sel buf_byrune(Buf* buf, Sel csr, int count);
Sel buf_byline(Buf* buf, Sel csr, int count);
Sel buf_getcol(Buf* buf, Sel csr);
Sel buf_setcol(Buf* buf, Sel csr, unsigned col);
char* buf_getstr(Buf* buf, Sel csr);
Sel buf_putstr(Buf* buf, Sel csr, char* str);
*/

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
    //size_t posx;
    //size_t posy;
    //size_t height;
    //size_t width;
    size_t nrows;  /* number of rows in the view */
    size_t ncols;  /* number of columns in the view */
    Row** rows;    /* array of row data structures */
    Buf buffer;    /* the buffer used to populate the view */
    Sel selection; /* range of currently selected text */
} View;

void view_init(View* view, char* file);
void view_resize(View* view, size_t nrows, size_t ncols);
//void view_resize(View* view, size_t x, size_t y, size_t height, size_t width, size_t fheight, size_t fwidth);
void view_update(View* view, size_t* csrx, size_t* csry);
Row* view_getrow(View* view, size_t row);
void view_byrune(View* view, int move);
void view_byline(View* view, int move);
void view_setcursor(View* view, size_t row, size_t col);
void view_selext(View* view, size_t row, size_t col);
void view_insert(View* view, Rune rune);

//size_t view_getoff(View* view, size_t pos, size_t row, size_t col);
//void view_getsize(View* view, size_t* nrows, size_t* ncols);
//void view_clearrow(View* view, size_t row);
//size_t view_setcell(View* view, size_t row, size_t col, uint32_t attr, Rune r);
//UGlyph* view_getglyph(View* view, size_t row, size_t col, size_t* scrwidth);

void screen_update(Buf* buf, unsigned crsr, unsigned* csrx, unsigned* csry);
unsigned screen_getoff(Buf* buf, unsigned pos, unsigned row, unsigned col);
void screen_setsize(Buf* buf, unsigned nrows, unsigned ncols);
void screen_getsize(unsigned* nrows, unsigned* ncols);
Row* screen_getrow(unsigned row);
void screen_clearrow(unsigned row);
unsigned screen_setcell(unsigned row, unsigned col, uint32_t attr, Rune r);
UGlyph* screen_getglyph(unsigned row, unsigned col, unsigned* scrwidth);

/* Command Executions
 *****************************************************************************/
typedef struct {
    int pid; /* process id of the child process */
    int in;  /* file descriptor for the child process's standard input */
    int out; /* file descriptor for the child process's standard output */
    int err; /* file descriptor for the child process's standard error */
} Process;

int execute(char** cmd, Process* proc);
void detach(Process* proc);
void terminate(Process* proc, int sig);
char* cmdread(char** cmd);
void cmdwrite(char** cmd, char* text);

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
    Rune key;
    void (*action)(void);
} KeyBinding;

typedef struct {
    uint32_t time;
    uint8_t count;
    bool pressed;
} ButtonState;

typedef struct {
    size_t x;
    size_t y;
    size_t height;
    size_t width;
    View view;
} Region;

/* Configuration
 *****************************************************************************/
enum {
    Width       = 640,  /* default window width */
    Height      = 480,  /* default window height */
    TabWidth    = 4,    /* maximum number of spaces used to represent a tab */
    ScrollLines = 1,    /* number of lines to scroll by for mouse wheel scrolling */
    BufSize     = 8192, /* default buffer size */
};

/* choose the font to  use for xft */
#ifdef __MACH__
#define FONTNAME "Monaco:size=10:antialias=true:autohint=true"
#else
#define FONTNAME "Liberation Mono:size=10.5:antialias=true:autohint=true"
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
#define DEFAULT_TAGS "Quit Save Cut Copy Paste | Find \n"
