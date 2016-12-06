#include <stdc.h>
#include <utf.h>
#include <edit.h>
#define INCLUDE_DEFS
#include <atf.h>

Buf Buffer;
unsigned CursorPos;
unsigned TargetCol;
unsigned SelBeg;
unsigned SelEnd;

void move_pointer(unsigned x, unsigned y) {
    (void)x;
    (void)y;
}

int main(int argc, char** argv) {
    atf_init(argc,argv);
    RUN_EXTERN_TEST_SUITE(XeditTests);
    RUN_EXTERN_TEST_SUITE(BufferTests);
    RUN_EXTERN_TEST_SUITE(Utf8Tests);
    return atf_print_results();
}
