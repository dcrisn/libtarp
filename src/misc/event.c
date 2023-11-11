#include <assert.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>

#include <tarp/common.h>
#include <tarp/log.h>
#include <tarp/container.h>
#include <tarp/timeutils.h>
#include <tarp/ioutils.h>
#include <tarp/dllist.h>
#include <tarp/staq.h>
#include <tarp/error.h>

#include <tarp/event.h>
#include "event_shared_defs.h"

#define EVP_DEFAULT_WAIT_TIME_SECS 36000


#ifdef __linux__
#else
// if defined __FreeBSD__ // TODO
#error "OS-specific event API interface missing"
#endif

/*
 * Ensure the inputs (secs, msecs, usecs -- as u32s) can always
 * be stored in a time_t without overflow. */
static_assert(sizeof(uint32_t) < sizeof(time_t),
        "sizeof uint32_t >= sizeof time_t");


// staqnode destructor
void user_event_destructor(struct staqnode *node){
    assert(node);
    salloc(0, get_container(node, struct user_event, link));
}

// NOTE the os api handle must have been initialized
static inline int initialize_eventfd_semaphore(struct evp_handle *handle){
    assert(handle);
    int rc;

    if ((rc = eventfd(0, EFD_CLOEXEC)) < 0){
        error("Failed to create eventfd: '%s'", strerror(errno));
        return -1;
    }

    handle->sem.evmask = FD_EVENT_READABLE;
    handle->sem.fd = rc;

    if (add_fd_event_monitor(handle->osapi, &handle->sem) != 0) return -1;

    return 0;
}

// NOTE the os api handle must have been initialized
static inline int initialize_wait_timer(struct evp_handle *handle){
    assert(handle);
    int rc;

    rc = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    if (rc < 0){
        error("Failed to create timerfd: '%s'", strerror(errno));
        return -1;
    }

    handle->timerfd.evmask = FD_EVENT_READABLE;
    handle->timerfd.fd = rc;

    if (add_fd_event_monitor(handle->osapi, &handle->timerfd) != 0) return -1;

    return 0;
}

// NOTE that the pthread API does not normally set errno
static int initialize_uevq_mutex(struct evp_handle *handle){
    int rc = 0;
    pthread_mutexattr_t attr;

    if ((rc = pthread_mutexattr_init(&attr)) != 0){
        error("pthread_attr_init error (%d)", rc);
        return rc;
    }

    if ((rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK)) != 0){
        error("pthread_mutexattr_settype error (%d)", rc);
        pthread_mutexattr_destroy(&attr);
        return rc;
    }

    if ((rc = pthread_mutex_init(&handle->uev_mtx, &attr)) != 0){
        error("pthread_mutex_init error (%d)", rc);
        pthread_mutexattr_destroy(&attr);
        return rc;
    }

    pthread_mutexattr_destroy(&attr);
    return 0;
}

static int initialize_event_pump_handle(struct evp_handle *handle){
    assert(handle);

    int rc = ERROR_RUNTIMEERROR;

    if ((handle->osapi = get_os_api_handle()) == NULL) return rc;

    /* Initialize fd-based semaphore */
    if (initialize_eventfd_semaphore(handle) != 0) return rc;

    /* Initialize fd-based timer */
    if (initialize_wait_timer(handle) != 0) return rc;

    /* initialize mutext */
    if (initialize_uevq_mutex(handle) != 0) return rc;

    Dll_init(&handle->timers, NULL);
    Dll_init(&handle->evq, NULL);
    Staq_init(&handle->uevq, user_event_destructor);
    memset(handle->watch, 0, sizeof(struct user_event_watch *) * ARRLEN(handle->watch));

    return ERRORCODE_SUCCESS;
}

struct evp_handle *Evp_new(void){
    struct evp_handle *handle = salloc(sizeof(struct evp_handle), NULL);

    if (initialize_event_pump_handle(handle) != ERRORCODE_SUCCESS){
        Evp_destroy(&handle);
    }

    return handle;
}

/*
 * Populate 'tspec' with an absolute MONOTONIC_CLOCK timepoint.
 * The difference between the timepoint and NOW is how long until the first
 * user-specified timer expires. If there is *no* timer in the timer queue,
 * then any arbitrary value (later than NOW) for the timepoint will do,
 * since this gets updated/recomputed each time the main event pump loop
 * unblocks.
 */
static inline void pick_shortest_wait_time(
        struct timespec *tspec,
        const struct dllist *timerq)
{
    assert(tspec);
    assert(timerq);

    if (Dll_empty(timerq)){
        *tspec = time_now_monotonic();
        tspec->tv_sec += EVP_DEFAULT_WAIT_TIME_SECS;
        if (tspec->tv_sec < EVP_DEFAULT_WAIT_TIME_SECS){
            warn("timespec overflow after EVP_DEFAULT_BLOCK_TIME_SECS addition");
        }
        return;
    }

    struct timer_event *tev = Dll_front(timerq, struct timer_event, link);
    assert(tev);
    *tspec = tev->tspec;   /* already an absolute timepoint */
}

