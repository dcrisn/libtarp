#ifndef TARP_EVENT_HXX
#define TARP_EVENT_HXX

/*
 * C++ wrapper for tarp/event.h
 *
 * See tarp/event.h FMI.
 *
 *
 * General API notes
 * ==================
 *
 *
 * EventPump and Callback objects
 * -------------------------------
 *
 * Callback objects can only be constructed through the EventPump interface
 * (see EventPump class below). Their constructors are semantically private.
 * This is enforced by each constructor requiring a special token that cannot
 * be created by public clients (see 'construction permits' below).
 * The same is true for the EventPump itself, which should and may only be
 * created through make_event_pump().
 *
 * Callback creation comes in two flavors:
 *
 *  - implicit: a lambda is bound to a callback object. The callback object
 *  is not accessible to or meant to be accessed by the user and is only
 *  tracked internally by the eventPump. The 'cb' argument to the EventPump
 *  must be non-NULL. The EventPump immediately implicitly activates the
 *  callback.
 *
 *  - explicit: a lambda is bound to a callback object, but the object is
 *  returned as a shared pointer. The user is responsible for binding the
 *  lambda to the callback (see the set_func method) and activating it.
 *  It's illegal to activate a callback without having bound a lambda first.
 *  Since in this case a concrete callback object is returned to and is
 *  available to the user, they may call methods specific to each type of
 *  callback (e.g. set_interval for TimerEventCallbacks), activate and
 *  de-activate the callback as needed, etc.
 *  The callback object can be captured inside the lambda by copying
 *  the shared pointer.
 *
 *
 *  ### Lambda return value
 *
 *  In both the implicit and explicit Callback cases the lambda must return
 *  boolean true to continue as normal and boolean false to unregister and
 *  destroy the callback. NOTE that internal state is destroyed when false is
 *  returned but the actual Callback object destructor gets invoked at that
 *  moment only if the user does not maintain other references either to the
 *  lambda or the Callback object that would keep them alive. Otherwise the
 *  destructor of the Callback object only gets called when these 'anchors' go
 *  out of scope as well. Regardless of when the destructor is eventually
 *  called, once false is returned, the Callback object is 'dead' and cannot
 *  be used any more. Reinitialization is also not possible: once dead, a new
 *  callback must be created.
 *
 * ### EventPump destruction
 *
 * A callback object is associated with the EventPump that was used to create
 * it. In particular, NOTE the following:
 * - a Callback object cannot be moved between EventPumps. New Callbacks must
 *   be created if that is necessary.
 * - When the EventPump is destructed, its associated callbacks are unusable.
 *   In fact, the EventPump destructor renders all callbacks 'dead'. Whether
 *   or not the Callback object gets freed at that instant depends on whether
 *   the user has other references that are keeping the object alive,
 *   preventing its destruction (see 'Lambda return value' above).
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *
 * Construction permits
 * ---------------------
 * Constructors of classes derived from tarp::Callback are semantically private.
 * There are 2 issues however with explicitly declaring them 'private':
 * 1) it makes it impossible to use std::make_shared without hacks;
 * 2) the EventPump would need to either be a friend of the respective
 *    concrete derived class, breaking its encapsulation, OR a friend
 *    factory must be created as a proxy (which, while better, leads to
 *    a bit of duplication/redundancy).
 * A solution to both is for the constructors to stay public but expect
 * as a parameter a key/token object that can only be instantiated by the
 * EventPump. A similar approach is taken so that the EventPump itself can
 * only be created through the make_event_pump function.
 * Look for 'construction_permit' below fmi.
 */

#include <functional>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <unordered_map>

#include <tarp/error.h>
#include <tarp/log.h>
#include <tarp/process.h>

#include "event_flags.h"

extern "C" {
    struct evp_handle;
    struct timer_event;
    struct fd_event;
    struct user_event_watch;
}

namespace tarp {
    using timer_callback = std::function<bool(void)>;
    using fd_callback    = std::function<bool(int fd, uint32_t events)>;
    using uev_callback   = std::function<bool(unsigned event_type, void *data)>;


class EventPump;
class RawEventPumpInterface;
class Process;


/*
 * This class sets the interface for concrete callback objects;
 *
 * subclasses must implement this, though they can expand on it by
 * adding their own methods.
 *
 * NOTE this sets both the public interface (usable by the user and other,
 * uncoupled, functions/classes) and the private interface accessible to
 * friends (e.g. the EventPump). Notice that almost all internal state is
 * encapsulated instead in the CallbackCore subclass for this exact reason.
 *
 * (1) activate/deactivate are used to (un)register a callback without
 * destroying it. This can be used to dynamically pause callbacks in a list
 * without destroying or creating new ones all the time. To gain
 * access to this interface, 'explicit' Callback objects must be created
 * through the EventPump API (see the EventPump class fmi).
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
    /* Copy/Move semantics undefined for Callbacks. Implicitly delete
     * these constructors and assignment operators for all derived
     * classes. */
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
    /* See 'construction permits' notes at the top of the file */
    class construction_permit{};

