#define _POSIX_C_SOURCE 200809L
#include <stdc.h>
#include <utf.h>
#include <edit.h>
#include <unistd.h>
#include <sys/wait.h>

#define PIPE_READ 0
#define PIPE_WRITE 1

int execute(char** cmd, Process* proc) {
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

void detach(Process* proc) {
    close(proc->in);
    close(proc->out);
    close(proc->err);
}

void terminate(Process* proc, int sig) {
    detach(proc);
    kill(proc->pid, sig);
}

char* cmdread(char** cmd) {
    Process proc;
    if (execute(cmd, &proc) < 0) {
        perror("failed to execute");
        return NULL;
    }
    char* str = fdgets(proc.out);
    detach(&proc);
    waitpid(proc.pid, NULL, 0);
    return str;
}

void cmdwrite(char** cmd, char* text) {
    Process proc;
    if (execute(cmd, &proc) < 0) {
        perror("failed to execute");
        return;
    }
    if (write(proc.in, text, strlen(text)) < 0) {
        perror("failed to write");
        return;
    }
    detach(&proc);
    waitpid(proc.pid, NULL, 0);
}
