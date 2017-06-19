/* extern the config variables */
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
    BufSize         = 8192,  /* default buffer size */
    FontCacheSize   = 16,    /* Maximum number of fonts allowed in the font cache */
    DefaultCRLF     = 0,     /* Default to Unix line endings */
    DefaultCharset  = UTF_8, /* We assume UTF-8 because nothing else matters */
    MaxScanDistance = 16384, /* Maximum distance to scan before visible lines for syntax higlighting */
};

#ifdef INCLUDE_DEFS

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

#undef INCLUDE_DEFS
#endif
