#include <stdc.h>
#include <utf.h>
#include <edit.h>
#include <unistd.h>
#include <ctype.h>

Buf GapBuffer;
char RawBuffer[32768];
char* CurrBeg = RawBuffer;
char* CurrEnd = RawBuffer;
size_t Pos;
size_t End;

int fetch_byte(void) {
    if (CurrBeg >= CurrEnd) {
        CurrBeg = CurrEnd = RawBuffer;
        long nread = read(STDIN_FILENO, RawBuffer, sizeof(RawBuffer));
        if (nread <= 0) exit( -nread );
        CurrEnd += nread;
    }
    return *(CurrBeg++);
}

void recv_chunk(void) {
    int chunksz = 0, bufpos = 0, byte;
    while (!bufpos) {
        /* read the size of the chunk first */
        while (isdigit(byte = fetch_byte()))
            chunksz = (chunksz * 10) + (byte - '0');
        if (!chunksz) continue;
        /* now read out the actual chunk and stuff it in the gap buffer */
        for (; chunksz; chunksz--, bufpos++) {
            size_t len = 0;
            Rune r;
            while (!utf8decode(&r, &len, fetch_byte()));
            buf_insert(&GapBuffer, false, bufpos, r);
        }
    }
    Pos = 0;
    End = buf_end(&GapBuffer);
}

extern void init(void);
extern void scan(void);

int main(int argc, char** argv) {
    init();
    while (true) {
        buf_init(&GapBuffer, NULL);
        recv_chunk();
        scan();
        printf("0,0,0\n");
        fflush(stdout);
    }
    return 0;
}

/******************************************************************************/

#include <ctype.h>

static int isalnum_(int c) { return (isalnum(c) || c == '_'); }
static int isalpha_(int c) { return (isalpha(c) || c == '_'); }

static int peekc(int i) {
    if (Pos >= End) return '\n';
    Rune r = buf_get(&GapBuffer, Pos+i);
    return (r == RUNE_CRLF ? '\n' : (int)r);
}

static int takec(void) {
    int c = peekc(0);
    Pos++;
    return c;
}

/******************************************************************************/

void init(void) {

}

void scan(void) {
    while (Pos < End) {
        if (isalpha_(peekc(0))) {
            size_t beg = Pos, end = Pos;
            while (isalnum_(takec()))
                end++;
            printf("%d,%d,%d\n", (int)beg, (int)end, 0);
        } else {
            Pos++;
        }
    }
}

