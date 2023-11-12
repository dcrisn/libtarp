#ifndef TARP_EVENT_H
#define TARP_EVENT_H


#ifdef __cplusplus
extern "C" {
#endif

#include <tarp/dllist.h>

#include "event_flags.h"

/*
 * Maximum number of of user-defined event types. This is the maximum
 * value that is acceptable for passing to Evp_add_uev_watch and
 * Evp_push_uev.
 * NOTE The semantics of the values in the [0, MAX_USER_EVENT_TYPE_VALUE)
 * are entirely user-defined. */
#define MAX_USER_EVENT_TYPE_VALUE 30

struct timer_event;
struct fd_event;
struct user_event_watch;

/*
 * The following signatures are for user-provided callbacks associated
 * with the different event types exposed through this library.
 *
 * The arguments to the callbacks are as follows:
 *
 * fdev, tev, uev are pointers to the same structure that was passed
 * to the corresponding registration function (e.g. Evp_add_timer_ms)
 * when registering the callback. This structure should not be modified
 * by the user and once associated with a callback it must not be
 * destroyed, freed etc before disassociation. IOW, de-register the
 * callback before freeing/destroying this structure.
 *
 * fd - (for fd_event_callbacs) the file descriptor for which an event
 * has been generated i.e. for which the callback has been invoked.
 *
 * priv - a pointer (potentially NULL) to some arbitrary data. The user can
 * pass this in at registration time, change it inside the callback etc. It
 * will not be modified or freed in any way by the API.
 *
 * user_event_callback's take some particular arguments:
 *  - event_type: the type of the event whose generation caused the callback
 *  to be invoked. This is user defined. See comment above
 *  MAX_USER_EVENT_TYPE_VALUE fmi.
 *  - data: like 'priv', but this is passed by the user event publisher when
 *  calling Evp_push_uev. If this is a pointer to dynamically-allocated data,
 *  it is the responsibility of the user to free this as and when appropriate
 *  (e.g. in the callback or otherwise) to avoid memory leaks.
 */
typedef void (*timer_callback)(struct timer_event *tev, void *priv);
typedef void (*fd_event_callback)(struct fd_event *fdev, int fd, void *priv);

typedef void (*user_event_callback)(
        struct user_event_watch *uev,
        unsigned event_type,
        void *data,
        void *priv);


/*
 * Opque Event pump handle. The user gets one through Evp_new()
 * and must destroy it when no longer needed by calling Evp_destroy.
 */
struct evp_handle;

/*
 * An evp_handle or NULL if this could not be obtained;
 * If it fails, it is because of system or library calls e.g.
 * failure to initialize a mutex. */
struct evp_handle *Evp_new(void);

/*
 * Destroy all internal state associated with the evp handle, then
 * deallocate the evp handle itself and set the pointer to NULL.
 * --> handle must be non-NULL and point to a live handle.
 */
void Evp_destroy(struct evp_handle **handle);

/*
 * Start the event pump. From this point on, all registered callbacks
 * will be invoked as appropriate. New ones can be added and existing
 * ones can be removed on request at any time. */
void Evp_run(struct evp_handle *handle);

/*
 * Initialize the internal state of the timer callback and
 * set the interval duration.
 *
 * The interval (specified in seconds, microseconds, or milliseconds) is
 * stored as a duration. Only when Evp_register_timer is called does
 * it get converted to an absolute timepoint internally. Practically
 * this means a timer can be initialized and registered at different
 * times, without risking the timer elapsing in the meantime.
 */
void Evp_init_timer_secs(
        struct timer_event *tev, uint32_t seconds,
        timer_callback cb, void *priv);

void Evp_init_timer_ms(
        struct timer_event *tev, uint32_t milliseconds,
        timer_callback cb, void *priv);

void Evp_init_timer_us(
        struct timer_event *tev, uint32_t microseconds,
        timer_callback cb, void *priv);

void Evp_init_timer_fromtimespec(
        struct timer_event *tev, struct timespec *timespec,
        timer_callback cb, void *priv);

/*
 * Set/update the timer interval duration; NOTE nothing else gets
 * (re)initialized. */
void Evp_set_timer_interval_secs(struct timer_event *tev, uint32_t seconds);
void Evp_set_timer_interval_ms(struct timer_event *tev, uint32_t milliseconds);
void Evp_set_timer_interval_us(struct timer_event *tev, uint32_t microseconds);
void Evp_set_timer_interval_fromtimespec(struct timer_event *tev,
        struct timespec *timespec);

/*
 * Register a timer with the event pump such that it is invoked on an
 * interval expiration. When this is called, an absolute timepoint is
 * created based on the timer's duration.
 *
 * NOTE the handle and tev must be non-NULL and must have already been
 * initialized.
 *
 * NOTE the timer tev must not currently be active. i.e. the timer must
 * not be registered consecutively with no intermediate de-registration.
 *
 * NOTE otoh it is safe to call unregister_timer on an *un*registered timer.
 *
 * NOTE a timer is automatically unregistered on each expiration. When the
 * callback gets invoked, the timer will have already been unregistered.
 * IOW to get the effect of a periodic timer, the registered callback should
 * re-register itself (by calling set_timer_interval and then register_timer).
 */
int Evp_register_timer(struct evp_handle *handle, struct timer_event *tev);
void Evp_unregister_timer(struct evp_handle *handle, struct timer_event *tev);

int Evp_init_fdmon(
        struct fd_event *fdev, int fd, uint32_t flags,
        fd_event_callback cb, void *priv);

int Evp_register_fdmon(struct evp_handle *handle, struct fd_event *fdev);
void Evp_unregister_fdmon(struct evp_handle *handle, struct fd_event *fdev);

/*
 * Register a callback to be invoked for specific user events;
 * The event_type is a number that *must* be smaller than
 * MAX_USER_EVENT_TYPE_VALUE identifying the type of event the callback
 * is to be bound to. The event types as well as the semantics of the 'data'
 * pointer are defined by the user.
 *
 * Events of the specified type are pushed using Evp_push_uev; callbacks
 * bound to that particular event type are then invoked. The 'data' argument
 * is also passed to them unchanged.
 *
 * NOTE If an event is pushed with no callback to handle it, the event is
 * silently removed.
 *
 * NOTE
 * Only one callback can be bound to a particular event type; if this does
 * not suffice for a given scenario, the user should maintain state in
 * their own callback and multiplex as appropriate.
 *
 * NOTE if a callback has already been bound for a particular event type,
 * it must be un-bound before a new one can be registered in its place.
 */
int Evp_init_uev_watch(
        struct user_event_watch *uev, unsigned event_type,
        user_event_callback cb, void *priv);

int Evp_register_uev_watch(struct evp_handle *handle, struct user_event_watch *uev);
void Evp_unregister_uev_watch(struct evp_handle *handle, struct user_event_watch *uev);
int Evp_push_uev(struct evp_handle *handle, unsigned event_type, void *data);


#include "impl/event_impl.h"

#ifdef __cplusplus
}   /* extern "C" */
#endif


#endif