/*
 * Create an fd timer set to expire either:
 * 1) at the same time as the earliest-expiring timer in the timer queue
 * 2) at some arbitrary time otherwise --- this makes no difference.
 *
 * Note if the timer is already set, re-arming it simply resets and updates
 * its state. */
static int wake_on_first_timer(struct evp_handle *handle){
    struct itimerspec itspec;
    memset(&itspec, 0, sizeof(struct itimerspec));
    pick_shortest_wait_time(&itspec.it_value, &handle->timers);
    assert(itspec.it_value.tv_nsec < 999999999L);

    int rc = timerfd_settime(handle->timerfd.fd, TFD_TIMER_ABSTIME, &itspec, NULL);
    if (rc != 0){
        error("Failed to arm timerfd: '%s'", strerror(errno));
        return rc;
    }

    return ERRORCODE_SUCCESS;
}

// true if ts <= reference (monotonic time).
// if reference=NULL, NOW is used.
static inline bool elapsed(struct timespec *ts, struct timespec *reference){
    assert(ts);

    struct timespec now;
    if (!reference){
        now = time_now_monotonic();
        reference = &now;
    }

    return lte(ts, reference, timespec_cmp);
}

/*
 * Ensure locking/unlocking the mutex never fails; if it does, it is
 * almost certainly a bug here and there is either a deadlock, corruption etc.
 * Recovery from this is a fool's errand. The problem needs to be fixed offline.
 * On failure, just exit (and fix the problem).
 */
static inline void lock_mutex(pthread_mutex_t *mtx){
    int rc;
    THROWS(ERROR_RUNTIMEERROR, (rc = pthread_mutex_lock(mtx)) != 0,
            "pthread_mutex_lock error (%d)", rc);
}

static inline void unlock_mutex(pthread_mutex_t *mtx){
    int rc;
    THROWS(ERROR_RUNTIMEERROR, (rc = pthread_mutex_unlock(mtx)) != 0,
            "pthread_mutex_unlock error (%d)", rc);
}

/*
 * Look at every events list; if any unhandled events exist, then invoke
 * the associated event callback.
 *
 * The ORDER the lists are looked at is important. E.g. interrupts get looked
 * at first, then signals, then timers, then fd events, then user-defined
 * events, and so on. NOTE Currently only timers, fd events, and user-defined
 * events are implemented.
 *
 * Return the number of events dispatched. If time_taken is non-NULL,
 * then the number of milliseconds that the function took to run is stored
 * in it (as a floating point number). This is useful for debugging etc.
 */
static unsigned dispatch_events(
        struct evp_handle *handle, double *time_taken)
{
    assert(handle);

    uint64_t buff;
    unsigned num_handled = 0;
    int rc = 0;

    double start;
    if (time_taken) start = time_now_monotonic_dbms();

    struct timer_event *tev;
    struct fd_event *fdev;
    struct user_event *uev;

    /* handle timer expirations */
    struct timespec now = time_now_monotonic();
    Dll_foreach(&handle->timers, tev, struct timer_event, link){
        if (!elapsed(&tev->tspec, &now)) break;
        Dll_popnode(&handle->timers, tev, link);
        assert(tev->cb);
        tev->registered = false;
        tev->cb(tev, tev->priv);
        num_handled++;
    }

    /* Handle OS events */
    Dll_foreach(&handle->evq, fdev, struct fd_event, link){
        Dll_popnode(&handle->evq, fdev, link);

        /* semaphore post or loop wait timer? reset to 0 and carry on; */
        if (fdev->fd == handle->sem.fd || fdev->fd == handle->timerfd.fd){
            rc = read(fdev->fd, &buff, sizeof(buff));
            UNUSED(rc);
            continue;
        }

        /* else 'normal' fd event; dispatch by invoking callback */
        assert(fdev->cb);
        fdev->cb(fdev, fdev->fd, fdev->priv);
        num_handled++;
    }

    /* Handle user events */
    struct user_event_watch *watch;
    while(true){
        lock_mutex(&handle->uev_mtx);
        uev = Staq_dq(&handle->uevq, struct user_event, link);
        unlock_mutex(&handle->uev_mtx);

        if (!uev) break;   /* empty queue */

        assert(uev->event_type < MAX_USER_EVENT_TYPE_VALUE);
        watch = handle->watch[uev->event_type];

        if (watch){
            assert(watch->cb);
            assert(watch->event_type == uev->event_type);
            watch->cb(watch, watch->event_type, uev->data, watch->priv);
            num_handled++;
        }

        salloc(0, uev);
    }

