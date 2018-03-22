#define _POSIX_C_SOURCE 200809L
#include <stdc.h>
#include <utf.h>
#include <edit.h>
#include <win.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>

#define MAX_JOBS 1024

static struct pollfd Jobs[MAX_JOBS];

bool exec_poll(int fd, int ms) {
    int njobs = 0;
    if (fd > 0) {
        Jobs[0].fd = fd;
        Jobs[0].events = POLLIN;
        Jobs[0].revents = 0;
        njobs = 1;
    }

    long ret = poll(Jobs, njobs, ms);

    /* reap zombie processes */
    for (int status; waitpid(-1, &status, WNOHANG) > 0;);

    return (ret > 0);
}

int exec_reap(void) {
    return 0;
}

int exec_spawn(char** cmd, int* in, int* out) {
    return -1;
}

void exec_job(char** cmd, char* data, size_t ndata, View* dest) {

}

#if 0
static int execute(char** cmd, Proc* proc) {
    int fds[2];
    /* create the sockets */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0)
        return -1;
    /* create the process */
    if ((proc->pid = fork()) < 0) {
        close(fds[0]), close(fds[1]), proc->fd = -1;
    } else if (0 == proc->pid) {
        /* redirect child process's io to the pipes */
        if ((dup2(fds[1], 0) < 0) || (dup2(fds[1], 1) < 0) || (dup2(fds[1], 2) < 0)) {
            perror("failed to pipe");
            exit(1);
        }
        /* execute the process */
        close(fds[0]);
        exit(execvp(cmd[0], cmd));
    } else {
        close(fds[1]), proc->fd = fds[0];
    }
    return proc->pid;
}
#endif
