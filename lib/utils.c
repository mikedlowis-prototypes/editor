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

static size_t pagealign(size_t sz) {
    size_t pgsize  = sysconf(_SC_PAGE_SIZE);
    size_t alignmask = pgsize - 1;
    if (sz & alignmask)
        sz += pgsize - (sz & alignmask);
    return sz;
}

FMap mmap_readonly(char* path) {
    FMap file = { .buf = NULL, .len = 0 };
    int fd;
    struct stat sb;
    if (((fd = open(path, O_RDONLY, 0)) < 0) ||
        (fstat(fd, &sb) < 0) ||
        (sb.st_size == 0)) {
        return file;
    }
    file.buf = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    file.len = sb.st_size;
    if (file.buf == MAP_FAILED)
        die("memory mapping of file failed");
    close(fd);
    return file;
}

FMap mmap_readwrite(char* path, size_t sz) {
    FMap file = { .buf = NULL, .len = 0 };
    int fd = open(path, O_CREAT|O_RDWR, 0644);
    if (fd >= 0) {
        ftruncate(fd, sz);
        void* buf = mmap(NULL, pagealign(sz), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);        if (buf != MAP_FAILED) {
            file.buf = buf;
            file.len = sz;
        }
        close(fd);
    }
    return file;
}

void mmap_close(FMap file) {
    if (file.buf)
        munmap(file.buf, file.len);
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
    fprintf(stderr, "Error: ");
    vfprintf(stderr, msgfmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(EXIT_FAILURE);
}

char* stringdup(const char* s) {
    char* ns = (char*)malloc(strlen(s) + 1);
    assert(ns);
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

char* chomp(char* in) {
    char* pos = strrchr(in, '\n');
    if (pos) *pos = '\0';
    return in;
}

uint64_t modtime(char* path) {
    struct stat status;
    if (stat(path, &status) < 0)
        return 0u;
    return (uint64_t)status.st_mtime;
}
