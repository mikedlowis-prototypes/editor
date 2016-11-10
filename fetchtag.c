#include <stdc.h>
#include <utf.h>
#include <edit.h>

typedef struct {
    char* file;
    char* cmd;
} Choice;

void add_choice(uint8_t* buf, size_t n) {
    char* str = malloc(n);
    memcpy(str, buf, n);
    str[n] = '\0';
    /**/
    char* tag  = strtok(str, "\t");
    char* file = strtok(NULL, "\t");
    char* cmd  = file + strlen(file) + 1;
    char* end  = strrchr(cmd, ';');
    if (end) *end = '\0';

    puts(tag);
    puts(file);
    puts(cmd);
}

void fetchtags(char* tagfile, char* tag) {
    size_t mlen = strlen(tag);
    FMap file = fmap(tagfile);
    for (size_t n, i = 0; i < file.len;) {
        if (file.buf[i] > *tag) {
            break;
        } if ((file.buf[i + mlen] == '\t') &&
              (0 == strncmp(((char*)file.buf+i), tag, mlen))) {
            for (n = 0; file.buf[i + n++] != '\n';);
            add_choice(file.buf + i, n-1);
            i += n;
        } else {
            while (file.buf[i++] != '\n');
        }
    }
}

int main(int argc, char** argv) {
    /* usage message */
    if (argc != 3) {
        printf("Usage: %s <tagfile> <tag>\n", argv[0]);
        exit(1);
    }
    /* scan the file */
    fetchtags(argv[1], argv[2]);
    return 0;
}