    if (time_taken) *time_taken = (time_now_monotonic_dbms() - start);
    return num_handled;
}

void Evp_run(struct evp_handle *handle){
    int rc = 0;
    double time;

    for(;;){
        if (wake_on_first_timer(handle) != 0)     break;
        if (pump_os_events(handle) != 0)          break;

        rc = dispatch_events(handle, &time);
        debug("Event pump loop: handled %zu events in %f ms", rc, time);
    }
}

/*
 * Expects tev->tspec to *already* have been populated with an interval
 * duration. This function will then convert it to an absolute timepoint
 * on the monotonic clock by adding the duration to NOW.
 *
 * The timer *must not* be currently registered. */
static int timer_duration_to_timepoint(struct timer_event *tev){
    assert(tev);

    long seconds_before = tev->tspec.tv_sec;

    struct timespec now = time_now_monotonic();
    timespec_add(&now, &tev->tspec, &tev->tspec);

    // check for overflow
    if (tev->tspec.tv_sec < seconds_before){
        error("timespec overflow inside Evp_register_timer");
        return ERROR_OUTOFBOUNDS;
    }

    return ERRORCODE_SUCCESS;
}

/*
 * Expects tev->tspec to have already been populated with an interval duration.
 * This function calls timer_duration_to_timepoint and if successful enqueues
 * the timer into the timer queue. */
int Evp_register_timer(struct evp_handle *handle, struct timer_event *tev){
    assert(handle);
    assert(tev);
    assert(tev->cb);

    if (tev->registered) return ERROR_INVALIDVALUE;

    int rc = timer_duration_to_timepoint(tev);
    if (rc != ERRORCODE_SUCCESS) return rc;

    struct timer_event *timer;

    if (Dll_empty(&handle->timers)){
        Dll_pushfront(&handle->timers, tev, link);
        tev->registered = true; return ERRORCODE_SUCCESS;
    }

    Dll_foreach(&handle->timers, timer, struct timer_event, link){
        if (gt(&timer->tspec, &tev->tspec, timespec_cmp)){
            Dll_put_before(&handle->timers, timer, tev, link);
            tev->registered = true; return ERRORCODE_SUCCESS;
        }
    }

    // no existing timer expires before this one
    Dll_pushback(&handle->timers, tev, link);
    tev->registered = true;

    return ERRORCODE_SUCCESS;
}

void Evp_set_timer_interval_secs(struct timer_event *tev, uint32_t seconds){
    assert(tev);
    tev->tspec.tv_sec = seconds;
    tev->tspec.tv_nsec = 0;
}

void Evp_set_timer_interval_ms(struct timer_event *tev, uint32_t milliseconds){
    assert(tev);
    tev->tspec.tv_sec = milliseconds / MSECS_PER_SEC;
    tev->tspec.tv_nsec = (milliseconds % MSECS_PER_SEC) * NSECS_PER_MSEC;
}

void Evp_set_timer_interval_us(struct timer_event *tev, uint32_t microseconds){
    assert(tev);
    tev->tspec.tv_sec = microseconds / USECS_PER_SEC;
    tev->tspec.tv_nsec = (microseconds % USECS_PER_SEC) * NSECS_PER_USEC;
}

void Evp_set_timer_interval_fromtimespec(
        struct timer_event *tev, struct timespec *tspec)
{
    assert(tev); assert(tspec);
    tev->tspec.tv_sec = tspec->tv_sec;
    tev->tspec.tv_nsec = tspec->tv_nsec;
}

static inline void initialize_tev(
        struct timer_event *tev, timer_callback cb, void *priv){
    tev->priv = priv;
    tev->cb = cb;
    tev->registered = false;
}

void Evp_init_timer_secs(
        struct timer_event *tev, uint32_t seconds,
        timer_callback cb, void *priv)
{
    Evp_set_timer_interval_secs(tev, seconds);
    initialize_tev(tev, cb, priv);
}

void Evp_init_timer_ms(
        struct timer_event *tev, uint32_t milliseconds,
        timer_callback cb, void *priv)
{
    Evp_set_timer_interval_ms(tev, milliseconds);
    initialize_tev(tev, cb, priv);
}

void Evp_init_timer_us(
        struct timer_event *tev, uint32_t microseconds,
        timer_callback cb, void *priv)
{
    Evp_set_timer_interval_us(tev, microseconds);
    initialize_tev(tev, cb, priv);
}

void Evp_init_timer_fromtimespec(
        struct timer_event *tev, struct timespec *tspec,
        timer_callback cb, void *priv)
{
    assert(tev); assert(tspec);
    Evp_set_timer_interval_fromtimespec(tev, tspec);
    initialize_tev(tev, cb, priv);
}

