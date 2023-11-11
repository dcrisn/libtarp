#ifndef TARP_EVENT_HXX
#define TARP_EVENT_HXX

/*
 * C++ wrapper for tarp/event.h
 *
 * See tarp/event.h FMI.
 *
 * General API notes
 * ==================
 *
 * Callback initialization and lifetime
 * -------------------------------------
 * NOTE a callback object can only be initialized at construction time. It can
 * be activated/deactivated multiple times, but if destructed outright
 * (lambda returns false, see next point), then the callback is dead and can no
 * longer be (re)activated. Instead, a new one must be constructed.
 *
 * NOTE when constructed through the EventPump (more on this below), the
 * callback object is tracked by the event pump. The user can't and is not
 * meant to access it. Therefore, it is expected the user will use a
 * lambda as the callable to be invoked by the callback object so as
 * to capture any state that is needed (which does *not* include the
 * callback object itself).
 *
 * The lambda controls the lifetime of the callback object through its return
 * value: if it returns true, the callback continues to exist and operate as
 * normal (for example, if it is a timer callback, the interval is renewed);
 * if it returns false, this is taken as a signal to destruct the callback
 * object. When this happens, any internal state is deallocated and the event
 * pump 'forgets' the callback such that the relevant destructors are triggered.
 * At this point the callback is dead and a new one must be created if necessary.
 *
 * Abstract and concrete handles
 * -----------------------------
 * The eventpump uses the abstract Callback handle internally. This class
 * defines the interface common to all callback objects. Publicly, the user
 * should ideally not interact with a callback object directly at all and
 * just carry out the callback registration through the EventPump API.
 *
 * NOTE the user *may* instantiate concrete classes directly (TimerEventCallback,
 * FdEventCallback, UserEventCallback). However, there's almost 0 reason
 * for this, since they all expose the same interface (as set by Callback).
 *
 * The only exception is the TimerEventCallback where the advantage
 * of instantiating it directly is that the user is able to change the
 * interval from within the callback on every call. Even this special case
 * is normally exceedingly rare. The disadvantage of direct instantion
 * (and not going through the EventPump API) in all cases is that the user
 * must own the callback object and keep it in scope. The EventPump will not
 * track it. The internal state of the callback object is still destructed
 * when the lambda returns false (i.e. lambda 'dies'), but its actual
 * destructor that frees the object itself will not be called until the
 * user allows it to go out of scope.
 *
 * NOTE
 * If not instantiated directly, any callback object should be created
 * through the EventPump (see the interface below). This should almost
 * always be the case. When doing this, the callback object is tracked and
 * owned implcitly by the EventPump. It is not accessible to (or meant to be
 * accessed by) the user. Their callback (normally a lambda) will just be
 * called as appropriate.
 *
 * The EventPump - Callback connection
 * ------------------------------------
 * Even when instantiated directly by the user, a callback object is
 * associated with an eventpump; this connection is set at construction
 * time and cannot be severed -- i.e. it is not possible to e.g. move
 * a callback from one event pump to another; if this is needed, a new
 * callback object should be created.
 *
 * Initialization and activation
 * ----------------------------------
 * The function object specified to the relevant Callback object constructor
 * must be non-NULL. The only exception is the TimerEventCallback when
 * instantiated directly. In that case the function can be passed as null
 * to the constructor  -- but it *must* be set via set_func before calling
 * activate().
 */

#include <functional>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <unordered_map>

#include <tarp/error.h>
#include <tarp/log.h>

#include "event_flags.h"

extern "C" {
    struct evp_handle;
    struct timer_event;
    struct fd_event;
    struct user_event_watch;
}

