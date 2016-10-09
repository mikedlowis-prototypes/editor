#define INCLUDE_DEFS
#include "atf.h"
#include "edit.h"

Buf Buffer;
unsigned CursorPos;
unsigned TargetCol;
enum ColorScheme ColorBase;

void die(char* m) {
    (void)m;
}

int main(int argc, char** argv) {
    atf_init(argc,argv);
    RUN_EXTERN_TEST_SUITE(BufferTests);
    RUN_EXTERN_TEST_SUITE(Utf8Tests);
    return atf_print_results();
}
