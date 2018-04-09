enum {
    UTF_MAX   = 6u,       /* maximum number of bytes that make up a rune */
    RUNE_SELF = 0x80,     /* byte values larger than this are *not* ascii */
    RUNE_ERR  = 0xFFFD,   /* rune value representing an error */
    RUNE_MAX  = 0x10FFFF, /* Maximum decodable rune value */
    RUNE_EOF  = -1,       /* rune value representing end of file */
};

/* Represents a unicode code point */
typedef int32_t Rune;

size_t utf8encode(char str[UTF_MAX], Rune rune);
bool utf8decode(Rune* rune, size_t* length, int byte);
int runewidth(unsigned col, Rune r);
bool risword(Rune r);
bool rissigil(Rune r);
bool risfile(Rune r);
bool riscmd(Rune r);
bool risblank(Rune r);
bool risbigword(Rune r);

