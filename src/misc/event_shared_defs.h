#ifndef EVENT_SHARED_DEFS_H__
#define EVENT_SHARED_DEFS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>

#include <tarp/dllist.h>
#include <tarp/staq.h>

#include <tarp/event.h>

struct user_event {
    struct staqnode link;
    unsigned event_type;
    void *data;
};

/*
 * opaque handle; defined specifically for each platform's event API;
 * see linux-event.c for example. */
struct os_event_api_handle;

/*
 * Opaque Event Pump handle;
 *
 * The handle consists of various internal data structure heads. This is
 * mostly FIFOs. In the future possibly the API could be augmented e.g. for
 * user-defined events to enqueue them into a priority queue instead, optionally.
 *
 * (1) Timer queue. A *sorted* list of timers; insertions must ensure the list
 * remains sorted. The doubly-linked nodes allow for O(1) removal on cancellation
 * and when traversing the list for processing timer expirations. The main
 * event loop always goes to sleep for the duration given by MIN(timers).
 * On unblocking, the event pump goes in a specific order (see Evp_start fmi)
 * through the relevant queues and dispatches queued events.
 * This queue is populated by the user through e.g. Evp_register_timer_ms.
 *
 * (2) Event queue. A list of file descriptor events; this queue is populated
 * from the code that implements the interface with the OS-specic event API.
 *
 * (3) a semaphore-like file descriptor. This is in the style of the self-pipe
 * trick. Used to unblock epoll. Since the main loop blocks in a call
 * to pump_os_events (which is ultimately epoll etc depending on the OS),
 * other threads publishing user-defined events or indeed timer expiries
 * need to break interrupt the blocking call. They can do so by writing
 * to this file descriptor. IOW this could easily be replaced by a pipe
 * if portability is a concern.
 *
 * (4) I mentioned above that the main loop goes to sleep until the first
 * timer expiry. This was a rough explanation. The loop blocks in a
 * call to pump_os_events; this call will unblock (at the very latest)
 * when the timerfd expires because: 1) the timerfd is fd-based and the fd
 * is monitored by e.g. epoll (inside pump_os_events) and 2) the timer is armed
 * so as to expire at the same time as the user-specified timer with the
 * earliest expiration time.
 *
 * (5) User event queue. User events are enqueued by any 'publisher' function
 * by calling Evp_push_uev. Once enqueued, the events only get dequeued when
 * processed. Since processing is always FIFO -style, a simple singly-linked
 * list suffices here.
 *
 * (6) The publisher(s) of user events could well be in separate threads
 * relative to the thread running the event pump. To ensure thread-safety,
 * Evp_push_uev will use this mutex to protect enqueue-ing actions to uevq.
 * Similarly, dispatch_events will use the mutex to protect dequeue-ing actions
 * from uevq.
 *
 * (7) This stores pointers to the user-defined event callback wrappers.
 * O(1) lookup can be had by using the event_type integer as a key in
 * an array for direct addressing. Of course, the event_type range must
 * be kept within reasonable limit -- see MAX_USER_EVENT_TYPE_VALUE fmi.
 */
struct evp_handle {
    struct dllist   timers;                                         /* (1) */
    struct dllist   evq;                                            /* (2) */

    struct fd_event sem;                                            /* (3) */
    struct fd_event timerfd;                                        /* (4) */

    struct staq     uevq;                                           /* (5) */
    pthread_mutex_t uev_mtx;                                        /* (6) */
    struct user_event_watch *watch[MAX_USER_EVENT_TYPE_VALUE];      /* (7) */

#ifdef __linux__
    struct os_event_api_handle *osapi;
#else
#error "tarp/event currently not compilable outside Linux"
#endif
};

/*
 * To support a specific platform, an interface consisting of the following
 * routines should be provided. See linux-event.c FMI.
 *
 * Some portability issues may otherwise be:
 *  - eventfd - AFAIK, supported by FREEBSD as well; easily replaceable with
 *  a pipe otherwise.
 *  - timerfd
 */
extern struct os_event_api_handle *get_os_api_handle(void);
extern void destroy_os_api_handle(struct os_event_api_handle *os_api_handle);
extern int pump_os_events(struct evp_handle *handle);

extern int add_fd_event_monitor(
        struct os_event_api_handle *os_api_handle,
        struct fd_event *fdev);

extern int remove_fd_event_monitor(
        struct os_event_api_handle *os_api_handle,
        struct fd_event *fdev);


#ifdef __cplusplus
}   /* extern "C" */
#endif


#endif
