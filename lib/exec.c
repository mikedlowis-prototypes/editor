#define _POSIX_C_SOURCE 200809L
#include <stdc.h>
#include <utf.h>
#include <edit.h>
#include <win.h>
#include <unistd.h>
#include <sys/wait.h>

#define PIPE_READ  0
#define PIPE_WRITE 1

typedef struct {
    int pid; /* process id of the child process */
    int in;  /* file descriptor for the child process's standard input */
    int out; /* file descriptor for the child process's standard output */
    int err; /* file descriptor for the child process's standard error */
} Proc;

typedef struct Job Job;

typedef struct {
    Job* job;     /* pointer to the job the receiver belongs to */
    View* view;   /* destination view */
    size_t beg;   /* start of output */
    size_t count; /* number of bytes written */
} Rcvr;

struct Job {
    Job* next;           /* Pointer to previous job in the job list */
    Job* prev;           /* Pointer to next job in the job list */
    Proc proc;           /* Process id and descriptors */
    size_t ndata;        /* number of bytes to write to stdout */
    size_t nwrite;       /* number of bytes written to stdout so far */
    char* data;          /* data to write to stdout */
    Rcvr err_rcvr;       /* reciever for the error output of the job */
    Rcvr out_rcvr;       /* receiver for the normal output of the job */
    View* dest;          /* destination view where output will be placed */
};

static Job* JobList = NULL;

static void job_closefd(Job* job, int fd);
static bool job_done(Job* job);
static Job* job_finish(Job* job);
static void send_data(int fd, void* data);
static void recv_data(int fd, void* data);
static int watch_or_close(bool valid, int dir, int fd, void* data);
static void rcvr_finish(Rcvr* rcvr);
static int execute(char** cmd, Proc* proc);

bool exec_reap(void) {
    int status;
    Job* job = JobList;
    while (job) {
        if (job_done(job)) {
            rcvr_finish(&(job->out_rcvr));
            rcvr_finish(&(job->err_rcvr));
            waitpid(job->proc.pid, &status, WNOHANG);
            job = job_finish(job);
        } else {
            job = job->next;
        }
    }
    if (JobList == NULL)
        while (waitpid(-1, &status, WNOHANG) > 0);
    return (JobList != NULL);
}

void exec_job(char** cmd, char* data, size_t ndata, View* dest) {
    Job* job = calloc(1, sizeof(Job));
    job->proc.pid = execute(cmd, &(job->proc));
    if (job->proc.pid < 0) {
        die("job_start() :");
    } else {
        /* add the job to the job list */
        job->err_rcvr.view = win_view(TAGS);
        job->err_rcvr.job = job;
        job->out_rcvr.view = dest;
        job->out_rcvr.job = job;
        job->ndata  = ndata;
        job->nwrite = 0;
        job->data   = data;
        job->next   = JobList;
        if (JobList) JobList->prev = job;
        JobList = job;
        /* register watch events for file descriptors */
        bool need_in  = (job->data != NULL),
             need_out = (dest != NULL),
             need_err = (need_in || need_out);
        job->proc.in  = watch_or_close(need_in,  OUTPUT, job->proc.in,  job);
        job->proc.out = watch_or_close(need_out, INPUT,  job->proc.out, &(job->out_rcvr));
        job->proc.err = watch_or_close(need_err, INPUT,  job->proc.err, &(job->err_rcvr));
    }
}

int exec_cmd(char** cmd) {
    Proc proc;
    if (execute(cmd, &proc) < 0) {
        perror("failed to execute");
        return -1;
    }
    /* wait for the process to finish */
    int status;
    waitpid(proc.pid, &status, 0);
    return status;
}

int exec_spawn(char** cmd, int* in, int* out) {
    Proc proc;
    if (execute(cmd, &proc) < 0) {
        perror("failed to execute");
        return -1;
    }
    *in  = proc.in;
    *out = proc.out;
    return proc.pid;
}

static void job_closefd(Job* job, int fd) {
    if (fd >= 0) close(fd), fd = -fd;
    if (job->proc.in  == -fd) job->proc.in  = fd;
    if (job->proc.out == -fd) job->proc.out = fd;
    if (job->proc.err == -fd) job->proc.err = fd;
}

static bool job_done(Job* job) {
    return ((job->proc.in < 0) && (job->proc.out < 0) && (job->proc.err < 0));
}

static Job* job_finish(Job* job) {
    Job* next = job->next;
    if (job->prev) {
        job->prev->next = next;
        if (next)
            next->prev = job->prev;
    } else {
        if (next)
            next->prev = NULL;
        JobList = next;
    }
    free(job->data);
    free(job);
    return next;
}

static void send_data(int fd, void* data) {
    Job* job = data;
    if (fd >= 0) {
        long nwrite = write(fd, (job->data + job->nwrite), job->ndata);
        if (nwrite >= 0) {
            job->ndata  -= nwrite;
            job->nwrite += nwrite;
        }
        if  (nwrite < 0 || job->ndata <= 0)
            close(fd);
    } else {
        job_closefd(job, fd);
    }
}

static void recv_data(int fd, void* data) {
    static char buffer[4096];
    Rcvr* rcvr = data;
    Job* job = rcvr->job;
    View* view = rcvr->view;
    Sel sel = view->selection;

    if (fd >= 0) {
        long i = 0, nread = read(fd, buffer, sizeof(buffer));
        if (nread > 0) {
            if (!rcvr->count)
                rcvr->beg = min(sel.beg, sel.end);
            while (i < nread) {
                Rune r;
                size_t len = 0;
                while (!utf8decode(&r, &len, buffer[i++]));
                view_insert(rcvr->view, false, r);
                rcvr->count++;
            }
        } else {
            close(fd);
        }
    } else {
        job_closefd(job, fd);
    }
}

static int watch_or_close(bool valid, int dir, int fd, void* data) {
    event_cbfn_t fn = (dir == OUTPUT ? send_data : recv_data);
    if (valid)
        event_watchfd(fd, dir, fn, data);
    else
        close(fd), fd = -fd;
    return fd;
}

static void rcvr_finish(Rcvr* rcvr) {
    if (rcvr->count) {
        View* view = rcvr->view;
        Buf* buf = &(rcvr->view->buffer);
        if (rcvr->view == win_view(TAGS))
            buf_chomp(buf), rcvr->count--;
        view->selection.beg = rcvr->beg;
        view->selection.end = rcvr->beg + rcvr->count;
    }
}

static int execute(char** cmd, Proc* proc) {
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
        proc->in  = inpipe[PIPE_WRITE];
        proc->out = outpipe[PIPE_READ];
        proc->err = errpipe[PIPE_READ];
    }
    return proc->pid;
}
