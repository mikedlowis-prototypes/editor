#define UTF_MAX   6u           /* maximum number of bytes that make up a rune */
#define RUNE_SELF 0x80         /* byte values larger than this are *not* ascii */
#define RUNE_ERR  0xFFFD       /* rune value representing an error */
#define RUNE_MAX  0x10FFFF     /* Maximum decodable rune value */
#define RUNE_EOF  UINT32_MAX   /* rune value representing end of file */
#define RUNE_CRLF UINT32_MAX-1 /* rune value representing a \r\n sequence */

/* Represents a unicode code point */
typedef uint32_t Rune;

size_t utf8encode(char str[UTF_MAX], Rune rune);
bool utf8decode(Rune* rune, size_t* length, int byte);
Rune fgetrune(FILE* f);
void fputrune(Rune rune, FILE* f);
int runewidth(unsigned col, Rune r);
