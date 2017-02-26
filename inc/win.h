typedef enum {
    STATUS   = 0,
    TAGS     = 1,
    EDIT     = 2,
    SCROLL   = 3,
    NREGIONS = 4,
    FOCUSED  = 4
} WinRegion;

typedef struct {
    int mods;
    Rune key;
    void (*action)(void);
} KeyBinding;

typedef struct {
    size_t x;
    size_t y;
    size_t height;
    size_t width;
    size_t csrx;
    size_t csry;
    bool warp_ptr;
    View view;
} Region;

typedef void (*MouseFunc)(WinRegion id, size_t count, size_t row, size_t col);

typedef struct {
    MouseFunc left;
    MouseFunc middle;
    MouseFunc right;
} MouseConfig;

void win_init(char* name);
void win_loop(void);
void win_settext(WinRegion id, char* text);
void win_setkeys(KeyBinding* bindings);
void win_setmouse(MouseConfig* mconfig);
void win_warpptr(WinRegion id);
View* win_view(WinRegion id);
Buf* win_buf(WinRegion id);
Sel* win_sel(WinRegion id);
bool win_btnpressed(MouseBtn btn);
WinRegion win_getregion(void);
void win_setregion(WinRegion id);

void onupdate(void);
void onmouseleft(WinRegion id, size_t count, size_t row, size_t col);
void onmousemiddle(WinRegion id, size_t count, size_t row, size_t col);
void onmouseright(WinRegion id, size_t count, size_t row, size_t col);

