#include <stdc.h>
#include <utf.h>
#include <edit.h>
#include <unistd.h>

#ifdef __MACH__
char* CopyCmd[]  = { "pbcopy", NULL };
char* PasteCmd[] = { "pbpaste", NULL };
#else
char* CopyCmd[]  = { "xsel", "-bi", NULL };
char* PasteCmd[] = { "xsel", "-bo", NULL };
#endif

void clipcopy(char* text) {
    cmdwrite(CopyCmd, text);
}

char* clippaste(void) {
    return cmdread(PasteCmd);
}