namespace tarp {
    using timer_callback = std::function<bool(void)>;
    using fd_callback    = std::function<bool(int fd)>;
    using uev_callback   = std::function<bool(unsigned event_type, void *data)>;


class EventPump;
class RawEventPumpInterface;


/*
 * This class sets the interface for concrete callback objects;
 * subclasses must implement this, though they can expand on it by
 * adding their own methods.
 * NOTE this sets both the public interface (usable by the user and other,
 * uncoupled, functions/classes) and the private interface accessible to
 * friends (e.g. the EventPump). Notice that almost all internal state is
 * encapsulated instead in the CallbackCore subclass for this exact reason.
 *
 * (1) activate/deactivate are used to (un)register a callback without
 * destroying it. This can be used to dynamically pause callbacks in a list
 * without destroying or creating new ones all the time. Of course, to gain
 * access to this interface, direct instantiation of the concrete classes
 * is required -- meaning this interface is inaccessible to the user when
 * using the simpler interface exposed by EventPump.
 *
 * (2) actually destroy the internal state of the callback. After this,
 * a callback can no longer be activated or .call()-ed. A new one must be
 * created. This method is protected because the users are not meant to
 * call it directly -- instead, their lambda (or whatever callable they
 * specified for the callback function) should simply return false to signal
 * destruction.
 */
class Callback {
    friend class tarp::EventPump;
public:
    /* Copy/Move semantics undefined for Callbacks;
     * Callbacks can only be constructed from explicitly specified
     * parameters -- not cloned, copied, or moved from other Callbacks. */
    Callback(const Callback &)                = delete;
    Callback(const Callback &&)               = delete;
    Callback &operator=(const Callback&)      = delete;
    Callback &operator=(Callback &&)          = delete;

    Callback(void)      = default;
    virtual ~Callback() = default;

    virtual int activate(void)                = 0;   // (1)
    virtual void deactivate(void)             = 0;
    virtual bool is_active(void) const        = 0;
    virtual bool is_dead(void) const          = 0;
    virtual void call(void)                   = 0;

protected:
    virtual void set_id(size_t id)            = 0;
    virtual size_t get_id(void) const         = 0;
    virtual void die(void)                    = 0;   // (2)

    struct evp_handle *get_raw_evp_handle(std::shared_ptr<tarp::EventPump> evp);
    void remove_callback_if_tracked(std::shared_ptr<tarp::EventPump> evp) const;
};

/*
 * This class implements most of the interface set by Callback,
 * stores the private state that is common to all concrete callback objects
 * (and which is inaccessible to friends), and uses a template pattern
 * to impose a structure to be followed by concrete subclasses as
 * regards (de)initialization, (de)activation, and {de,con}struction.
 */
template <typename FUNC_TYPE>
class CallbackCore : public tarp::Callback {
public:
    void set_func(FUNC_TYPE f);
    void assert_func_set(void) const;

    virtual int activate(void)         override;
    virtual void deactivate(void)      override;
    void die(void)                     override;
    bool is_active(void) const         override;
    bool is_dead(void) const           override;
    size_t get_id(void) const          override;
    void set_id(size_t id)             override;

protected:
    CallbackCore(
            std::shared_ptr<tarp::EventPump> evp,
            FUNC_TYPE cb);

    ~CallbackCore(void);

    /* hooks specific to each callback type; to be implemented by concrete
     * subclasses */
    virtual void initialize_raw_event_handle(void)    = 0;
    virtual void destroy_raw_event_handle(void)       = 0;
    virtual int register_event_monitor(struct evp_handle *raw_evp_handle)   = 0;
    virtual int deregister_event_monitor(struct evp_handle *raw_evp_handle) = 0;

    FUNC_TYPE m_func;

private:
    std::weak_ptr<tarp::EventPump> m_evp;
    bool m_isactive;
    bool m_isdead;
    size_t m_id;
};

/*
 * If the callback is left unspecified at instantiation time, it
 * *must* subsequently be set via set_func(). Attempting to call
 * activate() without having done this will throw an exception.
 *
 * NOTE the base duration used is microseconds; milliseconds, seconds,
 * or indeed any other unit multiple of it is directly convertible
 * without use of duration_cast; with this said, the user should
 * still excercise caution even when the conversion is accepted
 * implicitly (e.g. seconds to microseconds). If working at the far
 * end of the unit's range, the conversion can overflow the target
 * duration (e.g. chrono::seconds::max to chrono::microseconds).
 * Of course, the duration here is meant to store short, relative
 * intervals so under normal use this is a non-issue.
 */
class TimerEventCallback : public CallbackCore<tarp::timer_callback> {
public:
    TimerEventCallback(
            std::shared_ptr<tarp::EventPump> evp,
            std::chrono::microseconds interval,
            tarp::timer_callback cb=nullptr);

