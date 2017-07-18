/**
  @brief Collection of useful C types and functions.
  @author Michael D. Lowis
  @license BSD 2-clause License
*/

/* Standard Macros and Types */
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>

/* Useful Standard Functions */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

/* Type Definitions
 *****************************************************************************/
typedef unsigned short ushort;
typedef unsigned char uchar;
typedef unsigned long ulong;
typedef unsigned int uint;
typedef signed char schar;
typedef long long vlong;
typedef unsigned long long uvlong;

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uintptr_t uintptr;
typedef intptr_t  intptr;

/* Generic Death Function
 *****************************************************************************/
void die(const char* msgfmt, ...) __attribute__((__noreturn__));

/* Option Parsing
 *
 * This following macros implement a simple POSIX-style option parsing strategy.
 * They are heavily influenced and inspired by the arg.h file from suckless.org
 * (http://git.suckless.org/libsl/tree/arg.h). That file is in turn inspired by
 * the corresponding macros defined in plan9.
 *
 * The interface assumes that the main function will have the following
 * prototype:
 *
 * int main(int argc, char** argv);
 *
 * An example usage of the interface would look something like the follwoing:
 *
 * char* ARGV0;
 * int main(int argc, char** argv) {
 *     OPTBEGIN {
 *         case 'a': printf("Simple option\n"); break;
 *         case 'b': printf("Option with arg: %s\n", OPTARG()); break;
 *         default:  printf("Unknown option!\n");
 *     } OPTEND;
 *     return 0;
 * }
 */

/* This variable contains the value of argv[0] so that it can be referenced
 * again once the option parsing is done. This variable must be defined by the
 * program.
 *
 * NOTE: Ensure that you define this variable with external linkage (i.e. not
 * static) */
extern char* ARGV0;

/* This is a helper function used by the following macros to parse the next
 * option from the command line. */
static inline char* _getopt_(int* p_argc, char*** p_argv) {
    if (!(*p_argv)[0][1] && !(*p_argv)[1]) {
        return (char*)0;
    } else if ((*p_argv)[0][1]) {
        return &(*p_argv)[0][1];
    } else {
        *p_argv = *p_argv + 1;
        *p_argc = *p_argc - 1;
        return (*p_argv)[0];
    }
}

/* This macro is almost identical to the ARGBEGIN macro from suckless.org. If
 * it ain't broke, don't fix it. */
#define OPTBEGIN                                                              \
    for (                                                                     \
        ARGV0 = *argv, argc--, argv++;                                        \
        argv[0] && argv[0][1] && argv[0][0] == '-';                           \
        argc--, argv++                                                        \
    ) {                                                                       \
        int brk_; char argc_ , **argv_, *optarg_;                             \
        if (argv[0][1] == '-' && !argv[0][2]) {                               \
            argv++, argc--; break;                                            \
        }                                                                     \
        for (brk_=0, argv[0]++, argv_=argv; argv[0][0] && !brk_; argv[0]++) { \
            if (argv_ != argv) break;                                         \
            argc_ = argv[0][0];                                               \
            switch (argc_)

/* Terminate the option parsing. */
#define OPTEND }}

/* Get the current option character */
#define OPTC() (argc_)

/* Get an argument from the command line and return it as a string. If no
 * argument is available, this macro returns NULL */
#define OPTARG() \
    (optarg_ = _getopt_(&argc,&argv), brk_ = (optarg_!=0), optarg_)

/* Get an argument from the command line and return it as a string. If no
 * argument is available, this macro executes the provided code. If that code
 * returns, then abort is called. */
#define EOPTARG(code) \
    (optarg_ = _getopt_(&argc,&argv), \
    (!optarg_ ? ((code), abort(), (char*)0) : (brk_ = 1, optarg_)))

/* Helper macro to recognize number options */
#define OPTNUM \
    case '0':  \
    case '1':  \
    case '2':  \
    case '3':  \
    case '4':  \
    case '5':  \
    case '6':  \
    case '7':  \
    case '8':  \
    case '9'

/* Helper macro to recognize "long" options ala GNU style. */
#define OPTLONG \
    case '-'

/* Error Handling
 *****************************************************************************/
#define trace() \
    fprintf(stderr, "%s:%d\n", __FILE__, __LINE__)

#ifdef NDEBUG
    #define debug(msg, ...) \
        ((void)0)
#else
    #define debug(msg, ...) \
        fprintf(stderr, "DEBUG %s:%d: " msg "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#endif

#define errnostr() \
    (errno == 0 ? "None" : strerror(errno))

#define print_error(msg, ...) \
    fprintf(stderr, "[ERROR] (%s:%d: errno: %s) " msg "\n", __FILE__, __LINE__, errnostr(), ##__VA_ARGS__)

#define check(expr, msg, ...) \
    if(!(expr)) { print_error(msg, ##__VA_ARGS__); errno=0; goto error; }

#define sentinel(msg, ...) \
    { print_error(msg, ##__VA_ARGS__); errno=0; goto error; }

/* Miscellaneous
 *****************************************************************************/
#ifndef nelem
    #define nelem(x) \
        (sizeof(x)/sizeof((x)[0]))
#endif

#ifndef container_of
    #define container_of(obj, type, member) \
        (type*)((uintptr_t)obj - offsetof(type, member))
#endif

#define concat(a,b) \
    a##b

#define ident(a) \
    concat(id, a)

#define unique_id \
    ident(__LINE__)

#ifndef static_assert
    #define static_assert(expr) \
        typedef char unique_id[( expr )?1:-1]
#endif

#ifndef min
    #define min(x,y) \
        ((x) < (y) ? (x) : (y))
#endif

#ifndef max
    #define max(x,y) \
        ((x) > (y) ? (x) : (y))
#endif
