#ifndef UTIL_H
#define UTIL_H

/* Standard Macros and Types */
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

/* Useful Standard Functions */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

/* Generic Death Function */
static void die(const char* msgfmt, ...)
{
    va_list args;
    va_start(args, msgfmt);
    #ifdef CLEANUP_HOOK
    CLEANUP_HOOK();
    #endif
    fprintf(stderr, "Error: ");
    vfprintf(stderr, msgfmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(EXIT_FAILURE);
}

/* Signal Handling */
static void esignal(int sig, void (*func)(int))
{
    errno = 0;
    func  = signal(sig, func);
    if (func == SIG_ERR || errno > 0)
        die("failed to register signal handler for signal %d", sig);
}

static int eraise(int sig)
{
    int ret;
    if ((ret = raise(sig)) != 0)
        die("failed to raise signal %d", sig);
    return ret;
}

/* Dynamic Allocation */
static void* ecalloc(size_t num, size_t size)
{
    void* ret;
    if (NULL == (ret = calloc(num,size)))
        die("out of memory");
    return ret;
}

static void* emalloc(size_t size)
{
    void* ret;
    if (NULL == (ret = malloc(size)))
        die("out of memory");
    return ret;
}

static void* erealloc(void* ptr, size_t size)
{
    void* ret;
    if (NULL == (ret = realloc(ptr,size)))
        die("out of memory");
    return ret;
}

/* File Handling */
static FILE* efopen(const char* filename, const char* mode)
{
    FILE* file;
    errno = 0;
    if (NULL == (file = fopen(filename, mode)) || errno != 0)
        die("failed to open file: %s", filename);
    return file;
}

static char* efreadline(FILE* input)
{
    size_t size  = 8;
    size_t index = 0;
    char*  str   = (char*)emalloc(size);
    memset(str, 0, 8);
    if (feof(input)) {
        free(str);
        return NULL;
    }
    while (true) {
        char ch = fgetc(input);
        if (ch == EOF) break;
        str[index++] = ch;
        str[index]   = '\0';
        if (index+1 >= size) {
            size = size << 1;
            str  = erealloc(str, size);
        }
        if (ch == '\n') break;
    }
    return str;
}

/* String Handling */
char* estrdup(const char *s)
{
    char* ns = (char*)emalloc(strlen(s) + 1);
    strcpy(ns,s);
    return ns;
}

#endif
