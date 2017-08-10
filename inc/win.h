enum {
    MouseLeft    = 1,
    MouseMiddle  = 2,
    MouseRight   = 3,
    MouseWheelUp = 4,
    MouseWheelDn = 5
};

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
    size_t x, y;
    size_t height, width;
    size_t csrx, csry;
    int clrnor, clrsel, clrcsr;
    bool warp_ptr;
    View view;
} Region;

typedef void (*MouseFunc)(WinRegion id, size_t count, size_t row, size_t col);

typedef struct {
    MouseFunc left;
    MouseFunc middle;
    MouseFunc right;
} MouseConfig;

void win_window(char* name, bool isdialog, void (*errfn)(char*));
void win_loop(void);
void win_settext(WinRegion id, char* text);
void win_setlinenums(bool enable);
bool win_getlinenums(void);
void win_setruler(size_t ruler);
Rune win_getkey(void);
void win_setkeys(KeyBinding* bindings, void (*inputfn)(Rune));
void win_setmouse(MouseConfig* mconfig);
void win_warpptr(WinRegion id);
View* win_view(WinRegion id);
Buf* win_buf(WinRegion id);
Sel* win_sel(WinRegion id);
bool win_btnpressed(int btn);
WinRegion win_getregion(void);
bool win_setregion(WinRegion id);
void win_setscroll(double offset, double visible);

/* These functions must be implemented by any appliation that wishes
   to use this module */
void onshutdown(void);
void onfocus(bool focused);
void onupdate(void);
void onlayout(void);
void onscroll(double percent);
void onmouseleft(WinRegion id, bool pressed, size_t row, size_t col);
void onmousemiddle(WinRegion id, bool pressed, size_t row, size_t col);
void onmouseright(WinRegion id, bool pressed, size_t row, size_t col);
