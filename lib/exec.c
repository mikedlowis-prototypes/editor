#define _POSIX_C_SOURCE 200809L
#include <stdc.h>
#include <utf.h>
#include <edit.h>
#include <unistd.h>
#include <sys/wait.h>

static uint NumChildren = 0;

#define PIPE_READ 0
#define PIPE_WRITE 1

typedef struct {
    int pid; /* process id of the child process */
    int in;  /* file descriptor for the child process's standard input */
    int out; /* file descriptor for the child process's standard output */
    int err; /* file descriptor for the child process's standard error */
} Process;

static int execute(char** cmd, Process* proc) {
    int inpipe[2], outpipe[2], errpipe[2];
    /* create the pipes */
    if ((pipe(inpipe) < 0) || (pipe(outpipe) < 0) || (pipe(errpipe) < 0))
        return -1;
    /* create the process */
    proc->pid = fork();
    if (proc->pid < 0) {
        /* signal that we failed to fork */
        proc->in  = -1;
        proc->out = -1;
        proc->err = -1;
    } else if (0 == proc->pid) {
        /* redirect child process's io to the pipes */
        if ((dup2(inpipe[PIPE_READ], STDIN_FILENO) < 0)    ||
            (dup2(outpipe[PIPE_WRITE], STDOUT_FILENO) < 0) ||
            (dup2(errpipe[PIPE_WRITE], STDERR_FILENO) < 0)) {
            perror("failed to pipe");
            exit(1);
        }
        /* execute the process */
        close(inpipe[PIPE_WRITE]);
        close(outpipe[PIPE_READ]);
        close(errpipe[PIPE_READ]);
        exit(execvp(cmd[0], cmd));
    } else {
        close(inpipe[PIPE_READ]);
        close(outpipe[PIPE_WRITE]);
        close(errpipe[PIPE_WRITE]);
        proc->in   = inpipe[PIPE_WRITE];
        proc->out  = outpipe[PIPE_READ];
        proc->err  = errpipe[PIPE_READ];
    }
    return proc->pid;
}

void cmdreap(void) {
    while(NumChildren && (waitpid(-1, NULL, WNOHANG) > 0))
        NumChildren--;
}

int cmdrun(char** cmd, char** err) {
    Process proc;
    if (execute(cmd, &proc) < 0) {
        perror("failed to execute");
        return -1;
    }
    NumChildren++;
    if (err) *err = fdgets(proc.err);
    close(proc.in);
    close(proc.out);
    close(proc.err);
    return proc.pid;
}

char* cmdread(char** cmd, char** err) {
    Process proc;
    if (execute(cmd, &proc) < 0) {
        perror("failed to execute");
        return NULL;
    }
    close(proc.in);
    char* str = fdgets(proc.out);
    close(proc.out);
    if (err) *err = fdgets(proc.err);
    close(proc.err);
    waitpid(proc.pid, NULL, 0);
    return str;
}

void cmdwrite(char** cmd, char* text, char** err) {
    Process proc;
    if (execute(cmd, &proc) < 0) {
        perror("failed to execute");
        return;
    }
    if (text && write(proc.in, text, strlen(text)) < 0) {
        perror("failed to write");
        return;
    }
    close(proc.in);
    if (err) *err = fdgets(proc.err);
    close(proc.err);
    close(proc.out);
    waitpid(proc.pid, NULL, 0);
}

char* cmdwriteread(char** cmd, char* text, char** err) {
    Process proc;
    if (execute(cmd, &proc) < 0) {
        perror("failed to execute");
        return NULL;
    }
    if (text && write(proc.in, text, strlen(text)) < 0) {
        perror("failed to write");
        return NULL;
    }
    close(proc.in);
    char* str = fdgets(proc.out);
    close(proc.out);
    if (err) *err = fdgets(proc.err);
    close(proc.err);
    waitpid(proc.pid, NULL, 0);
    return str;
}
