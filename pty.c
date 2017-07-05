#define _XOPEN_SOURCE 700
#include <stdc.h>
#include <unistd.h>
#ifdef __MACH__
    #include <util.h>
#else
    #include <pty.h>
#endif
#include <poll.h>
#include <termios.h>

void usage(void) {
   fprintf(stderr, "usage: pty COMMAND ...\n");
   exit(EXIT_FAILURE);
}

int pty_spawn(char** argv) {
    int fd;
    struct termios tio;
    pid_t pid;
    putenv("TERM=dumb");
    switch ( (pid = forkpty(&fd, NULL, NULL, NULL)) ) {
        case -1: // Failed
            die("forkpty() :");
            break;

        case 0: // Child Process
            if (execvp(argv[0], argv) < 0)
                die("execvp('%s', ...) :", argv[0]);
            exit(EXIT_FAILURE);
            break;

        default: // Parent Process
            tcgetattr(fd, &tio);
            tio.c_lflag      &= ~(ECHO | ECHONL);
            tio.c_cc[ VMIN ]  = 1;
            tio.c_cc[ VTIME ] = 0;
            tcsetattr(fd, TCSANOW, &tio);
            break;
    }
    return fd;
}

int pty_input(int ptyfd, char* buf, size_t bufsz) {
    long n;
    if ((n = read(STDIN_FILENO, buf, bufsz)) < 0)
        die("read() stdin :");
    if (n == 0)
        return 1;
    if ((n = write(ptyfd, buf, n)) < 0)
        die("write() subprocess :");
    return 0;
}

int pty_output(int ptyfd, char* buf, size_t bufsz) {
    long n;
    if ((n = read(ptyfd, buf, bufsz)) < 0) {
        if (errno == EIO)
            return 1;
        else
            die("read() subprocess :");
    }
    if ((n = write(STDOUT_FILENO, buf, n)) < 0)
        die("write() stdout :");
    return 0;
}

int main(int argc, char** argv) {
    if (argc == 1) usage();

    /* allocate the buffer, spawn the pty, and setup the polling structures */
    static char buf[4096];
    int done = 0, ptyfd = pty_spawn(++argv);
    struct pollfd pfds[2] = {
        { .fd = STDIN_FILENO, .events = POLLIN },
        { .fd = ptyfd,        .events = POLLIN },
    };

    /* enter the event loop */
    while (!done) {
        /* clear previous events and poll for new ones */
        pfds[0].revents = pfds[1].revents = 0;
        if (poll(pfds,2,-1) < 0)
            die("poll() :");

        /* shutdown if there's an error or the pipes are closed */
        if ((pfds[0].revents | pfds[1].revents) & (POLLERR|POLLHUP))
            done = 1;

        /* pass stdin to the subprocess */
        if (pfds[0].revents & POLLIN)
            done = pty_input(ptyfd, buf, sizeof(buf));

        /* pass subprocess output to stdout */
        if (pfds[1].revents & POLLIN)
            done = pty_output(ptyfd, buf, sizeof(buf));
    }
    close(ptyfd);

    return 0;
}
