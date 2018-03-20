#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <stdc.h>
#include <utf.h>
#include <edit.h>

#if 0
enum { OFF = 0, ON = 1 };

#ifdef __MACH__
    #define FONT "Monaco:size=10:antialias=true:autohint=true"
    #define LNSPACE 0
#else
    #define FONT "Liberation Mono:pixelsize=14:antialias=true:autohint=true"
    #define LNSPACE 1
#endif

char Tags[] = "Quit Save Undo Redo Cut Copy Paste | Find ";
char Font[] = FONT;

int /* Integer config options */
    WinWidth = 640,
    WinHeight = 480,
    LineSpacing = LNSPACE,
    Ruler = 80,
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

struct { int fg, bg; } Colors[] = {
    /* UI Colors */
    [ClrScrollNor] = { .bg = 0, .fg = 0 },
    [ClrGutterNor] = { .bg = 0, .fg = 0 },
    [ClrGutterSel] = { .bg = 0, .fg = 0 },
    [ClrStatusNor] = { .bg = 0, .fg = 0 },
    [ClrTagsNor]   = { .bg = 0, .fg = 0 },
    [ClrTagsSel]   = { .bg = 0, .fg = 0 },
    [ClrTagsCsr]   = { .bg = 0, .fg = 0 },
    [ClrEditNor]   = { .bg = 0, .fg = 0 },
    [ClrEditSel]   = { .bg = 0, .fg = 0 },
    [ClrEditCsr]   = { .bg = 0, .fg = 0 },
    [ClrEditRul]   = { .bg = 0, .fg = 0 },
    [ClrBorders]   = { .bg = 0, .fg = 0 },
    /* Syntax Colors */
    [SynNormal]    = { .bg = 0, .fg = 0 },
    [SynComment]   = { .bg = 0, .fg = 0 },
    [SynConstant]  = { .bg = 0, .fg = 0 },
    [SynNumber]    = { .bg = 0, .fg = 0 },
    [SynBoolean]   = { .bg = 0, .fg = 0 },
    [SynFloat]     = { .bg = 0, .fg = 0 },
    [SynString]    = { .bg = 0, .fg = 0 },
    [SynChar]      = { .bg = 0, .fg = 0 },
    [SynPreProc]   = { .bg = 0, .fg = 0 },
    [SynType]      = { .bg = 0, .fg = 0 },
    [SynKeyword]   = { .bg = 0, .fg = 0 },
    [SynStatement] = { .bg = 0, .fg = 0 },
    [SynFunction]  = { .bg = 0, .fg = 0 },
    [SynVariable]  = { .bg = 0, .fg = 0 },
    [SynSpecial]   = { .bg = 0, .fg = 0 },
    [SynOperator]  = { .bg = 0, .fg = 0 },
};

#endif

