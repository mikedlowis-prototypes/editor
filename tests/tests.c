#define INCLUDE_DEFS
#include "atf.h"

int main(int argc, char** argv) {
    atf_init(argc,argv);
    RUN_EXTERN_TEST_SUITE(BufferTests);
    return atf_print_results();
}
