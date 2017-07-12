#define _POSIX_C_SOURCE 200809L
#include <stdc.h>
#include <utf.h>
#include <edit.h>
#include <win.h>
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
} Proc;

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

void cmdreap(void) {
    while(NumChildren && (waitpid(-1, NULL, WNOHANG) > 0))
        NumChildren--;
}

int cmdspawn(char** cmd, int* in, int* out) {
    Proc proc;
    if (execute(cmd, &proc) < 0) {
        perror("failed to execute");
        return -1;
    }
    *in  = proc.in;
    *out = proc.out;
    return proc.pid;
}

int cmdrun(char** cmd, char** err) {
    Proc proc;
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
    Proc proc;
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
    Proc proc;
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
    Proc proc;
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

/******************************************************************************/

typedef struct Job Job;

typedef struct {
    View* view;
    char nbytes;
    char bytes[6];
} Recvr;

struct Job {
    Job* next;       /* Pointer to previous job in the job list */
    Job* prev;       /* Pointer to next job in the job list */
    size_t ndata;    /* number of bytes to write to stdout */
    size_t nwrite;   /* number of bytes written to stdout so far */
    char* data;      /* data to write to stdout */
    Recvr err_recvr; /* view in which the error output will be placed */
    Recvr out_recvr; /* view in which the output will be placed */
    int pid;         /* process id of the running job */
};

static Job* JobList = NULL;

static void send_data(int fd, void* data) {
    Job* job = data;
    long nwrite = write(fd, (job->data + job->nwrite), job->ndata);
    if (nwrite < 0) { free(job->data); close(fd); return; }
    job->ndata  -= nwrite;
    job->nwrite += nwrite;
    if (job->ndata <= 0) { free(job->data); close(fd); return; }
}

static void recv_data(int fd, void* data) {
    static char buf[4096];
    long i = 0, nread = read(fd, buf, sizeof(buf));
    Recvr* rcvr = data;
    if (nread <= 0) { close(fd); return; }
    for (; i < nread;) {
        Rune r;
        size_t len = 0;
        while (!utf8decode(&r, &len, buf[i++]));
        view_insert(rcvr->view, false, r);
    }
}

static void watch_or_close(bool valid, int dir, int fd, void* data) {
    event_cbfn_t fn = (dir == OUTPUT ? send_data : recv_data);
    if (valid)
        event_watchfd(fd, dir, fn, data);
    else
        close(fd);
}

void exec_job(char** cmd, char* data, size_t ndata, View* dest) {
    Proc proc;
    Job* job = calloc(1, sizeof(Job));
    job->pid = execute(cmd, &proc);
    if (job->pid < 0) {
        die("job_start() :");
    } else {
        /* add the job to the job list */
        job->err_recvr.view = win_view(TAGS);
        job->out_recvr.view = dest;
        job->ndata  = ndata;
        job->nwrite = 0;
        job->data   = data;
        job->next   = JobList;
        if (JobList) JobList->prev = job;
        JobList = job;
        /* register watch events for file descriptors */
        watch_or_close((job->data != NULL), OUTPUT, proc.in, job);
        watch_or_close((dest != NULL), INPUT, proc.out, &(job->out_recvr));
        watch_or_close(true, INPUT, proc.err, &(job->err_recvr));
    }
}
