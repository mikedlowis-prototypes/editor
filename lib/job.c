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

struct PipeData {
    char* data;
    size_t ndata;
    size_t nwrite;
    View* dest;
};

#define MAX_JOBS 256

static struct pollfd JobFds[MAX_JOBS];
static Job* JobList = NULL;

static void pipe_read(Job* job) {
}

static void pipe_write(Job* job) {
}

static void job_finish(Job* job) {
    //close(job->fd);
    // delete job
    // free(job);
}

static void job_process(int fd, int events) {
    Job* job = JobList; // Get job by fd
    while (job && job->fd != fd)
        job = job->next;
    if (!job) return;
    if (job->readfn && (events & POLLIN))
        job->readfn(job);
    if (job->writefn && (events & POLLOUT))
        job->writefn(job);
    if (!job->readfn && !job->writefn)
        job_finish(job);
}

static int job_exec(char** cmd) {
    int pid, fds[2];
    /* create the sockets */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0)
        return -1;
    /* create the process */
    if ((pid = fork()) < 0) {
        close(fds[0]), close(fds[1]), fds[0] = -1;
    } else if (0 == pid) {
        /* redirect child process's io to the pipes */
        if ((dup2(fds[1], 0) < 0) || (dup2(fds[1], 1) < 0) || (dup2(fds[1], 2) < 0)) {
            perror("failed to pipe");
            exit(1);
        }
        /* execute the process */
        close(fds[0]);
        exit(execvp(cmd[0], cmd));
    } else {
        close(fds[1]);
    }
    return fds[0];
}

bool job_poll(int ms) {
    int njobs = 0;
    /* Add jobs from the job list */
    for (Job *job = JobList; job && njobs < MAX_JOBS; job = job->next) {
        JobFds[njobs].fd = job->fd;
        JobFds[njobs].events = 0;
        JobFds[njobs].revents = 0;
        if (job->readfn) JobFds[njobs].events = POLLIN;
        if (job->writefn) JobFds[njobs].events = POLLOUT;
        if (JobFds[njobs].events) njobs++;
    }
    /* Poll until a job is ready, call the functions based on events */
    long ret = poll(JobFds, njobs, ms);
    for (int i = 0; i < njobs; i++)
        job_process(JobFds[i].fd, JobFds[i].revents);
    /* reap zombie processes */
    for (int status; waitpid(-1, &status, WNOHANG) > 0;);
    return (ret > 0);
}

void job_spawn(int fd, jobfn_t readfn, jobfn_t writefn, void* data) {
    Job *job = calloc(1, sizeof(Job));
    job->fd = fd;
    job->readfn = readfn;
    job->writefn = writefn;
    job->data = data;
    job->next = JobList;
    JobList = job;
}

void job_start(char** cmd, char* data, size_t ndata, View* dest) {
    int fd = job_exec(cmd);
    if (fd >= 0) {
        struct PipeData* pipedata = calloc(1, sizeof(struct PipeData));
        pipedata->data = data;
        pipedata->ndata = ndata;
        pipedata->dest = dest;
        job_spawn(fd, pipe_read, pipe_write, pipedata);
    }
}