XrmDatabase DB;
struct {
    char* name;
    enum { STRING, INTEGER, BOOLEAN } type;
    union {
        bool opt;
        int num;
        char* str;
    } value;
} Options[] = {
    [EditTagString] = { "tide.ui.tags.edit", STRING, {
        .str = "Quit Save Undo Redo Cut Copy Paste | Find " } },
    [CmdTagString] = { "tide.ui.tags.cmd", STRING, {
        .str = "Quit Undo Redo Cut Copy Paste | Send Find " } },

#ifdef __MACH__
    [FontString] = { "tide.ui.font", STRING, {
        .str = "Monaco:size=10:antialias=true:autohint=true" } },
    [LineSpacing]  = { "tide.ui.line_spacing", INTEGER, { .num = 0    } },
#else
    [FontString] = { "tide.ui.font", STRING, {
        .str = "Liberation Mono:pixelsize=14:antialias=true:autohint=true" } },
    [LineSpacing]  = { "tide.ui.line_spacing", INTEGER, { .num = 1    } },
#endif

    /* user interface related options */
    [WinWidth]      = { "tide.ui.width",          INTEGER, { .num = 640  } },
    [WinHeight]     = { "tide.ui.height",         INTEGER, { .num = 480  } },
    [LineNumbers]   = { "tide.ui.line_numbers",   BOOLEAN, { .opt = true } },
    [SyntaxEnabled] = { "tide.ui.syntax_enabled", BOOLEAN, { .opt = true } },
    [RulerColumn]   = { "tide.ui.ruler_column",   INTEGER, { .num = 80   } },
    [EventTimeout]  = { "tide.ui.timeout",        INTEGER, { .num = 50   } },

    /* input related options */
    [CopyIndent]   = { "tide.input.copy_indent",   BOOLEAN, { .opt = true  } },
    [TrimOnSave]   = { "tide.input.trim_on_save",  BOOLEAN, { .opt = true  } },
    [ExpandTabs]   = { "tide.input.expand_tabs",   BOOLEAN, { .opt = true  } },
    [TabWidth]     = { "tide.input.tab_width",     INTEGER, { .num = 4     } },
    [ScrollLines]  = { "tide.input.scroll_lines",  INTEGER, { .num = 4     } },
    [DblClickTime] = { "tide.input.click_time",    INTEGER, { .num = 500   } },
    [MaxScanDist]  = { "tide.input.max_scan_dist", INTEGER, { .num = 0     } },

    /* default color palette definition */
    [Color00] = { "tide.palette.00", INTEGER, { .num = 0xff002b36 } },
    [Color01] = { "tide.palette.01", INTEGER, { .num = 0xff073642 } },
    [Color02] = { "tide.palette.02", INTEGER, { .num = 0xff40565d } },
    [Color03] = { "tide.palette.03", INTEGER, { .num = 0xff657b83 } },
    [Color04] = { "tide.palette.04", INTEGER, { .num = 0xff839496 } },
    [Color05] = { "tide.palette.05", INTEGER, { .num = 0xff93a1a1 } },
    [Color06] = { "tide.palette.06", INTEGER, { .num = 0xffeee8d5 } },
    [Color07] = { "tide.palette.07", INTEGER, { .num = 0xfffdf6e3 } },
    [Color08] = { "tide.palette.08", INTEGER, { .num = 0xffb58900 } }, // Yellow
    [Color09] = { "tide.palette.09", INTEGER, { .num = 0xffcb4b16 } }, // Orange
    [Color10] = { "tide.palette.10", INTEGER, { .num = 0xffdc322f } }, // Red
    [Color11] = { "tide.palette.11", INTEGER, { .num = 0xffd33682 } }, // Magenta
    [Color12] = { "tide.palette.12", INTEGER, { .num = 0xff6c71c4 } }, // Violet
    [Color13] = { "tide.palette.13", INTEGER, { .num = 0xff268bd2 } }, // Blue
    [Color14] = { "tide.palette.14", INTEGER, { .num = 0xff2aa198 } }, // Cyan
    [Color15] = { "tide.palette.15", INTEGER, { .num = 0xff859900 } }, // Green

    /* UI Colors */
    [ClrScrollNor] = { "tide.colors.scroll.normal",   INTEGER, { .num = 0x0300 } },
    [ClrGutterNor] = { "tide.colors.gutter.normal",   INTEGER, { .num = 0x0104 } },
    [ClrGutterSel] = { "tide.colors.gutter.selected", INTEGER, { .num = 0x0207 } },
    [ClrStatusNor] = { "tide.colors.status.normal",   INTEGER, { .num = 0x0005 } },
    [ClrTagsNor]   = { "tide.colors.tags.normal",     INTEGER, { .num = 0x0105 } },
    [ClrTagsSel]   = { "tide.colors.tags.selected",   INTEGER, { .num = 0x0205 } },
    [ClrTagsCsr]   = { "tide.colors.tags.cursor",     INTEGER, { .num = 0x07   } },
    [ClrEditNor]   = { "tide.colors.edit.normal",     INTEGER, { .num = 0x0005 } },
    [ClrEditSel]   = { "tide.colors.edit.selected",   INTEGER, { .num = 0x0205 } },
    [ClrEditCsr]   = { "tide.colors.edit.cursor",     INTEGER, { .num = 0x07   } },
    [ClrEditRul]   = { "tide.colors.edit.ruler",      INTEGER, { .num = 0x01   } },
    [ClrBorders]   = { "tide.colors.borders",         INTEGER, { .num = 0x0303 } },

    /* Syntax Colors */
    [SynNormal]    = { "tide.colors.syntax.normal",       INTEGER, { .num = 0x0005 } },
    [SynComment]   = { "tide.colors.syntax.comment",      INTEGER, { .num = 0x0003 } },
    [SynConstant]  = { "tide.colors.syntax.constant",     INTEGER, { .num = 0x000E } },
    [SynNumber]    = { "tide.colors.syntax.number",       INTEGER, { .num = 0x000E } },
    [SynBoolean]   = { "tide.colors.syntax.boolean",      INTEGER, { .num = 0x000E } },
    [SynFloat]     = { "tide.colors.syntax.float",        INTEGER, { .num = 0x000E } },
    [SynString]    = { "tide.colors.syntax.string",       INTEGER, { .num = 0x000E } },
    [SynChar]      = { "tide.colors.syntax.character",    INTEGER, { .num = 0x000E } },
    [SynPreProc]   = { "tide.colors.syntax.preprocessor", INTEGER, { .num = 0x0009 } },
    [SynType]      = { "tide.colors.syntax.type",         INTEGER, { .num = 0x0008 } },
    [SynKeyword]   = { "tide.colors.syntax.keyword",      INTEGER, { .num = 0x000F } },
    [SynStatement] = { "tide.colors.syntax.statement",    INTEGER, { .num = 0x000A } },
    [SynFunction]  = { "tide.colors.syntax.function",     INTEGER, { .num = 0x000B } },
    [SynVariable]  = { "tide.colors.syntax.variable",     INTEGER, { .num = 0x000C } },
    [SynSpecial]   = { "tide.colors.syntax.special",      INTEGER, { .num = 0x000D } },
    [SynOperator]  = { "tide.colors.syntax.operator",     INTEGER, { .num = 0x000C } },
};

