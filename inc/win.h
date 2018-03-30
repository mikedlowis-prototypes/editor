enum {
    MouseLeft    = 1,
    MouseMiddle  = 2,
    MouseRight   = 3,
    MouseWheelUp = 4,
    MouseWheelDn = 5
};

typedef enum {
    TAGS     = 0,
    EDIT     = 1,
    SCROLL   = 2,
    NREGIONS = 3,
    FOCUSED  = 3
} WinRegion;

typedef struct {
    int mods;
    Rune key;
    void (*action)(char*);
} KeyBinding;

typedef struct {
    size_t x, y;
    size_t height, width;
    size_t csrx, csry;
    CPair clrnor, clrsel, clrcsr;
    View view;
} Region;

void win_init(KeyBinding* bindings);
void win_save(char* path);
void win_loop(void);
void win_quit(void);
View* win_view(WinRegion id);
Buf* win_buf(WinRegion id);
bool win_btnpressed(int btn);
WinRegion win_getregion(void);
bool win_setregion(WinRegion id);
void win_setscroll(double offset, double visible);

/* move these to x11.c when possible */
extern int SearchDir;
extern char* SearchTerm;
void exec(char* cmd);
void cut(char* arg);
void paste(char* arg);