    ~TimerEventCallback(void);
    void set_interval(std::chrono::microseconds interval);
    void call(void) override;

private:
    void initialize_raw_event_handle(void) override;
    void destroy_raw_event_handle(void) override;
    int register_event_monitor(struct evp_handle *raw_evp_handle) override;
    int deregister_event_monitor(struct evp_handle *raw_evp_handle) override;

    struct timer_event *m_raw_tev_handle;
    struct timespec m_interval;
};


class FdEventCallback : public CallbackCore<tarp::fd_callback> {
public:
    FdEventCallback(
            std::shared_ptr<tarp::EventPump> evp,
            int fd, uint32_t flags,
            tarp::fd_callback cb);

    ~FdEventCallback(void);
    void call(void) override;

private:
    void initialize_raw_event_handle(void) override;
    void destroy_raw_event_handle(void) override;
    int register_event_monitor(struct evp_handle *raw_evp_handle) override;
    int deregister_event_monitor(struct evp_handle *raw_evp_handle) override;

    struct fd_event *m_raw_fdev_handle;
    int m_fd;
    uint32_t m_flags;
};

class UserEventCallback : public CallbackCore<tarp::uev_callback> {
public:
    UserEventCallback(
            std::shared_ptr<tarp::EventPump> evp,
            unsigned event_type,
            tarp::uev_callback cb);

    ~UserEventCallback(void);

    void call(void) override;
    void call(void *data);

private:
    void initialize_raw_event_handle(void) override;
    void destroy_raw_event_handle(void) override;
    int register_event_monitor(struct evp_handle *raw_evp_handle) override;
    int deregister_event_monitor(struct evp_handle *raw_evp_handle) override;

    struct user_event_watch *m_raw_uev_handle;
    unsigned m_event_type;
    void *m_event_data;
};


/*
 * This is to limit the coupling between Callbacks and
 * the EventPump. Specifically, they are friends of each other
 * so as to gain access to private/protected data, but this is
 * only through a very restricted interface such as this one. */
class RawEventPumpInterface {
    friend class Callback;
private:
    virtual struct evp_handle *get_raw_evp_handle(void) = 0;
    virtual void track_callback(std::shared_ptr<tarp::Callback> cb) = 0;
    virtual void remove_callback_if_tracked(size_t id) = 0;
};

/*
 * Public instantiable EventPump; *not* to be used through its
 * RawEventPumpInterface -- which is only available to its Callback
 * friend for limited acess to protected/private state. */
class EventPump final :
    public RawEventPumpInterface,
    public std::enable_shared_from_this<tarp::EventPump>
{
public:
    EventPump(void);
    ~EventPump(void);

    // not copyable/clonable or movable.
    EventPump(const EventPump &)               = delete;
    EventPump(const EventPump &&)              = delete;
    EventPump &operator=(const EventPump &)    = delete;
    EventPump &operator=(EventPump &&)         = delete;

    void run(void);
    int set_fd_callback(int fd, uint32_t flags, tarp::fd_callback cb);
    int set_uev_callback(unsigned event_type, tarp::uev_callback cb);
    int set_timer_callback(std::chrono::microseconds interval,
            tarp::timer_callback cb);

    int push_event(unsigned event_type, void *data=nullptr);

private:
    struct evp_handle *get_raw_evp_handle(void) override;
    void track_callback(std::shared_ptr<tarp::Callback> cb) override;
    void remove_callback_if_tracked(size_t id) override;
    int activate_and_track(std::shared_ptr<tarp::Callback> callback);

    struct evp_handle *m_raw_state;
    size_t m_callback_id;
    std::unordered_map<size_t, std::shared_ptr<tarp::Callback>> m_callbacks;
};


#include "impl/event.txx"

}; /* namespace tarp */

#endif
