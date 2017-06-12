/* extern the config variables */
extern char *FontString, *DefaultTags;
extern unsigned int ColorPalette[16];
extern char *ShellCmd[], *SedCmd[], *PickFileCmd[], *PickTagCmd[], *OpenCmd[];
extern int CLR_NormalText, CLR_GutterText, CLR_SelectedText, CLR_TagsBkg,
           CLR_EditBkg, CLR_HorBorder, CLR_VerBorder, CLR_Ruler, CLR_ScrollBkg,
           CLR_ThumbBkg, CLR_Cursor, CLR_CurrentLine, CLR_Comment;

/* OS-Specific Config
 ******************************************************************************/
#ifdef __MACH__
    #define FONT    "Monaco:size=10:antialias=true:autohint=true"
    #define LNSPACE 0
#else
    #define FONT    "Liberation Mono:pixelsize=14:antialias=true:autohint=true"
    #define LNSPACE 2
#endif

/* General Config
 ******************************************************************************/
enum {
    Width          = 640,     /* default window width */
    Height         = 480,     /* default window height */
    ExpandTabs     = 1,       /* Tabs will be expanded to spaces */
    TabWidth       = 4,       /* maximum number of spaces used to represent a tab */
    ScrollLines    = 4,       /* number of lines to scroll by for mouse wheel scrolling */
    BufSize        = 8192,    /* default buffer size */
    EventTimeout   = 50,      /* Maximum blocking wait time for events */
    TrimOnSave     = 1,       /* Enable trimming of trailing whitespace on save */
    DefaultCRLF    = 0,       /* Default to Unix line endings */
    DefaultCharset = UTF_8,   /* We assume UTF-8 because nothing else matters */
    FontCacheSize  = 16,      /* Maximum number of fonts allowed in the font cache */
    LineSpacing    = LNSPACE, /* Set the vertical spacing between lines */
    DblClickTime   = 250,     /* Millisecond time for detecting double clicks */
    RulePosition   = 80,      /* Column in which the vertical ruler appears */
    CopyIndent     = 1,       /* New lines will inherit the indent of the preceding line */
    LineNumbers    = 1,       /* Enable line numbers by default or not */
};

#ifdef INCLUDE_DEFS

/* Default contents of the tags region */
char* DefaultTags = "Quit Save Undo Redo Cut Copy Paste | Find ";

/* Default font string */
char* FontString = FONT;

/* The shell: Filled in with $SHELL. Used to execute commands */
char* ShellCmd[] = { NULL, "-c", NULL, NULL };

/* Sed command used to execute commands marked with ':' sigil */
char* SedCmd[] = { "sed", "-e", NULL, NULL };

/* Fuzzy Picker for files in the current directory and subdirectories */
char* PickFileCmd[] = { "pickfile", ".", NULL };

/* Fuzzy picker for tags in a ctags database. */
char* PickTagCmd[] = { "picktag", NULL, "tags", NULL, NULL };

/* Open a new instance of the editor */
char* OpenCmd[] = { "tide", NULL, NULL };

/* Solarized Theme - 16 color palette used for drawing */
unsigned int ColorPalette[16] = {
    0xff002b36, // 00 - Base03
    0xff073642, // 01 - Base02
    0xff586e75, // 02 - Base01
    0xff657b83, // 03 - Base00
    0xff839496, // 04 - Base0
    0xff93a1a1, // 05 - Base1
    0xffeee8d5, // 06 - Base2
    0xfffdf6e3, // 07 - Base3
    0xffb58900, // 08 - Yellow
    0xffcb4b16, // 09 - Orange
    0xffdc322f, // 10 - Red
    0xffd33682, // 11 - Magenta
    0xff6c71c4, // 12 - Violet
    0xff268bd2, // 13 - Blue
    0xff2aa198, // 14 - Cyan
    0xff859900  // 15 - Green
};

#define COLOR_PAIR(bg, fg) (((bg) << 8) | (fg))
int CLR_NormalText   = COLOR_PAIR(0,4);
int CLR_GutterText   = COLOR_PAIR(0,3);
int CLR_SelectedText = COLOR_PAIR(4,0);
int CLR_TagsBkg      = 1;  // Background color for the tags region
int CLR_EditBkg      = 0;  // Background color for the edit region
int CLR_ScrollBkg    = 3;  // Background color for the scroll region
int CLR_ThumbBkg     = 0;  // Background color of the scroll thumb
int CLR_HorBorder    = 2;  // Horizontal border color
int CLR_VerBorder    = 2;  // Vertical border color
int CLR_Ruler        = 1;  // Ruler color
int CLR_Cursor       = 7;  // Cursor color

int CLR_CurrentLine  = 13; // Current Line Number
int CLR_Comment      = 2;  // Comment color

#undef INCLUDE_DEFS
#endif
