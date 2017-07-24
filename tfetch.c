#define _XOPEN_SOURCE 700
#include <stdc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <regex.h>
#include <ctype.h>

#ifdef __MACH__
    #define OPENCMD "open"
#else
    #define OPENCMD "xdg-open"
#endif

typedef struct {
    enum {
        COMPLETE=0, MATCHES, IS, ISSET, ISDIR, ISFILE,
        SET, UNSET, FINDFILE, EXEC, LAUNCH, CHECK
    } type;
    char* arg1;
    char* arg2;
} Rule;

char* Matches[10];

Rule* BuiltinRules[] = {
    (Rule[]){ // Match URLS and open them with the browser
        { ISSET, "BROWSER", NULL },
        { MATCHES, "data", "^(https?|ftp)://.*" },
        { LAUNCH, "$BROWSER $0", NULL },
        { COMPLETE, NULL, NULL }
    },
    (Rule[]){ // Open files with addresses in the editor
        { ISSET, "EDITOR", NULL },
        { MATCHES, "data", "^([^:]+):([0-9]+)" },
        { ISFILE, "$1", NULL },
        { LAUNCH, "$EDITOR $0", NULL },
        { COMPLETE, NULL, NULL }
    },
    (Rule[]){ // If it's an existing text file, open it with editor
        { ISSET, "EDITOR", NULL },
        { ISFILE, "$data", NULL },
        { CHECK, "file --mime '$file' | grep -q 'text/'", NULL },
        { LAUNCH, "$EDITOR '$file'", NULL },
        { COMPLETE, NULL, NULL }
    },
    (Rule[]){ // If it's an existing directory, open it with system default
        { ISDIR, "$data", NULL },
        { LAUNCH, OPENCMD " $data", NULL },
        { COMPLETE, NULL, NULL }
    },
};

/******************************************************************************/

char* getvar(char* val) {
    if (strlen(val) == 1 && isdigit(*val))
        val = Matches[*val - '0'];
    else
        val = getenv(val);
    return (val ? val : "");
}

char* eval(char* str) {
    static bool inited = false;
    static char* patt = "\\$([a-zA-Z0-9_]+)";
    static regex_t regex;

    if (!inited && (regcomp(&regex, patt, REG_EXTENDED) < 0)) {
        perror("regcomp() :");
        exit(1);
    }

    regmatch_t matches[2] = {0};
    if (regexec(&regex, str, nelem(matches), matches, 0) < 0) {
        return str;
    } else if (matches[1].rm_so > 0) {
        char* var = strndup(str+matches[1].rm_so, matches[1].rm_eo-matches[1].rm_so);
        char* val = getvar(var);
        size_t sz = strlen(str) + strlen(val);
        char* exp = calloc(1, sz);
        strncat(exp, str, matches[0].rm_so);
        strcat(exp, val);
        strcat(exp, str + matches[0].rm_eo);
        return eval(exp);
    } else {
        return str;
    }
}

/******************************************************************************/

bool complete(void) {
    exit(0);
    return false;
}

bool matches(char* var, char* patt) {
    regex_t regex = {0};
    regmatch_t matches[10] = {0};
    if (regcomp(&regex, patt, REG_EXTENDED) == 0) {
        var = getvar(var);
        memset(Matches, 0, sizeof(Matches));
        int err = regexec(&regex, var, nelem(matches), matches, 0);
        for (int i = 0; i < 10 && matches[i].rm_so >= 0; i++) {
            Matches[i] = strndup(var+matches[i].rm_so, matches[i].rm_eo-matches[i].rm_so);
        }
        return (err == 0);
    }
    return false;
}

bool var_is(char* var, char* val) {
    return (strcmp(getvar(var), eval(val)) == 0);
}

bool var_isset(char* var) {
    return (getenv(var) != NULL);
}

bool var_isdir(char* var) {
    struct stat st = {0};
    char* path = eval(var);
    if ((stat(path, &st) < 0) && (errno == ENOENT)) {
        return false;
    } else if (S_ISDIR(st.st_mode)) {
        setenv("file", var, 1);
        return true;
    } else {
        return false;
    }
}

bool var_isfile(char* var) {
    struct stat st = {0};
    char* path = eval(var);
    if ((stat(eval(var), &st) < 0) && (errno == ENOENT)) {
        return false;
    } else if (!S_ISDIR(st.st_mode)) {
        return true;
    } else {
        return false;
    }
}

bool var_set(char* var, char* val) {
    return (setenv(var, eval(val), 1) == 0);
}

bool var_unset(char* var) {
    return (unsetenv(var) == 0);
}

bool find_file(char* file) {
    return false;
}

bool exec(char* cmd) {
    return false;
}

bool launch(char* cmd) {
    int pid = fork();
    if (pid > 0) {
        return true;
    } else if (pid == 0) {
        char* shellcmd[] = { getvar("SHELL"), "-c", NULL, NULL };
        if (!shellcmd[0]) shellcmd[0] = "/bin/sh";
        shellcmd[2] = eval(cmd);
        exit(execvp(shellcmd[0], shellcmd));
    }
    return false;
}

bool checkcmd(char* cmd) {
    return false;
}

bool apply_rule(Rule* rule) {
    switch (rule->type) {
        case COMPLETE: exit(0);
        case MATCHES:  return matches(rule->arg1, rule->arg2);
        case IS:       return var_is(rule->arg1, rule->arg2);
        case ISSET:    return var_isset(rule->arg1);
        case ISDIR:    return var_isdir(rule->arg1);
        case ISFILE:   return var_isfile(rule->arg1);
        case SET:      return var_set(rule->arg1, rule->arg2);
        case UNSET:    return var_unset(rule->arg1);
        case FINDFILE: return find_file(rule->arg1);
        case EXEC:     return exec(rule->arg1);
        case LAUNCH:   return launch(rule->arg1);
        case CHECK:    return checkcmd(rule->arg1);
    }
    return false;
}

/******************************************************************************/

void usage(char* pname) {
    fprintf(stderr, "Usage: %s [ITEM]\n", pname);
    exit(1);
}

int main(int argc, char** argv) {
    if (argc != 2) usage(argv[0]);
    setenv("data", argv[1], 1);
    for (int i = 0; i < nelem(BuiltinRules); i++) {
        for (Rule* rule = BuiltinRules[i]; true; rule++) {
            //printf("%d '%s' '%s'\n", rule->type, rule->arg1, rule->arg2);
            if (!apply_rule(rule))
                break;
        }
        puts("");
    }
    return 1;
}
