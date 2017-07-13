#define _POSIX_C_SOURCE 200809L
#include <stdc.h>
#include <utf.h>
#include <edit.h>
#include <win.h>
#include <unistd.h>
#include <sys/wait.h>

#define PIPE_READ 0
#define PIPE_WRITE 1

typedef struct {
    int pid; /* process id of the child process */
    int in;  /* file descriptor for the child process's standard input */
    int out; /* file descriptor for the child process's standard output */
    int err; /* file descriptor for the child process's standard error */
} Proc;

typedef struct Job Job;

typedef struct {
    View* view;   /* destination view */
    size_t beg;   /* start of output */
    size_t count; /* number of bytes written */
} Rcvr;

struct Job {
    Job* next;     /* Pointer to previous job in the job list */
    Job* prev;     /* Pointer to next job in the job list */
    int pid;       /* process id of the running job */
    size_t ndata;  /* number of bytes to write to stdout */
    size_t nwrite; /* number of bytes written to stdout so far */
    char* data;    /* data to write to stdout */
    Rcvr err_rcvr; /* reciever for the error output of the job */
    Rcvr out_rcvr; /* receiver for the normal output of the job */
    View* dest;    /* destination view where output will be placed */
};

static Job* JobList = NULL;

static void send_data(int fd, void* data);
static void recv_data(int fd, void* data);
static void watch_or_close(bool valid, int dir, int fd, void* data);
static void rcvr_finish(Rcvr* rcvr);
static int execute(char** cmd, Proc* proc);

bool exec_reap(void) {
    int pid;
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
        Job* job = JobList;
        for (; job && job->pid != pid; job = job->next);
        if (job && job->pid == pid) {
            rcvr_finish(&(job->out_rcvr));
            rcvr_finish(&(job->err_rcvr));
            if (job->prev) {
                job->prev->next = job->next;
                job->next->prev = job->prev;
            } else {
                JobList = job->next;
            }
            free(job);
        }
    }
    return (JobList != NULL);
}

void exec_job(char** cmd, char* data, size_t ndata, View* dest) {
    Proc proc;
    Job* job = calloc(1, sizeof(Job));
    job->pid = execute(cmd, &proc);
    if (job->pid < 0) {
        die("job_start() :");
    } else {
        /* add the job to the job list */
        job->err_rcvr.view = win_view(TAGS);
        job->out_rcvr.view = dest;
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
        watch_or_close(need_in,  OUTPUT, proc.in,  job);
        watch_or_close(need_out, INPUT,  proc.out, &(job->out_rcvr));
        watch_or_close(need_err, INPUT,  proc.err, &(job->err_rcvr));
    }
}

void exec_cmd(char** cmd, char* text, char** out, char** err) {
    Proc proc;
    if (execute(cmd, &proc) < 0) {
        perror("failed to execute");
        return;
    }
    /* send the input to stdin of the command */
    if (text && write(proc.in, text, strlen(text)) < 0) {
        perror("failed to write");
        return;
    }
    close(proc.in);
    /* read the stderr of the command */
    if (err) *err = fdgets(proc.err);
    close(proc.err);
    /* read the stdout of the command */
    if (out) *out = fdgets(proc.out);
    close(proc.out);
    /* wait for the process to finish */
    waitpid(proc.pid, NULL, 0);
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

static void send_data(int fd, void* data) {
    Job* job = data;
    long nwrite = write(fd, (job->data + job->nwrite), job->ndata);
    if (nwrite < 0) { free(job->data); close(fd); return; }
    job->ndata  -= nwrite;
    job->nwrite += nwrite;
    if (job->ndata <= 0) { free(job->data); close(fd); return; }
}

static void recv_data(int fd, void* data) {
    static char buffer[4096];
    long i = 0, nread = read(fd, buffer, sizeof(buffer));
    Rcvr* rcvr = data;
    View* view = rcvr->view;
    Buf* buf = &(rcvr->view->buffer);
    Sel sel = view->selection;
    if (nread > 0) {
        if (!rcvr->count) {
            if (sel.end < sel.beg) {
                size_t temp = sel.beg;
                sel.beg = sel.end, sel.end = temp;
            }
            rcvr->beg = sel.beg = sel.end = buf_change(buf, sel.beg, sel.end);
            view->selection = sel;
            buf_loglock(buf);
        }
        for (; i < nread;) {
            Rune r;
            size_t len = 0;
            while (!utf8decode(&r, &len, buffer[i++]));
            view_insert(rcvr->view, false, r);
            rcvr->count++;
        }
    } else {
        close(fd);
    }
}

static void watch_or_close(bool valid, int dir, int fd, void* data) {
    event_cbfn_t fn = (dir == OUTPUT ? send_data : recv_data);
    if (valid)
        event_watchfd(fd, dir, fn, data);
    else
        close(fd);
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
        proc->in   = inpipe[PIPE_WRITE];
        proc->out  = outpipe[PIPE_READ];
        proc->err  = errpipe[PIPE_READ];
    }
    return proc->pid;
}
