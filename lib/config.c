#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <stdc.h>
#include <utf.h>
#include <edit.h>

#ifdef __MACH__
    #define FONT "Verdana:size=10"
    #define LNSPACE 0
#else
    #define FONT "Verdana:size=11"
    #define LNSPACE 1
#endif

char* TagString = "Del Put Undo Redo Find ";
char* FontString = FONT;

int /* Integer config options */
    WinWidth = 640,
    WinHeight = 480,
    ScrollWidth = 5,
    LineSpacing = LNSPACE,
    Timeout = 50,
    TabWidth = 4,
    ScrollBy = 1,
    ClickTime = 500,
    MaxScanDist = 0,
    Syntax = ON,
    CopyIndent = ON,
    TrimOnSave = ON,
    ExpandTabs = ON,
    DosLineFeed = OFF;

int Palette[ClrCount] = {
    [Red]      = 0x772222, // Red
    [Green]    = 0x227722, // Green
    [Blue]     = 0x222277, // Blue
    [Yellow]   = 0x000000, // Yellow ???
    [Purple]   = 0x000000, // Purple ???
    [Aqua]     = 0x000000, // Aqua   ???
    [Orange]   = 0x000000, // Orange ???
    [EditBg]   = 0xF0F0E5, // Edit region background
    [EditSel]  = 0xDFDF99, // Edit region selection
    [EditFg]   = 0x000000, // Edit region text
    [EditCsr]  = 0x000000, // Edit region cursor
    [TagsBg]   = 0xE5F0F0, // Tags region background
    [TagsSel]  = 0x99DFDF, // Tags region selection
    [TagsFg]   = 0x000000, // Tags region text
    [TagsCsr]  = 0x000000, // Tags region cursor
    [ScrollBg] = 0x909047, // Scroll region background
    [ScrollFg] = 0xF0F0E5, // Scroll region foreground
    [VerBdr]   = 0x909047, // Vertical border
    [HorBdr]   = 0x000000, // Horizontal border
};
