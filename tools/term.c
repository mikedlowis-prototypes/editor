#define _POSIX_C_SOURCE 200809L
#include <stdc.h>
#include <unistd.h>

#define PIPE_READ 0
#define PIPE_WRITE 1

typedef struct {
    int pid; /* process id of the child process */
    int in;  /* file descriptor for the child process's standard input */
    int out; /* file descriptor for the child process's standard output */
} Process;

static int execute(char** cmd, Process* proc) {
    int inpipe[2], outpipe[2];
    /* create the pipes */
    if ((pipe(inpipe) < 0) || (pipe(outpipe) < 0))
        return -1;
    /* create the process */
    proc->pid = fork();
    if (proc->pid < 0) {
        /* signal that we failed to fork */
        proc->in  = -1;
        proc->out = -1;
    } else if (0 == proc->pid) {
        /* redirect child process's io to the pipes */
        if ((dup2(inpipe[PIPE_READ], STDIN_FILENO) < 0)    ||
            (dup2(outpipe[PIPE_WRITE], STDOUT_FILENO) < 0)) {
            perror("failed to pipe");
            exit(1);
        }
        /* execute the process */
        close(inpipe[PIPE_WRITE]);
        close(outpipe[PIPE_READ]);
        dup2(STDOUT_FILENO, STDERR_FILENO);
        close(STDERR_FILENO);
        exit(execvp(cmd[0], cmd));
    } else {
        close(inpipe[PIPE_READ]);
        close(outpipe[PIPE_WRITE]);
        proc->in   = inpipe[PIPE_WRITE];
        proc->out  = outpipe[PIPE_READ];
    }
    return proc->pid;
}

int main(int argc, char** argv){
    setenv("PS1", "$ ", 1);
    Process proc;
    execute((char*[]){"/bin/sh", "-i", NULL}, &proc);
    
    int nbytes = 0;
    char buffer[8192];
    while (true) {
        puts("foo");
        nbytes = read(STDIN_FILENO, buffer, sizeof(buffer));
        if (nbytes < 0) break;
        puts("bar");
        write(proc.in, buffer, nbytes);
        puts("baa");
        while ((nbytes = read(proc.out, buffer, sizeof(buffer))) > 0) {
            puts("boo");
            write(1, buffer, nbytes);
        }
        puts("baz");
        if (nbytes < 0) break;
    }
    close(proc.in);
    close(proc.out);
        
    return 0;
}
