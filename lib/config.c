#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <stdc.h>
#include <utf.h>
#include <edit.h>

enum { OFF = 0, ON = 1 };

#ifdef __MACH__
    #define FONT "Monaco:size=10:antialias=true:autohint=true"
    #define LNSPACE 0
#else
    #define FONT "Liberation Mono:pixelsize=14:antialias=true:autohint=true"
    #define LNSPACE 1
#endif

char TagString[] = "Quit Save Undo Redo Cut Copy Paste | Find ";
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
    LineNums = ON,
    Syntax = ON,
    CopyIndent = ON,
    TrimOnSave = ON,
    ExpandTabs = ON;

int Palette[16] = {
    0xff002b36,
    0xff073642,
    0xff40565d,
    0xff657b83,
    0xff839496,
    0xff93a1a1,
    0xffeee8d5,
    0xfffdf6e3,
    0xffb58900,
    0xffcb4b16,
    0xffdc322f,
    0xffd33682,
    0xff6c71c4,
    0xff268bd2,
    0xff2aa198,
    0xff859900
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
    [SynNormal]    = { .bg = 0, .fg = 5  },
    [SynComment]   = { .bg = 0, .fg = 3  },
    [SynConstant]  = { .bg = 0, .fg = 14 },
    [SynNumber]    = { .bg = 0, .fg = 14 },
    [SynBoolean]   = { .bg = 0, .fg = 14 },
    [SynFloat]     = { .bg = 0, .fg = 14 },
    [SynString]    = { .bg = 0, .fg = 14 },
    [SynChar]      = { .bg = 0, .fg = 14 },
    [SynPreProc]   = { .bg = 0, .fg = 9  },
    [SynType]      = { .bg = 0, .fg = 8  },
    [SynKeyword]   = { .bg = 0, .fg = 15 },
    [SynStatement] = { .bg = 0, .fg = 10 },
    [SynFunction]  = { .bg = 0, .fg = 11 },
    [SynVariable]  = { .bg = 0, .fg = 12 },
    [SynSpecial]   = { .bg = 0, .fg = 13 },
    [SynOperator]  = { .bg = 0, .fg = 12 },
};
