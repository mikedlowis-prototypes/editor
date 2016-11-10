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

#ifdef __MACH__
#define CLOCK_MONOTONIC 0
// clock_gettime is not implemented on OSX (supposedly added on macOS)
int clock_gettime(int id, struct timespec* t) {
    (void)id;
    struct timeval now;
    int rv = gettimeofday(&now, NULL);
    if (rv) return rv;
    t->tv_sec  = now.tv_sec;
    t->tv_nsec = now.tv_usec * 1000;
    return 0;
}
#endif

uint32_t getmillis(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return ((time.tv_sec * 1000) + (time.tv_nsec / 1000000));
}

void die(const char* msgfmt, ...) {
    va_list args;
    va_start(args, msgfmt);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, msgfmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(EXIT_FAILURE);
}

FMap fmap(char* path) {
    int fd;
    FMap file = { .buf = NULL, .len = 0 };
    struct stat sb;
    if (((fd = open(path, O_RDONLY, 0)) < 0) ||
        (fstat(fd, &sb) < 0) ||
        (sb.st_size == 0))
        return file;
    file.buf = (uint8_t*)mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
    file.len = sb.st_size;
    if (file.buf == MAP_FAILED)
        die("memory mapping of file failed");
    return file;
}

void funmap(FMap file) {
    if (file.buf)
        munmap(file.buf, file.len);
}

bool risword(Rune r) {
    return (r < 127 && (isalnum(r) || r == '_'));
}

bool risblank(Rune r) {
    return (r == ' ' || r == '\t' || r == '\n' || r == '\r' || r == RUNE_CRLF);
}

char* stringdup(const char* s) {
    char* ns = (char*)malloc(strlen(s) + 1);
    strcpy(ns,s);
    return ns;
}

char* fdgets(int fd) {
    char buf[256];
    size_t len = 0, nread = 0;
    char* str = NULL;
    while ((nread = read(fd, buf, 256)) > 0) {
        str = realloc(str, len + nread + 1);
        memcpy(str+len, buf, nread);
        len += nread;
    }
    if (str) str[len] = '\0';
    return str;
}