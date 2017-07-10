#define _XOPEN_SOURCE 700
#include <stdc.h>
#include <utf.h>
#include <edit.h>
#include <unistd.h>
#include <poll.h>

struct event_data {
    int iodir;
    void* data;
    event_cbfn_t fn;
};

static size_t NumDescriptors = 0;
static struct pollfd* Descriptors = NULL;
static struct event_data* EventData = NULL;

bool event_poll(int ms) {
    /* poll for new events */
    long n = poll(Descriptors, NumDescriptors, ms);
    if (n < 0) die("poll() :");
    if (n == 0) return false;

    /* Handle any events that occurred */
    for (int i = 0; i < NumDescriptors; i++) {
        /* skip any eventless entries */
        if (!Descriptors[i].revents) continue;

        /* if a requested event occurred, handle it */
        if (Descriptors[i].revents & Descriptors[i].events) {
            EventData[i].fn(Descriptors[i].fd, EventData[i].data);
            Descriptors[i].revents = 0;
        }

        /* if the desriptor is done or errored, throw it out */
        if (Descriptors[i].revents & (POLLERR|POLLHUP)) {
            close(Descriptors[i].fd);
            for (int x = i+1; x < NumDescriptors; x++) {
                Descriptors[x-1] = Descriptors[x];
                EventData[x-1]   = EventData[x];
            }
            NumDescriptors--;
        }
    }

    return true;
}

void event_watchfd(int fd, int iodir, event_cbfn_t fn, void* data) {
    int idx = NumDescriptors++;
    Descriptors = realloc(Descriptors, NumDescriptors * sizeof(struct pollfd));
    EventData   = realloc(EventData,   NumDescriptors * sizeof(struct event_data));
    if (!Descriptors || !EventData)
        die("event_Watchfd() : out of memory\n");
    Descriptors[idx].fd = fd;
    EventData[idx].data = data;
    EventData[idx].fn   = fn;
    switch (iodir) {
        case INPUT:  Descriptors[idx].events = POLLIN; break;
        case OUTPUT: Descriptors[idx].events = POLLOUT; break;
        case NOTIFY: Descriptors[idx].events = (POLLIN|POLLOUT); break;
    }
}
