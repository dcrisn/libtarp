#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#include <tarp/common.h>
#include <tarp/log.h>

#include <tarp/timeutils.h>
#include <tarp/event.h>

/*
 * Basic example of using part of the events C api;
 *
 * 3 callbacks are created:
 *
 * 1) 'writer': a timer-based one that re-registers itself on every call --
 * in effect becoming a periodic/interval callback. This callback writes to
 * the write-end of a pipe every time.
 *
 * 2) 'reader': a fd-event based one that is called when the read-end of the pipe
 *    is readable.
 *
 * 3) 'extracb' : just an additional callback added to the mix. No particular
 * purpose.
 *
 * Error checking, resource freeing etc ommited for simplicity.
 */

static int waker_read;
static int waker_write;

void reader(struct fd_event *fdev, int fd, uint32_t events, void *priv){
    UNUSED(priv);  /* could be used to pass any user data */
    UNUSED(events);

    assert(fdev);
    assert(fd);
    assert(waker_read);

    uint8_t buf[1024];
    int bytes = read(fd, buf, ARRLEN(buf));
    UNUSED(bytes);

    info("called reader at %f", time_now_monotonic_dbs());
}

void writer(struct timer_event *tev, void *priv){
    assert(priv);
    assert(tev);

    info("called writer at %f", time_now_monotonic_dbs());

    char buf[8] = {1, 2, 3, 4};
    int num_written = write(waker_write, buf, ARRLEN(buf));
    UNUSED(num_written);

    Evp_set_timer_interval_ms(tev, 1000);
    Evp_register_timer(priv, tev);
}

void extracb(struct timer_event *tev, void *priv){
    assert(priv);
    assert(tev);

    info("called extracb at %f;", time_now_monotonic_dbs());

    Evp_set_timer_interval_ms(tev, 4000);
    Evp_register_timer(priv, tev);
}

int main(int argc, char *argv[]){
    UNUSED(argc); UNUSED(argv);

    struct evp_handle *evp = Evp_new();

    // set up a pipe for example purposes; error checking ommitted
    int pipefds[2];
    int rc = pipe(pipefds);
    UNUSED(rc);
    waker_read = pipefds[0];
    waker_write = pipefds[1];
    info("waker_read: %i waker_write: %i", waker_read, waker_write);

    struct timer_event tev;
    Evp_init_timer_ms(&tev, 1000, writer, evp);
    Evp_register_timer(evp, &tev);

    struct timer_event tev2;
    Evp_init_timer_ms(&tev2, 4000, extracb, evp);
    Evp_register_timer(evp, &tev2);

    struct fd_event fdev;
    Evp_init_fdmon(&fdev, waker_read, FD_EVENT_READABLE, reader, NULL);
    Evp_register_fdmon(evp, &fdev);

    Evp_run(evp, -1);
}


