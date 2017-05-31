#define INCLUDE_DEFS
#include <atf.h>
#include <time.h>

// Inculd the source file so we can access everything
#include "../pick.c"

TEST_SUITE(UnitTests) {
}

int main(int argc, char** argv) {
    atf_init(argc,argv);
    RUN_TEST_SUITE(UnitTests);
    return atf_print_results();
}
