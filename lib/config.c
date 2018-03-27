#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <stdc.h>
#include <utf.h>
#include <edit.h>

enum { OFF = 0, ON = 1 };

#ifdef __MACH__
    #define FONT "Monaco:size=10"
    #define LNSPACE 0
#else
    #define FONT "Liberation Mono:pixelsize=14"
    #define LNSPACE 1
#endif

char TagString[] = "Del Put Undo Redo Find ";
char FontString[] = FONT;

int /* Integer config options */
    WinWidth = 640,
    WinHeight = 480,
    LineSpacing = LNSPACE,
    RulerPos = 80,
    Timeout = 50,
    TabWidth = 4,
    ScrollBy = 4,
    ClickTime = 500,
    MaxScanDist = 0,
    Syntax = ON,
    CopyIndent = ON,
    TrimOnSave = ON,
    ExpandTabs = ON,
    DosLineFeed = OFF;

int Palette[16] = {
    0xFF1d2021,
    0xFF3c3836,
    0xFF504945,
    0xFF665c54,
    0xFFbdae93,
    0xFFd5c4a1,
    0xFFebdbb2,
    0xFFfbf1c7,
    0xFFfb4934,
    0xFFfe8019,
    0xFFfabd2f,
    0xFFb8bb26,
    0xFF8ec07c,
    0xFF83a598,
    0xFFd3869b,
    0xFFd65d0e
};

CPair Colors[28] = {
    /* UI Colors */
    [ClrScrollNor] = { .bg = 3, .fg = 0 },
    [ClrGutterNor] = { .bg = 1, .fg = 4 },
    [ClrGutterSel] = { .bg = 2, .fg = 7 },
    [ClrStatusNor] = { .bg = 0, .fg = 5 },
    [ClrTagsNor]   = { .bg = 1, .fg = 5 },
    [ClrTagsSel]   = { .bg = 2, .fg = 5 },
    [ClrTagsCsr]   = { .bg = 0, .fg = 7 },
    [ClrEditNor]   = { .bg = 0, .fg = 5 },
    [ClrEditSel]   = { .bg = 2, .fg = 5 },
    [ClrEditCsr]   = { .bg = 0, .fg = 7 },
    [ClrEditRul]   = { .bg = 0, .fg = 1 },
    [ClrBorders]   = { .bg = 3, .fg = 3 },
    /* Syntax Colors */
    [SynNormal]    = { .fg = 5  },
    [SynComment]   = { .fg = 3  },
    [SynConstant]  = { .fg = 14 },
    [SynNumber]    = { .fg = 14 },
    [SynBoolean]   = { .fg = 14 },
    [SynFloat]     = { .fg = 14 },
    [SynString]    = { .fg = 10 },
    [SynChar]      = { .fg = 10 },
    [SynPreProc]   = { .fg = 8  },
    [SynType]      = { .fg = 13 },
    [SynKeyword]   = { .fg = 8  },
    [SynStatement] = { .fg = 15 },
    [SynFunction]  = { .fg = 13 },
    [SynVariable]  = { .fg = 8  },
    [SynSpecial]   = { .fg = 15 },
    [SynOperator]  = { .fg = 12 },
};