void config_init(void* disp) {
    XrmDatabase db;
    char *homedir  = getenv("HOME"),
         *userfile = strmcat(homedir, "/.config/tiderc", 0),
         *rootfile = strmcat(homedir, "/.Xdefaults", 0);
    XrmInitialize();

    /* load from xrdb or .Xdefaults */
    if (XResourceManagerString(disp) != NULL)
        db = XrmGetStringDatabase(XResourceManagerString(disp));
    else
        db = XrmGetFileDatabase(rootfile);
    XrmMergeDatabases(db, &DB);

    /* load user settings from ~/.config/tiderc */
    db = XrmGetFileDatabase(userfile);
    (void)XrmMergeDatabases(db, &DB);

    /* cleanup */
    free(userfile);
    free(rootfile);

    char* type;
    XrmValue val;
    for (size_t i = 0; i < nelem(Options); i++) {
        if (XrmGetResource(DB, Options[i].name, NULL, &type, &val)) {
            char* value = (char*)(val.addr);
            size_t len = val.size;
            switch (Options[i].type) {
                case STRING:
                    if (*value == '\'') value++, len -= 2;
                    Options[i].value.str = strndup(value, len);
                    break;

                case INTEGER:
                    Options[i].value.num = strtol(value, NULL, 0);
                    break;

                case BOOLEAN:
                    Options[i].value.opt = !strncmp(value, "true", len);
                    break;
            }
        }
    }
}

void config_set_int(int key, int val) {
    assert(Options[key].type == INTEGER);
    Options[key].value.num = val;
}

void config_set_bool(int key, bool val) {
    assert(Options[key].type == BOOLEAN);
    Options[key].value.opt = val;
}

void config_set_str(int key, char* val) {
    assert(Options[key].type == STRING);
    Options[key].value.str = val;
}

int config_get_int(int key) {
    assert(Options[key].type == INTEGER);
    return Options[key].value.num;
}

bool config_get_bool(int key) {
    assert(Options[key].type == BOOLEAN);
    return Options[key].value.num;
}

char* config_get_str(int key) {
    assert(Options[key].type == STRING);
    return Options[key].value.str;
}