void Evp_unregister_timer(struct evp_handle *handle, struct timer_event *tev){
    assert(handle);
    assert(tev);

    if (tev->registered){
        Dll_popnode(&handle->timers, tev, link);
        tev->registered = false;
    }
}

int Evp_init_fdmon(
        struct fd_event *fdev,
        int fd,
        uint32_t flags,
        fd_event_callback cb,
        void *priv)
{
    assert(fdev);
    assert(cb);

    if (fd < 0) return ERROR_INVALIDVALUE;

    if (! (flags & FD_EVENT_READABLE || flags & FD_EVENT_WRITABLE)){
        return ERROR_INVALIDVALUE;
    }

    if (fdev->evmask & FD_NONBLOCKING){
	    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    }

    fdev->cb = cb;
    fdev->evmask = flags;
    fdev->fd = fd;
    fdev->priv = priv;
    fdev->registered = false;

    return ERRORCODE_SUCCESS;
}

int Evp_register_fdmon(struct evp_handle *handle, struct fd_event *fdev){
    assert(handle);
    assert(fdev);
    assert(fdev->cb);
    assert(fdev->fd >= 0);
    assert(fdev->evmask & FD_EVENT_READABLE || fdev->evmask & FD_EVENT_WRITABLE);

    if (fdev->registered) return ERROR_INVALIDVALUE;

    int rc = add_fd_event_monitor(handle->osapi, fdev);
    if (rc == 0) fdev->registered = true;
    return rc;
}

void Evp_unregister_fdmon(struct evp_handle *handle, struct fd_event *fdev){
    assert(handle);
    assert(fdev);

    if (fdev->registered){
        fdev->registered = false;
        remove_fd_event_monitor(handle->osapi, fdev);
    }
}

void Evp_destroy(struct evp_handle **handle){
    assert(handle);
    assert(*handle);

    remove_fd_event_monitor((*handle)->osapi, &(*handle)->sem);
    remove_fd_event_monitor((*handle)->osapi, &(*handle)->timerfd);
    close((*handle)->sem.fd);
    close((*handle)->timerfd.fd);

    destroy_os_api_handle((*handle)->osapi);

    pthread_mutex_destroy(&(*handle)->uev_mtx);

    Dll_clear(&(*handle)->timers, false);
    Dll_clear(&(*handle)->evq, false);
    Staq_clear(&(*handle)->uevq, true);

    salloc(0, *handle);
    *handle = NULL;
}

int Evp_init_uev_watch(
        struct user_event_watch *uev,
        unsigned event_type,
        user_event_callback cb,
        void *priv)
{
    assert(uev);
    assert(cb);

    if (event_type >= MAX_USER_EVENT_TYPE_VALUE) return ERROR_OUTOFBOUNDS;

    uev->cb = cb;
    uev->priv = priv;
    uev->event_type = event_type;
    uev->registered = false;

    return ERRORCODE_SUCCESS;
}

int Evp_register_uev_watch(struct evp_handle *handle, struct user_event_watch *uev)
{
    assert(handle);
    assert(uev);
    assert(uev->cb);
    assert(uev->event_type < MAX_USER_EVENT_TYPE_VALUE);

    // aready registered ?
    if (uev->registered) return ERROR_INVALIDVALUE;

    // another callback aready registered ?
    if (uev->event_type) return ERROR_CONFLICT;

    handle->watch[uev->event_type] = uev;
    uev->registered = true;
    return ERRORCODE_SUCCESS;
}


void Evp_unregister_uev_watch(struct evp_handle *handle, struct user_event_watch *uev)
{
    assert(handle);
    assert(uev);

    if (uev->event_type >= MAX_USER_EVENT_TYPE_VALUE) return;
    if (!uev->registered) return;

    handle->watch[uev->event_type] = NULL;
    uev->registered = false;
}

/* Unblock the main event loop via the eventfd */
static inline int notify_event_pushlished(struct evp_handle *handle){
    assert(handle);
    uint64_t buff = 1;  /* ~must~ write 8 bytes */
    return full_write(handle->sem.fd, (uint8_t*)&buff, sizeof(uint64_t));
}

int Evp_push_uev(struct evp_handle *handle, unsigned event_type, void *data)
{
    assert(handle);

    if (event_type >= MAX_USER_EVENT_TYPE_VALUE) return ERROR_OUTOFBOUNDS;

    struct user_event *ev = salloc(sizeof(struct user_event), NULL);
    ev->event_type = event_type;
    ev->data = data;

    lock_mutex(&handle->uev_mtx);
    Staq_enq(&handle->uevq, ev, link);
    unlock_mutex(&handle->uev_mtx);

    return notify_event_pushlished(handle);
}

