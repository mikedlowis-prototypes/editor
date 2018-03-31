#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <stdc.h>
#include <utf.h>
#include <edit.h>

#ifdef __MACH__
    #define FONT "Monaco:size=10"
    #define LNSPACE 0
#else
    #define FONT "Liberation Mono:pixelsize=14"
    #define LNSPACE 1
#endif

char* TagString = "Del Put Undo Redo Find ";
char* FontString = FONT;

int /* Integer config options */
    WinWidth = 640,
    WinHeight = 480,
    LineSpacing = LNSPACE,
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

int Palette[ClrCount] = {
    [Red]      = 0xFFfb4934, // Red
    [Green]    = 0xFFb8bb26, // Green
    [Yellow]   = 0xFFfabd2f, // Yellow
    [Blue]     = 0xFF83a598, // Blue
    [Purple]   = 0xFF93869b, // Purple
    [Aqua]     = 0xFF8ec07c, // Aqua
    [Orange]   = 0xFFfe8019, // Orange
    [EditBg]   = 0xFF1d2021, // Edit region background
    [EditFg]   = 0xFFd5c4a1, // Edit region text
    [EditSel]  = 0xFF504945, // Edit region selection
    [EditCsr]  = 0xFFfbf1c7, // Edit region cursor
    [TagsBg]   = 0xFF3c3836, // Tags region background
    [TagsFg]   = 0xFFd5c4a1, // Tags region text
    [TagsSel]  = 0xFF504945, // Tags region selection
    [TagsCsr]  = 0xFFfbf1c7, // Tags region cursor
    [ScrollBg] = 0xFF665c54, // Scroll region background
    [ScrollFg] = 0xFF1d2021, // Scroll region foreground
    [VerBdr]   = 0xFF665c54, // Vertical border
    [HorBdr]   = 0xFF665c54, // Horizontal border
};


