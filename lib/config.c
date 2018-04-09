#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <stdc.h>
#include <utf.h>
#include <edit.h>

#ifdef __MACH__
    #define FONT "Monaco:size=10"
    #define LNSPACE 0
#else
    #define FONT "Liberation Mono:size=11"
    #define LNSPACE 1
#endif

char* TagString = "Del Put Undo Redo Find ";
char* FontString = FONT;

int /* Integer config options */
    WinWidth = 640,
    WinHeight = 480,
    ScrollWidth = 7,
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

#define PARCHMENT 1

#if defined(GRUVBOX)
int Palette[ClrCount] = {
    [Red]      = 0xfb4934, // Red
    [Green]    = 0xb8bb26, // Green
    [Yellow]   = 0xfabd2f, // Yellow
    [Blue]     = 0x83a598, // Blue
    [Purple]   = 0x93869b, // Purple
    [Aqua]     = 0x8ec07c, // Aqua
    [Orange]   = 0xfe8019, // Orange
    [EditBg]   = 0x1d2021, // Edit region background
    [EditFg]   = 0xd5c4a1, // Edit region text
    [EditSel]  = 0x504945, // Edit region selection
    [EditCsr]  = 0xfbf1c7, // Edit region cursor
    [TagsBg]   = 0x3c3836, // Tags region background
    [TagsFg]   = 0xd5c4a1, // Tags region text
    [TagsSel]  = 0x504945, // Tags region selection
    [TagsCsr]  = 0xfbf1c7, // Tags region cursor
    [ScrollBg] = 0x665c54, // Scroll region background
    [ScrollFg] = 0x1d2021, // Scroll region foreground
    [VerBdr]   = 0x665c54, // Vertical border
    [HorBdr]   = 0x665c54, // Horizontal border
};
#elif defined(PARCHMENT)
int Palette[ClrCount] = {
    [Red]      = 0x772222, // Red
    [Green]    = 0x227722, // Green
    [Blue]     = 0x222277, // Blue
    [Yellow]   = 0x000000, // Yellow ???
    [Purple]   = 0x000000, // Purple ???
    [Aqua]     = 0x000000, // Aqua   ???
    [Orange]   = 0x000000, // Orange ???
    [EditBg]   = 0xefefda, // Edit region background
    [EditSel]  = 0xdede8e, // Edit region selection
    [EditFg]   = 0x000000, // Edit region text
    [EditCsr]  = 0x000000, // Edit region cursor
    [TagsBg]   = 0xdaefef, // Tags region background
    [TagsSel]  = 0x8edede, // Tags region selection
    [TagsFg]   = 0x000000, // Tags region text
    [TagsCsr]  = 0x000000, // Tags region cursor
    [ScrollBg] = 0x89893c, // Scroll region background
    [ScrollFg] = 0xefefda, // Scroll region foreground
    [VerBdr]   = 0x89893c, // Vertical border
    [HorBdr]   = 0x000000, // Horizontal border
};
#endif
