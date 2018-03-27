#define _XOPEN_SOURCE 700
#include <stdc.h>
#include <utf.h>
#include <edit.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>

size_t pagealign(size_t sz) {
    size_t pgsize = sysconf(_SC_PAGE_SIZE), alignmask = pgsize - 1;
    if (sz & alignmask)
        sz += pgsize - (sz & alignmask);
    return sz;
}

uint64_t getmillis(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    uint64_t ms = ((uint64_t)time.tv_sec * (uint64_t)1000);
    ms += ((uint64_t)time.tv_nsec / (uint64_t)1000000);
    return ms;
}

void die(const char* msgfmt, ...) {
    va_list args;
    va_start(args, msgfmt);
    fprintf(stderr, "error: ");
    vfprintf(stderr, msgfmt, args);
    va_end(args);
    if (*msgfmt && msgfmt[strlen(msgfmt)-1] == ':')
        fprintf(stderr, " %s", strerror(errno));
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

char* stringdup(const char* s) {
    char* ns = (char*)malloc(strlen(s) + 1);
    assert(ns);
    strcpy(ns,s);
    return ns;
}

uint64_t modtime(char* path) {
    struct stat status;
    if (stat(path, &status) < 0)
        return 0u;
    return (uint64_t)status.st_mtime;
}

char* strmcat(char* first, ...) {
    va_list args;
    /* calculate the length of the final string */
    size_t len = strlen(first);
    va_start(args, first);
    for (char* s = NULL; (s = va_arg(args, char*));)
        len += strlen(s);
    va_end(args);
    /* allocate the final string and copy the args into it */
    char *str  = malloc(len+1), *curr = str;
    while (first && *first) *(curr++) = *(first++);
    va_start(args, first);
    for (char* s = NULL; (s = va_arg(args, char*));)
        while (s && *s) *(curr++) = *(s++);
    va_end(args);
    /* null terminate and return */
    *curr = '\0';
    return str;
}