    virtual void set_id(size_t id)            = 0;
    virtual size_t get_id(void) const         = 0;
    virtual void die(void)                    = 0;   // (2)

    struct evp_handle *get_raw_evp_handle(std::shared_ptr<tarp::EventPump> evp);
    void untrack_callback(std::shared_ptr<tarp::EventPump> evp) const;
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
 * intervals so under expected normal use this is a non-issue.
 */
class TimerEventCallback : public CallbackCore<tarp::timer_callback> {
public:
    TimerEventCallback(
            const Callback::construction_permit &permit,
            std::shared_ptr<tarp::EventPump> evp,
            std::chrono::microseconds interval,
            tarp::timer_callback cb=nullptr);

    TimerEventCallback(void) = delete;
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
            const tarp::Callback::construction_permit &permit,
            std::shared_ptr<tarp::EventPump> evp,
            int fd, uint32_t flags,
            tarp::fd_callback cb);

    FdEventCallback(void) = delete;
    ~FdEventCallback(void);
    void call(void) override;
    void call(uint32_t events);

private:
    void initialize_raw_event_handle(void) override;
    void destroy_raw_event_handle(void) override;
    int register_event_monitor(struct evp_handle *raw_evp_handle) override;
    int deregister_event_monitor(struct evp_handle *raw_evp_handle) override;

    struct fd_event *m_raw_fdev_handle;
    int m_fd;
    uint32_t m_flags;
    uint32_t m_revents;
};

class UserEventCallback : public CallbackCore<tarp::uev_callback> {
public:
   UserEventCallback(
           const tarp::Callback::construction_permit &permit,
           std::shared_ptr<tarp::EventPump> evp,
           unsigned event_type,
           tarp::uev_callback cb);

    UserEventCallback(void) = delete;
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
 * Private EventPump interface accessible to the Callback interface.
 *
 * This is to limit the coupling between Callbacks and the
 * EventPump and restrict private access to very specific operations.
 */
class RawEventPumpInterface {
    friend class Callback;
    friend class Process;
private:
    virtual struct evp_handle *get_raw_evp_handle(void) = 0;
    virtual void untrack_callback(size_t id) = 0;
};

/*
 * Ensure EventPumps are only created as std::shared_ptr's through
 * std::make_shared. */
std::shared_ptr<tarp::EventPump> make_event_pump(void);

/*
 * Event pump for registering timer, file descriptor, and user-defined
 * event-based callbacks.
 *
 * (1) The event pump also supports spawning processes (see proces.h fmi)
 * asynchronously. Much like callbacks, a process is also bound to its
 * event pump and cannot be moved around (create a new process instead
 * as needed).
 *
 * The EventPump is only constructible through make_event_pump.
 */
class EventPump final :
    public RawEventPumpInterface,
    public std::enable_shared_from_this<tarp::EventPump>
{
public:
    EventPump(void) = delete;
    ~EventPump(void);

    // not copyable/clonable or movable.
    EventPump(const EventPump &)               = delete;
    EventPump(const EventPump &&)              = delete;
    EventPump &operator=(const EventPump &)    = delete;
    EventPump &operator=(EventPump &&)         = delete;

    class construction_permit {
    private:
        friend std::shared_ptr<tarp::EventPump> make_event_pump(void);
        construction_permit(void) { };
    };

    EventPump(const EventPump::construction_permit &permit);

    void run(int seconds = -1);
    int push_event(unsigned event_type, void *data=nullptr);

    int set_fd_event_callback(int fd, uint32_t flags, tarp::fd_callback cb);
    int set_user_event_callback(unsigned event_type, tarp::uev_callback cb);
    int set_timer_callback(std::chrono::microseconds interval,
            tarp::timer_callback cb);

    std::shared_ptr<tarp::TimerEventCallback> make_explicit_timer_callback(
            std::chrono::microseconds interval);

    std::shared_ptr<tarp::FdEventCallback> make_explicit_fd_event_callback(
            int fd, uint32_t flags);

    std::shared_ptr<tarp::UserEventCallback> make_explicit_user_event_callback(
            unsigned event_type);

    /* (1) */
    std::shared_ptr<tarp::Process> make_process(
            std::initializer_list<std::string> cmd_spec,
            int ms_timeout = -1,
            int instream  = STREAM_ACTION_PASS,
            int outstream = STREAM_ACTION_PASS,
            int errstream = STREAM_ACTION_PASS,
            Process::ioevent_cb ioevent_callback       = nullptr,
            Process::completion_cb completion_callback = nullptr
            );

private:
    struct evp_handle *get_raw_evp_handle(void) override;
    void untrack_callback(size_t id) override;
    void track_callback(std::shared_ptr<tarp::Callback> cb);
    int activate_and_track(std::shared_ptr<tarp::Callback> callback);

    struct evp_handle *m_raw_state;
    size_t m_callback_id;
    std::unordered_map<size_t, std::shared_ptr<tarp::Callback>> m_callbacks;
    tarp::Callback::construction_permit m_callback_construction_permit;
};



#include "impl/event.txx"

}; /* namespace tarp */

#endif
