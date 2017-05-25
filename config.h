/* extern the config variables */
extern char *FontString, *DefaultTags;
extern unsigned int ColorPalette[16];
extern char *ShellCmd[], *SedCmd[], *PickFileCmd[], *PickTagCmd[], *OpenCmd[];

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
    TabWidth       = 4,       /* maximum number of spaces used to represent a tab */
    ScrollLines    = 4,       /* number of lines to scroll by for mouse wheel scrolling */
    BufSize        = 8192,    /* default buffer size */
    FontCacheSize  = 16,      /* Maximum number of fonts allowed in the font cache */
    EventTimeout   = 50,      /* Maximum blocking wait time for events */
    TrimOnSave     = 1,       /* Enable trimming of trailing whitespace on save */
    DefaultCRLF    = 0,       /* Default to Unix line endings */
    DefaultCharset = UTF_8,   /* We assume UTF-8 because nothing else matters */
    LineSpacing    = LNSPACE, /* Set the vertical spacing between lines */
    DblClickTime   = 250,     /* Millisecond time for detecting double clicks */
    RulePosition   = 80,      /* Column in which the vertical ruler appears */
};

#ifdef INCLUDE_DEFS

/* Default contents of the tags region */
char* DefaultTags = "Quit Save Undo Redo Cut Copy Paste | Find ";

/* Default font string */
char* FontString = FONT;

/* 16 color palette used for drawing */
unsigned int ColorPalette[16] = {
    0xff002b36,
    0xff073642,
    0xff586e75,
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

/* The shell: Filled in with $SHELL. Used to execute commands */
char* ShellCmd[] = { NULL, "-c", NULL, NULL };

/* Sed command used to execute commands marked with ':' sigil */
char* SedCmd[] = { "sed", "-e", NULL, NULL };

/* Fuzzy Picker for files in the current directory and subdirectories */
char* PickFileCmd[] = { "xfilepick", ".", NULL };

/* Fuzzy picker for tags in a ctags database. */
char* PickTagCmd[] = { "xtagpick", NULL, "tags", NULL, NULL };

/* Open a new instance of the editor */
char* OpenCmd[] = { "xedit", NULL, NULL };

#undef INCLUDE_DEFS
#endif
