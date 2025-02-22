#pragma once

#include <chrono>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <tarp/cxxcommon.hxx>
#include <tarp/semaphore.hxx>
#include <tarp/type_traits.hxx>

//

namespace tarp {
namespace evchan {
//

#ifdef REAL
#error "#define REAL would overwrite existing definition"
#endif

#define REAL       static_cast<C *>(this)
#define CONST_REAL static_cast<const C *>(this)

//

using binary_semaphore_t = tarp::binary_semaphore;

//

// Enumeration of the states of a channel from the POV of a monitor.
// The state values are such that they can be easily stored in a bit field.
enum chanState {
    CLOSED = 1 << 0,
    READABLE = 1 << 1,
    WRITABLE = 1 << 2,
};

//

namespace interfaces {

// Interface for a notifier.
//
// A notifier is a class that implements a notification mechanism. Some examples
// would be:
// - writing to a pipe (self-pipe trick) or an eventfd e.g. to unblock epoll
// - signaling a condition variable or posting a semaphore to unblock a waiting
//    thread
// - setting a flag
// - any other custom action.
//
// The arguments can be used as event flags or masks, ids etc or whatever makes
// sense for the respective notification mechanism. I.e. they can store any
// information that can be encoded into 64 bits.
//
// The return value of the notifier should be False if the notifier should be
// removed from the list of monitors after the current call, and True otherwise
// if it should remain in the list.
class notifier {
public:
    virtual ~notifier() = default;
    virtual bool notify(std::uint32_t, std::uint32_t) = 0;
};

//

// Interface for a write-only event channel.
// CRTP is used for static dispatch.
// Refer to the corresponding event_channel member functions
// being forwarded to fmi.
template<typename C, typename... types>
class wchan {
    using payload_t = tarp::type_traits::type_or_tuple_t<types...>;

public:
    // prevent object slicing!
    DISALLOW_COPY_AND_MOVE(wchan);
    wchan() = default;
    virtual ~wchan() = default;

    // The monitor can only be added for state=WRITABLE (and implicitly
    // state=CLOSED).
    std::uint32_t add_monitor(std::shared_ptr<notifier> notifier,
                              std::uint32_t states = chanState::WRITABLE) {
        if (!(states & chanState::WRITABLE)) {
            throw std::invalid_argument(
              "unacceptable state specified for monitoring");
        }
        return REAL->add_monitor(notifier, chanState::WRITABLE);
    }

    auto closed() const { return CONST_REAL->closed(); }

    bool empty() const { return REAL->empty(); }

    std::size_t size() const { return REAL->size(); }

    auto operator<<(const payload_t &event) { return REAL->operator<<(event); }

    auto operator<<(payload_t &&event) {
        return REAL->operator<<(std::move(event));
    }

    template<typename... T>
    std::pair<bool, std::optional<payload_t>> try_push(T &&...data) {
        return REAL->try_push(std::forward<T>(data)...);
    }
};

//

// Interface for a read-only event channel.
// CRTP is used for static dispatch.
// Refer to the corresponding event_channel member functions
// being forwarded to fmi.
template<typename C, typename... types>
class rchan {
    using payload_t = tarp::type_traits::type_or_tuple_t<types...>;

public:
    // prevent object slicing!
    DISALLOW_COPY_AND_MOVE(rchan);

    rchan() = default;
    virtual ~rchan() = default;

    // The monitor can only be added for state=READABLE (and implicitly
    // state=CLOSED).
    std::uint32_t add_monitor(std::shared_ptr<notifier> notifier,
                              std::uint32_t states = chanState::READABLE) {
        if (!(states & chanState::READABLE)) {
            throw std::invalid_argument(
              "unacceptable state specified for monitoring");
        }
        return REAL->add_monitor(notifier, chanState::READABLE);
    }

    auto closed() const { return CONST_REAL->closed(); }

    bool empty() const { return REAL->empty(); }

    std::size_t size() const { return REAL->size(); }

    std::optional<payload_t> try_get() { return REAL->try_get(); }

    std::deque<payload_t> get_all() { return REAL->get_all(); }

    auto &operator>>(std::optional<payload_t> &event) {
        REAL->operator>>(event);
        return *this;
    }
};

//

// Interface for a write-only *unbuffered* event channel ('trunk').
// CRTP is used for static dispatch.
// Refer to the corresponding `class trunk` member functions being
// forwarded to fmi.
template<typename C, typename... types>
class wtrunk {
    using payload_t = tarp::type_traits::type_or_tuple_t<types...>;
    using Ts = std::tuple<types...>;

public:
    // prevent object slicing!
    DISALLOW_COPY_AND_MOVE(wtrunk);

    wtrunk() = default;
    virtual ~wtrunk() = default;

    // The monitor can only be added for state=WRITABLE (and implicitly
    // state=CLOSED).
    std::uint32_t add_monitor(std::shared_ptr<notifier> notifier,
                              std::uint32_t states = chanState::WRITABLE) {
        if (!(states & chanState::WRITABLE)) {
            throw std::invalid_argument(
              "unacceptable state specified for monitoring");
        }
        return REAL->add_monitor(chanState::WRITABLE, notifier);
    }

    auto closed() const { return CONST_REAL->closed(); }

    template<typename... T>
    auto push(T &&...data) {
        return REAL->push(std::forward<T>(data)...);
    }

    template<typename... T>
    auto try_push(T &&...data) {
        return REAL->try_push(std::forward<T>(data)...);
    }

    template<typename timepoint, typename... T>
    auto try_push_until(const timepoint &abs_time, T &&...data) {
        return REAL->try_push_until(abs_time, std::forward<T>(data)...);
    }

    template<class Rep, class Period, typename... T>
    auto try_push_for(const std::chrono::duration<Rep, Period> &rel_time,
                      T &&...data) {
        return REAL->try_push_for(rel_time, std::forward<T>(data)...);
    }

    auto operator<<(const payload_t &event) { return REAL->operator<<(event); }

    auto operator<<(payload_t &&event) {
        return REAL->operator<<(std::move(event));
    }
};

//

// Interface for a read-only *unbuffered* event channel ('trunk').
// CRTP is used for static dispatch.
// Refer to the corresponding `class trunk` member functions being
// forwarded to fmi.
template<typename C, typename... types>
class rtrunk {
    using payload_t = tarp::type_traits::type_or_tuple_t<types...>;
    using Ts = std::tuple<types...>;

public:
    // prevent object slicing!
    DISALLOW_COPY_AND_MOVE(rtrunk);
    rtrunk() = default;
    virtual ~rtrunk() = default;

    std::uint32_t add_monitor(std::shared_ptr<notifier> notifier,
                              std::uint32_t states = chanState::READABLE) {
        if (!(states & chanState::READABLE)) {
            throw std::invalid_argument(
              "unacceptable state specified for monitoring");
        }
        return REAL->add_monitor(chanState::READABLE, notifier);
    }

    auto closed() const { return CONST_REAL->closed(); }

    auto get() { return REAL->get(); }

    auto try_get() { return REAL->try_get(); }

    template<typename timepoint>
    auto try_get_until(const timepoint &abs_time) {
        return REAL->try_get_until(abs_time);
    }

    template<class Rep, class Period>
    auto try_get_for(const std::chrono::duration<Rep, Period> &rel_time) {
        return REAL->try_get_for(rel_time);
    }

    auto &operator>>(std::optional<payload_t> &event) {
        REAL->operator>>(event);
        return *this;
    }
};

//

// Restricted interface for an event stream.
//
// A single-producer-multi-consumer (SPMC) or multi-producer-single-consumer
// (MPSC) event channel wrapper interface.
//
// Unlike the event_broadcaster, which multiple channels can be attached to,
// forming a broadcast group, the event_stream consists of a singular channel
// that carries a stream of events, hence the name of the class.
// The stream's directionality is given by that of the underlying channel:
// an rstream uses an rchan, and a wstream uses a wchan.
//
// Unlike the event_broadcaster, since events are always enqueued to a single
// channel (the stream channel) and only dequeued from this one channel once,
// events need not be copiable: move-only objects such as std::unique_ptr
// can be enqueued.
//
// In the most basic scenario, the stream joins a single producer to a single
// consumer. The object exposing the stream fundamentally sets the producer and
// consumer roles based on whether the stream is an r- or a w- stream,
// respectively. The object that exposes the stream must by definition be
// 'well-known' so interested consumers or producers (depending on the direction
// of the stream, as mentioned) must themselves connect to the stream by
// obtaining a shared_ptr to its (r/w) channel.
//
// The object exposing the stream decides on the type of the stream depending
// on the role it wants or needs to play: it uses an rstream if it wants to be
// a producer, and it uses a wstream if it wants to be a consumer. That is, the
// 'r' and 'w' directional prefixes are from the POV of _clients_ of this object
// (i.e. other objects wanting to connect to the stream).
// This means this side of the stream is always fixed to 1 participant
// (1 consumer if a wstream, 1 producer if an rstream). The other side, however
// need not be fixed to one. Any number of producers (for a wstream) or
// consumers (for an rstream) can connect to the stream.
// * * *
// HOWEVER, NOTE: whereas in the case of the event_broadcaster the SPMC
// topology entails broadcast semantics (an event posted by the producer is
// broadcast to all the consumers), here it has **anycast** semantics:
// an event posted by the producer is dequeued by a single one (any one)
// of an arbitrary number of consumers. This is since there is a single channel
// and since the data item could be move-only (e.g. a std::unique_ptr).
// A suitable use case would be delivering tasks to a thread pool where it
// is acceptable for any one of a number of workers to get whichever task.
//   *
// NOTE: CRTP is used for interface inheritance to enable static dispatch.
template<typename T>
class event_stream {
public:
    DISALLOW_COPY_AND_MOVE(event_stream);
    event_stream() = default;
    virtual ~event_stream() = default;

    auto channel() { return static_cast<T *>(this)->channel(); }
};


}  // namespace interfaces

#undef REAL

//

namespace impl {
using namespace interfaces;

struct monitor_entry {
    monitor_entry(std::shared_ptr<notifier> notifier) : notif(notifier) {}

    std::shared_ptr<notifier> notif;
};

// useful bitmask constants.
static constexpr std::uint8_t APPLY = 1;
static constexpr std::uint8_t CLEAR = 0;

//

// An event channel is a homogenous queue that stores data items of a
// specified type, suitable for producer-consumer setups.
//
// A channel can be enqueued into and dequeued from. Some other basic
// operations are provided.
//
// A channel can be monitored for readability and writability, so that it's
// easily integrated with typical IO-multiplexing setups e.g. epoll.
//
// Channel directionality (send/write or receive/read) is achieved/enforced
// by casting the event_channel to an rchan (read channel) or wchan
// (write-channel). Note the event_channel inherits from both of those
// two interfaces using CRTP. CRTP makes it possible to use static dispatch
// thus avoiding the overhead of dynamic dispatch and dynamic binding.
// And iheriting these interfaces enables implicit conversion from an
// event_channel to either interface (upcasting does not need an explicit
// cast).
//
// NOTE: producer-consumer and/or pub-sub etc functionality is made convenient
// by higher-level classes and by using the rchan & wchan interfaces.
//
// === Channel Capacity ===
// -------------------------
// A channel has a specified (queue) length. Reads (see try_get()) are possible
// only when the channel is non-empty. Writes (see try_push()) are normally
// possible only when the channel is not full. Otherwise both reads and writes
// fail, and it is then up to the caller how to proceed: discard the data,
// or try again later. Notice only try_(get,push) functions are provided.
// This makes clear the non-blocking nature of an event_channel. If _blocking_
// behavior is required, there are various options: 1) use a `class trunk`
// instead; 2) use a semaphore and specify a notifier that increments it;
// 3) use a monitor.
//
// The thought process behind designing the event channels to be non-blocking
// is that blocking behavior is almost never needed, since an event channel
// is mostly meant to be used as part of the higher level constructs of
// event_{broadcaster, stream, aggregator}, where blocking behavior is
// undesirable. This also simplifies the implementation and makes it more
// efficient (otherwise trunk and event_channel would be a single class, which
// would be less duplicated, but also slower, messier, and less maintainable).
// And as mentioned earlier, there are various ways blocking behavior can be
// easily achieved otherwise.
//
// In fact, very frequently not only do we want an event_channel that does not
// block, but we want one where writes never fail. That is, ones where events
// can be lossy, and more recent events can overwrite older ones. To this end,
// the event channel can be told to use ring-buffer semantics. In this case,
// a write always succeeds (provided that the channel is not **closed**).
// When the channel is filled to capacity, a write will cause the oldest
// buffered item to be discarded to make room. When used this way, the capacity
// of the channel should be the number of last events you are interested in. For
// example, if only the most recent event is important, the proper capacity for
// the channel is exactly 1.
//
// === Thread Safety ===
// ----------------------
// The ts_policy template argument is used to compile in/out mutex-locking
// calls in order to add or remove thread-safety. For example, in a single
// threaded event-driven program mutex protection is not needed and it would
// incur unnecessary overhead.
//
template<typename ts_policy, typename... types>
class event_channel
    : public wchan<event_channel<ts_policy, types...>, types...>
    , public rchan<event_channel<ts_policy, types...>, types...>
    , public std::enable_shared_from_this<event_channel<ts_policy, types...>> {
    //
    using is_tuple =
      typename tarp::type_traits::type_or_tuple<types...>::is_tuple;
    using this_type = event_channel<ts_policy, types...>;
    using lock_t = std::unique_lock<std::mutex>;
    using mutex_t = std::mutex;
    using payload_type = tarp::type_traits::type_or_tuple_t<types...>;

    // Structure that represents a monitor to be notified of changes in the
    // channel state.
    struct monitor_entry {
        monitor_entry(std::shared_ptr<notifier> notifier) : notif(notifier) {}

        std::shared_ptr<notifier> notif;
    };

public:
    using payload_t = payload_type;
    using Ts = std::tuple<types...>;
    using wchan_t = interfaces::wchan<this_type, types...>;
    using rchan_t = interfaces::rchan<this_type, types...>;

    DISALLOW_COPY_AND_MOVE(event_channel);

    // Make a buffered channel.
    // When the channel is filled to its max capacity, the behavior is as
    // follows:
    //  - if circular=true, each new write discards the oldest item
    //  in the channel. IOW, the channel uses ring-buffer semantics. Writes
    //  in this case never fail.
    //  - if circular=false, writes fail until until reads make room.
    //
    // Writes fail when circular=false and the channel has been filled to
    // capacity. Reads fail when the channel is empty.
    event_channel(std::uint32_t channel_capacity, bool circular)
        : m_circular(circular), m_channel_capacity(channel_capacity) {
        if (m_channel_capacity == 0) {
            auto errmsg = "nonsensical max capacity of 0 for buffered channel";
            throw std::logic_error(errmsg);
        }
    }

    // return a wchan interface reference.
    interfaces::wchan<this_type, types...> &as_wchan() { return *this; }

    // return a wchan interface shared ptr. NOTE: this may only be called
    // if the event_channel has been constructed as a std::shared_ptr.
    std::shared_ptr<interfaces::wchan<this_type, types...>>
    as_wchan_sharedptr() {
        return this->shared_from_this();
    }

    // return an rchan interface reference.
    interfaces::rchan<this_type, types...> &as_rchan() { return *this; }

    // return a wchan interface shared ptr. NOTE: this may only be called
    // if the event_channel has been constructed as a std::shared_ptr.
    std::shared_ptr<interfaces::rchan<this_type, types...>>
    as_rchan_sharedptr() {
        return this->shared_from_this();
    }

    // Add a monitor for the states specified in states. Note notifications
    // have edge-triggered semantics and are only emitted on the rising and
    // falling edge, respectively, of a state change. I.e. notifier.notify()
    // only gets called when the channel _becomes_ readable/writable.
    // See trunk::add_monitor fmi.
    std::uint32_t add_monitor(std::shared_ptr<notifier> notifier,
                              std::uint32_t states) {
        struct monitor_entry mon(notifier);

        lock_t l {m_mtx};

        if (states & chanState::READABLE) {
            m_recv_monitors.push_back(mon);
        }

        if (states & chanState::WRITABLE) {
            m_send_monitors.push_back(mon);
        }

        return m_state_mask;
    }

    // Close a channel.
    //
    // A closed channel cannot be reopened.
    // No read/write operations are possible on a closed channel.
    // All buffered events are discarded.
    // All monitors are woken.
    void close() {
        std::vector<std::shared_ptr<notifier>> monitors;

        {
            lock_t l {m_mtx};
            m_closed = true;
            m_state_mask |= chanState::CLOSED;
            m_msgs.clear();

            // gather all monitors and clear the monitor queues.
            auto get_all_and_clear = [this, &monitors](auto &ls) {
                for (auto &i : ls) {
                    monitors.push_back(i.notif);
                }
                ls.clear();
            };
            get_all_and_clear(m_recv_monitors);
            get_all_and_clear(m_send_monitors);
        }

        // notify all monitors
        for (auto &mon : monitors) {
            mon->notify(chanState::CLOSED, APPLY);
        }
    }

    // True if channel is closed, else False.
    bool closed() const {
        lock_t l {m_mtx};
        return m_closed;
    }

    // True if there are no queued items.
    bool empty() const {
        lock_t l {m_mtx};
        return m_msgs.empty();
    }

    // Return the number of events currently enqueued.
    std::size_t size() const {
        lock_t l {m_mtx};
        return m_msgs.size();
    }

    // Discard all events currently enqueued.
    void clear() {
        lock_t l {m_mtx};
        m_msgs.clear();
        refresh_channel_state(l);
    }

    // Try to push a new event item. The push is only made if possible to be
    // carried through immediately; that is, either 1) the channel is not yet
    // filled to capacity, or 2) the channel has ring-buffer semantics, in which
    // case writes never fail but event storage is lossy, since when the buffer
    // is full the most recent event causes the oldest one to be discarded to
    // make room.
    //
    // This always fails if the channel is closed.
    //
    // If successful, return {true, nullopt}.
    // Otherwise return {false, std::optional<data>}, whereby the data is not
    // lost but returned to the caller.
    template<typename... T>
    std::pair<bool, std::optional<payload_type>> try_push(T &&...data) {
        lock_t l {m_mtx};

        // [[unlikely]] (c++20).
        if (m_closed) {
            // std::cerr << "Failed try_push --> closed\n";
            return {false, opt_payload(std::forward<T>(data)...)};
        }

        // there is room.
        if ((m_msgs.size() < m_channel_capacity)) {
            store(l, std::forward<T>(data)...);
            refresh_channel_state(l);
            return {true, std::nullopt};
        }

        // ~~ else, full buffer.

        // if using ring-buffer semantics, a write always succeeds. Buffer
        // room is always made by dropping the oldest entry.
        if (m_circular) {
            m_msgs.pop_front();
            store(l, std::forward<T>(data)...);
            if (m_msgs.size() > m_channel_capacity) {
                throw std::logic_error("BUG: overfilled circular channel");
            }
            refresh_channel_state(l);
            return {true, std::nullopt};
        }

        // full, and no ring-buffer semantics => failed push.
        // NOTE: in this case, the state does not change in any way: no need
        // to call refresh_channel_state().
        auto ret = opt_payload(std::move(m_msgs.back()));
        return {false, std::move(ret)};
    }

    // Return the oldest buffered item from the channel.
    // This function never blocks. It returns nullopt if the event channel
    // is empty. NOTE: payload_t is either a single type for a single-item
    // event, or a tuple of types for a multi-item event. Refer to
    // the type_or_tuple helper type trait.
    std::optional<payload_t> try_get() {
        lock_t l {m_mtx};

        // No message to get; state does not change, no need to call
        // refresh_channel_state()
        if (m_msgs.empty() or m_closed) {
            return std::nullopt;
        }

        auto ret = opt_payload(std::move(m_msgs.front()));
        m_msgs.pop_front();
        refresh_channel_state(l);
        return ret;
    }

    // Return the entire channel event buffer. Note this will be empty if there
    // are no events.
    std::deque<payload_t> get_all() {
        lock_t l {m_mtx};
        decltype(m_msgs) events;
        std::swap(events, m_msgs);
        refresh_channel_state(l);
        return events;
    }

    // More convenient and expressive overloads for enqueuing
    // and dequeueing an element. See the comments in `class trunk` fmi.
    auto operator<<(payload_t &data) {
        // return *this;
        return try_push(std::move(data));
    }

    auto operator<<(payload_t &&data) {
        // return *this;
        return try_push(std::move(data));
    }

    auto &operator>>(std::optional<payload_t> &event) {
        event.reset(); /* defensive programming here */
        auto res = try_get();
        if (res.has_value()) {
            event.emplace(std::move(res.value()));
        }
        return *this;
    }

private:
    // Consider the current state of the channel and update m_state_mask
    // based on that and send out any new state change notifications as
    // appropriate.
    void refresh_channel_state(lock_t &) {
        // assume not writable and not readable.
        std::uint32_t current_state = 0;

        // writable if we have buffer space OR the channel uses ring-buffer
        // semantics (in which case it is always writable).
        if (m_circular or (m_msgs.size() < m_channel_capacity)) {
            current_state |= chanState::WRITABLE;
        }

        // readable if there is anything in the buffer.
        if (!m_msgs.empty()) {
            current_state |= chanState::READABLE;
        }

        auto notify_monitors = [this](auto &ls, auto state_flags, auto action) {
            for (auto it = ls.begin(); it != ls.end();) {
                auto &notifier = it->notif;

                // NOTE: notifier->notify() **must not** call us back, else we
                // will get a deadlock.
                bool must_remove = !notifier->notify(state_flags, action);

                // std::cerr << "called notify() with state=" <<
                // static_cast<unsigned>(state) << " and mask=" <<
                // static_cast<unsigned>(mask) << ", got " << must_remove <<
                // std::endl;

                if (must_remove) {
                    it = ls.erase(it);
                    continue;
                }
                ++it;
            }
        };

        // ====== generate state change notifications. These are edge-triggered
        // notifications. No notification is generated if the state has not
        // changed.

        // no changes
        if (current_state == m_state_mask) {
            return;
        }

        using S = chanState;

        // the channel has _become_ READABLE/WRITABLE => rising edge
        if ((current_state & S::READABLE) && !(m_state_mask & S::READABLE)) {
            notify_monitors(m_recv_monitors, S::READABLE, APPLY);
        }
        if ((current_state & S::WRITABLE) && !(m_state_mask & S::WRITABLE)) {
            notify_monitors(m_send_monitors, S::WRITABLE, APPLY);
        }

        // the channel has become NOT READABLE/WRITABLE => falling edge
        if ((m_state_mask & S::READABLE) && !(current_state & S::READABLE)) {
            notify_monitors(m_recv_monitors, S::READABLE, CLEAR);
        }
        if ((m_state_mask & S::WRITABLE) && !(current_state & S::WRITABLE)) {
            notify_monitors(m_send_monitors, S::WRITABLE, CLEAR);
        }

        m_state_mask = current_state;
        if (m_closed) {
            m_state_mask |= chanState::CLOSED;
        }
    }

    // Return a std::optional<payload> that properly stores data according to
    // its type_or_tuple semantics.
    template<typename... T>
    constexpr auto opt_payload(T &&...data) {
        std::optional<payload_type> opt;
        if constexpr (tarp::type_traits::is_tuple_v<T...>) {
            opt.emplace(std::make_tuple(std::forward<T>(data)...));
        } else {
            opt.emplace(std::forward<T>(data)...);
        }
        return opt;
    }

    // Append data as a new event to the channel buffer.
    template<typename... T>
    void store(lock_t &, T &&...data) {
        if constexpr (tarp::type_traits::is_tuple_v<T...>) {
            m_msgs.emplace_back(std::make_tuple(std::forward<T>(data)...));
        } else {
            m_msgs.emplace_back(std::forward<T>(data)...);
        }
    }

private:
    const bool m_circular = false;
    const std::uint32_t m_channel_capacity = 0;

    using CLOCK = std::chrono::steady_clock;
    const CLOCK::time_point m_past_tp {CLOCK::now()};

    mutable mutex_t m_mtx;
    bool m_closed {false};
    std::deque<payload_t> m_msgs;

    // send and receive monitor queues.
    std::list<struct monitor_entry> m_send_monitors;
    std::list<struct monitor_entry> m_recv_monitors;

    // an event can happen before the addition of any monitor;
    // we need to track these so we can signal the true state of the channel
    // when a monitor joins. NOTE: A channel starts off empty but with non-0
    // capacity and is therefore WRITABLE to start with.
    std::uint32_t m_state_mask = chanState::WRITABLE;
};

//

// Unbuffered event channel i.e. a channel with capacity 0.
//
// No writes or reads are possible if the channel is closed. Closing a channel
// unblocks all waiting actors.
//
// Writes (see push()) to this channel can only be carried out when there's a
// corresponding blocked receiver waiting.
//
// Reads (see get()) from this channel can only be carried out when there's a
// corresponding blocked sender waiting.
//
// The channel can be monitored for readability or writability. NOTE: a monitor
// does **not** count as a **waiting** (i.e. blocking) receiver or sender.
// As a consequence, if all actors are monitors, no messages can/will be sent or
// received because monitors only use try_get/try_push (see comments below).
//
template<typename... types>
class trunk
    : public interfaces::wtrunk<trunk<types...>, types...>
    , public interfaces::rtrunk<trunk<types...>, types...> {
    //
    using is_tuple =
      typename tarp::type_traits::type_or_tuple<types...>::is_tuple;
    using this_type = trunk<types...>;
    using lock_t = std::unique_lock<std::mutex>;
    using mutex_t = std::mutex;
    using payload_type = tarp::type_traits::type_or_tuple_t<types...>;

    // Structure that represents a blocked, waiting writer (sender) or
    // reader (receiver).
    struct operation {
        // Sender CTOR.
        template<typename... T>
        operation(T &&...d) {
            if constexpr (tarp::type_traits::is_tuple_v<T...>) {
                data.emplace(std::make_tuple(std::forward<T>(d)...));
            } else {
                data.emplace(std::forward<T>(d)...);
            }
        }

        // Receiver CTOR.
        operation() = default;

        // Sender data to send or receiver data placeholder to populate.
        std::optional<payload_type> data;

        // True if data successfully sent/received.
        bool done = false;

        // Lets us individually wake up a sender or receiver. This helps
        // prevent thundering herds.
        std::condition_variable condvar;
    };

    // Structure that represents a monitor to be notified of changes in the
    // channel state.
    struct monitor_entry {
        monitor_entry(std::shared_ptr<notifier> notifier) : notif(notifier) {}

        std::shared_ptr<notifier> notif;
    };

public:
    using payload_t = payload_type;
    using Ts = std::tuple<types...>;
    using wtrunk_t = interfaces::wtrunk<this_type, types...>;
    using rtrunk_t = interfaces::rtrunk<this_type, types...>;

    DISALLOW_COPY_AND_MOVE(trunk);
    trunk() = default;

    // return a wtrunk interface reference.
    interfaces::wtrunk<this_type, types...> &as_wtrunk() { return *this; }

    // return an rtrunk interface reference.
    interfaces::rtrunk<this_type, types...> &as_rtrunk() { return *this; }

    // Convenience notation for pushing to a channel.
    //
    // NOTE: this just calls push(), so it is blocking.
    //
    // NOTE: chaining/cascading is not permitted because the returned value
    // is that of push() (see comments fmi) in order to avoid losing data on
    // failure, rather than *this.
    // That is, the following is not allowed:
    // mychan << event1 << event2 ... << eventn;
    //
    // NOTE: the input is expected to be a payload_t. If it is a multi-type
    // payload, then the caller must make a tuple.
    // trunk<unsigned> chan;
    // auto [ok, returned] = chan << 1u;
    //
    // trunk<unsigned, unsigned> chan;
    // auto [ok, returned] = chan << tuple(1u, 1u);
    auto operator<<(const payload_t &data) {
        // return *this;
        return push(data);
    }

    auto operator<<(payload_t &&data) {
        // return *this;
        return push(std::move(data));
    }

    // Convenience notation for getting from a channel.
    //
    // NOTE: this just calls get(), so it is blocking.
    //
    // NOTE: chaining is allowed in this case. std::optional-wrapped
    // payloads are stored in the arguments.
    //
    // trunk<int> chan;
    // chan << 1;
    // chan << 2;
    // chan << 3;
    // std::optional<int> a, b, c;
    // chan >> a >> b >> c;
    //  # NOTE: this will make (and block in) a call to get() 3 times.
    auto &operator>>(std::optional<payload_t> &event) {
        auto res = get();
        if (res.has_value()) {
            event.emplace(std::move(res.value()));
        }
        return *this;
    }

    // Add a monitor to be notified of changes involving the specified states.
    //
    // NOTE: a notification is emitted in two cases:
    // - state becomes active (rising-edge notification)
    // - state stops being active (falling-edge notification)
    //
    // For example, if state == chanState::READABLE, then a notification will
    // be emitted when:
    // - the channel becomes readable
    // - the channel becomes unreadable
    //
    // The edge-triggered semantics makes it easy to implement either edge
    // or level - triggered mechanisms on top.
    //
    // NOTE: once a monitor is added, this channel owns a shared_ptr reference
    // to the associated notifier. Since .notify() may be invoked at any
    // time, the monitor must ensure its notifier does not access resources
    // that have gone out of scope. For example, the notifier might outlive the
    // monitor since it's a shared_ptr; thus the notifier implementation must
    // be designed such that this case is properly handled.
    //
    // NOTE: a monitor should be prepared to handle spurious notifications.
    // E.g. if a WRITABLE notification is received, there is no guarantee the
    // channel will still be writable by the time the monitor eventually acts
    // on the notification, since *multiple* monitors could've been registered
    // and notified. Therefore there is an inherent race between monitors. Other
    // such situations may also result in a notification not reflecting the
    // true state of the channel. Therefore monitors should use the
    // try_{get,push} methods and be prepared to handle failures.
    //
    // == @return ==
    // Return a bitfield indicating the currently pending state events on the
    // channel. This is useful if a monitor is added after the occurence of
    // an event. Specifically, it is meant to prevent this type of scenario:
    //  - receiver calls get(), blocking forever.
    //  - monitor is added for state=WRITABLE
    //  - .. monitor never notified; receiver waits forever.
    std::uint32_t add_monitor(std::shared_ptr<notifier> notifier,
                              std::uint32_t states) {
        struct monitor_entry mon(notifier);

        lock_t l {m_mtx};

        if (states & chanState::READABLE) {
            m_recv_monitors.push_back(mon);
        }

        if (states & chanState::WRITABLE) {
            m_send_monitors.push_back(mon);
        }

        return m_state_mask;
    }

    // Close a channel.
    //
    // A closed channel cannot be reopened.
    // No read/write operations are possible on a closed channel.
    //
    // When the channel is closed, all blocked senders and receivers,
    // and all monitors are woken.
    // NOTE:
    //     Since any receivers/senders currently engaged in a blocking wait
    //     will be woken and immediately thereafter will access fields of this
    //     class, the caller must ensure the object does not go out of scope
    //     as this happens.
    void close() {
        decltype(m_recv_waitq) recvq;
        decltype(m_send_waitq) sendq;
        std::vector<std::shared_ptr<notifier>> monitors;

        {
            lock_t l {m_mtx};
            m_closed = true;
            m_state_mask |= chanState::CLOSED;

            // clear the send and recv wait queues.
            std::swap(recvq, m_recv_waitq);
            std::swap(sendq, m_send_waitq);

            // gather all monitors and clear the monitor queues.
            auto get_all_and_clear = [this, &monitors](auto &ls) {
                for (auto &i : ls) {
                    monitors.push_back(i.notif);
                }
                ls.clear();
            };
            get_all_and_clear(m_recv_monitors);
            get_all_and_clear(m_send_monitors);
        }

        // wake up all blocked senders and receivers
        auto wake = [&](auto &list) {
            for (auto &i : list) {
                i.get().condvar.notify_one();
            }
        };
        wake(recvq);
        wake(sendq);

        // notify all monitors
        for (auto &mon : monitors) {
            mon->notify(chanState::CLOSED, APPLY);
        }
    }

    // True if channel is closed, else False.
    bool closed() const {
        lock_t l {m_mtx};
        return m_closed;
    }

    // Push a message to the channel. Wait until this can be done.
    //
    // Return {true, std::nullopt} after pushing the data to the channel or
    // {false, std::optional<payload_t>} if the channel was closed before the
    // data could be pushed.
    //
    // The boolean serves as a discriminant that indicates whether the data was
    // successfully pushed or whether it was returned back.
    //
    // This avoids the case where method fails and the data is *lost*. Consider:
    //  - caller calls push() with a e.g. unique_ptr.
    //  - channel gets closed
    //  - the push failed. The data was *not* pushed and it has been lost, since
    //    the caller no longer has it.
    //  By returning the data to the user in case of failure, the caller can
    //  best decide how to proceed: discard the data, retry, push to another
    //  channel etc.
    template<typename... T>
    auto push(T &&...data) {
        return do_try_push_until(m_past_tp, false, std::forward<T>(data)...);
    }

    // Like push, but return if the operation is not immediately doable.
    //
    // For example, a push cannot be done if the channel is closed or there is
    // no **waiting (blocked)** receiver.
    //
    // NOTE: since this operation never waits, which is to say it never amounts
    // to attaching a blocking sender, it means monitors or waiting
    // receivers will not be notified of there being a new sender. IOW try_push
    // does not change the readability of the channel.
    // It's important to realize this because monitors always use the try_*
    // functions. If there are two monitors monitoring a channel for READABLE
    // and WRITABLE, respectively, state changes, and there are no
    // blocking senders/receivers, then a send or receive will never happen!
    template<typename... T>
    std::pair<bool, std::optional<payload_type>> try_push(T &&...data) {
        lock_t l {m_mtx};

        if (m_closed or m_recv_waitq.empty()) {
            // std::cerr << "Failed try_push --> closed=" << m_closed
            //           << " recv_waitq size=" << m_recv_waitq.size()
            //           << std::endl;
            return {false, opt_payload(std::forward<T>(data)...)};
        }

        pass_data(l, std::forward<T>(data)...);
        return {true, std::nullopt};
    }

    // Like push(), but the wait has a deadline.
    template<typename timepoint, typename... T>
    auto try_push_until(const timepoint &abs_time, T &&...data) {
        return do_try_push_until(abs_time, true, std::forward<T>(data)...);
    }

    template<class Rep, class Period, typename... T>
    auto try_push_for(const std::chrono::duration<Rep, Period> &rel_time,
                      T &&...data) {
        auto deadline = CLOCK::now() + rel_time;
        return try_push_until(deadline, std::forward<T>(data)...);
    }

    // Get a message from the channel. Wait util this can be done.
    // See push() FMI.
    // nullopt is returned if the get ultimately fails. This can happen when
    // e.g. the channel gets closed.
    std::optional<payload_t> get() {
        return do_try_get_until(m_past_tp, false);
    }

    // Like get(), but return if the operation is not immediately doable.
    // This operation never waits and therefore never amounts to attaching a
    // waiting receiver; consequently it does not change the writability
    // of the channel.
    // See try_push fmi.
    std::optional<payload_t> try_get() {
        lock_t l {m_mtx};

        if (m_closed) {
            return std::nullopt;
        }

        if (!m_send_waitq.empty()) {
            return get_data(l);
        }

        std::cerr << "try_get(): m_send_waitq empty, impossible to get"
                  << std::endl;
        return std::nullopt;
    }

    // Like get(), but the wait has a deadline.
    template<typename timepoint>
    std::optional<payload_t> try_get_until(const timepoint &abs_time) {
        return do_try_get_until(abs_time, true);
    }

    template<class Rep, class Period>
    std::optional<payload_t>
    try_get_for(const std::chrono::duration<Rep, Period> &rel_time) {
        auto deadline = CLOCK::now() + rel_time;
        return try_get_until(deadline);
    }

private:
    // Block until one of the following conditions is true:
    // 1) (use_deadline=true AND) the deadline has passed.
    // 2) the channel has been closed.
    // 3) the data has been pushed to the channel.
    //
    // Return {true, nullopt} if the data has been pushed to the channel,
    // else {false, optional<data>}. See push() fmi.
    template<typename timepoint, typename... T>
    std::pair<bool, std::optional<payload_type>>
    do_try_push_until(const timepoint &abs_time,
                      bool use_deadline,
                      T &&...data) {
        lock_t l {m_mtx};
        // std::cerr << "try_push got mutex " << std::endl;

        if (m_closed) {
            return {false, opt_payload(std::forward<T>(data)...)};
        }

        // If we can do the push immediately without waiting, then do it.
        if (!m_recv_waitq.empty()) {
            pass_data(l, std::forward<T>(data)...);
            return {true, std::nullopt};
        }

        // else, join the wait queue and block.
        struct operation op(std::forward<T>(data)...);
        add_sender(l, op);
        refresh_channel_state(l);

        // NOTE: each sender/receiver gets its own condition variable so that
        // it can be invidually woken up. This lets us avoid the thundering
        // herd problem. However, all of them still use one and the same mutex.
        // The reason is if we gave each its own mutex we would get races
        // and potential deadlocks (e.g. a sender/receiver will first lock its
        // own mutex but if its deadline has expired it will try to lock the
        // trunk mutex. The trunk logic does the reverse: it locks the trunk
        //  mutex and when waking a receiver/sender it will lock the receiver/
        //  sender's mutex. So the order is backward in each case and we can
        //  easily end up in a deadlock).
        //
        // INVARIANT: when we unlock in this loop there are two cases.
        // 1) we were actually signaled. This happens either when the channel
        // is closed, OR when the data is passed to a receiver. In either case,
        // the sender will have been removed from the sender wait q.
        // 2) either a spurious wakeup or timeout. In case of timeout, we must
        // remove ourselves from the sender wait queue before returning.
        while (true) {
            // std::cerr << "[PUSH] will wait on condvar" << std::endl;
            if (use_deadline) {
                op.condvar.wait_until(l, abs_time);
            } else {
                op.condvar.wait(l);
            }
            // std::cerr << "[PUSH] after wait on condvar" << std::endl;

            // We do not call refresh_channel_state() here because it is called
            // in get_data().
            if (op.done) {
                return {true, std::nullopt};
            }

            if (m_closed) {
                return {false, std::move(op.data)};
            }

            if (use_deadline && abs_time <= CLOCK::now()) {
                remove_sender(l, op);
                refresh_channel_state(l);
                return {false, std::move(op.data)};
            }
        }
    }

    // see do_try_push_until fmi.
    template<typename timepoint>
    std::optional<payload_t> do_try_get_until(const timepoint &abs_time,
                                              bool use_deadline) {
        lock_t l {m_mtx};

        if (m_closed) {
            return std::nullopt;
        }

        // If we can do the get immediately without waiting, then do it.
        if (!m_send_waitq.empty()) {
            return get_data(l);
        }

        // else, join the wait queue and block.
        struct operation op;
        add_receiver(l, op);
        refresh_channel_state(l);

        while (true) {
            // std::cerr << "[GET] will wait on condvar" << std::endl;
            if (use_deadline) {
                op.condvar.wait_until(l, abs_time);
            } else {
                op.condvar.wait(l);
            }
            // std::cerr << "[GET] after wait on condvar" << std::endl;

            // We do not call refresh_channel_state() here because it is called
            // in pass_data().
            if (op.done) {
                return std::move(op.data);
            }

            if (m_closed) {
                return std::nullopt;
            }

            if (use_deadline && abs_time < CLOCK::now()) {
                remove_receiver(l, op);
                refresh_channel_state(l);
                return std::nullopt;
            }
        }
    }

    // Pass the data to a waiting receiver.
    // The first receiver (i.e. the one that has been waiting the longest) is
    // passed the data. The receiver is then removed from the receiver waitqueue
    // and woken up.
    template<typename... T>
    void pass_data(lock_t &l, T &&...data) {
        struct operation &receiver = m_recv_waitq.front();
        receiver.data.emplace(std::forward<T>(data)...);
        receiver.done = true;
        receiver.condvar.notify_one();
        m_recv_waitq.pop_front();
        refresh_channel_state(l);
    }

    // Get the data from a waiting sender.
    // The first sender (i.e. the one that has been waiting the longest) is
    // selected. The data is *moved* from it. The sender is removed from the
    // wait queue and woken up.
    std::optional<payload_t> get_data(lock_t &l) {
        struct operation &sender = m_send_waitq.front();
        auto data = std::move(sender.data);
        sender.done = true;
        sender.condvar.notify_one();
        m_send_waitq.pop_front();
        refresh_channel_state(l);
        return data;
    }

    // add receiver to wait queue
    void add_receiver(lock_t &, struct operation &op) {
        m_recv_waitq.emplace_back(op);
    }

    // add sender to wait queue
    void add_sender(lock_t &, struct operation &op) {
        m_send_waitq.emplace_back(op);
    }

    // Scan the sender waitq and remove the specified sender, if found.
    void remove_sender(lock_t &, struct operation &op) {
        m_send_waitq.remove_if([&op](auto &i) {
            return std::addressof(i.get()) == std::addressof(op);
        });
        // std::cerr << "sender removed" << std::endl;
    }

    // Scan the receiver waitq and remove the specified receiver, if found.
    void remove_receiver(lock_t &, struct operation &op) {
        m_recv_waitq.remove_if([&op](auto &i) {
            return std::addressof(i.get()) == std::addressof(op);
        });
        // std::cerr << "receiver removed" << std::endl;
    }

    // Consider the current state of the channel and update m_state_mask
    // based on that and send out any new state change notifications as
    // appropriate.
    void refresh_channel_state(lock_t &) {
        // assume not writable and not readable.
        std::uint32_t current_state = 0;

        // writable if there are blocked receivers waiting.
        if (!m_recv_waitq.empty()) {
            current_state |= chanState::WRITABLE;
        }

        // readable if there are blocked senders waiting.
        if (!m_send_waitq.empty()) {
            current_state |= chanState::READABLE;
        }

        auto notify_monitors = [this](auto &ls, auto state_flags, auto action) {
            for (auto it = ls.begin(); it != ls.end();) {
                auto &notifier = it->notif;

                // NOTE: notifier->notify() **must not** call us back, else we
                // get a deadlock.
                bool must_remove = !notifier->notify(state_flags, action);

                // std::cerr << "called notify() with state=" <<
                // static_cast<unsigned>(state) << " and mask=" <<
                // static_cast<unsigned>(mask) << ", got " << must_remove <<
                // std::endl;

                if (must_remove) {
                    it = ls.erase(it);
                    continue;
                }
                ++it;
            }
        };

        // ====== generate state change notifications. These are edge-triggered
        // notifications. No notification is generated if the state has not
        // changed.

        // no changes
        if (current_state == m_state_mask) {
            return;
        }

        using S = chanState;

        // the channel has _become_ READABLE/WRITABLE; rising edge
        if ((current_state & S::READABLE) && !(m_state_mask & S::READABLE)) {
            notify_monitors(m_recv_monitors, S::READABLE, APPLY);
        }
        if ((current_state & S::WRITABLE) && !(m_state_mask & S::WRITABLE)) {
            notify_monitors(m_send_monitors, S::WRITABLE, APPLY);
        }

        // the channel has become NOT READABLE/WRITABLE; falling edge
        if ((m_state_mask & S::READABLE) && !(current_state & S::READABLE)) {
            notify_monitors(m_recv_monitors, S::READABLE, CLEAR);
        }
        if ((m_state_mask & S::WRITABLE) && !(current_state & S::WRITABLE)) {
            notify_monitors(m_send_monitors, S::WRITABLE, CLEAR);
        }

        m_state_mask = current_state;
        if (m_closed) {
            m_state_mask |= chanState::CLOSED;
        }
    }

    // return a std::optional event payload, stored according to its
    // type_or_tuple semantics.
    template<typename... T>
    constexpr auto opt_payload(T &&...data) {
        std::optional<payload_type> opt;
        if constexpr (tarp::type_traits::is_tuple_v<T...>) {
            opt.emplace(std::make_tuple(std::forward<T>(data)...));
        } else {
            opt.emplace(std::forward<T>(data)...);
        }
        return opt;
    }

private:
    using CLOCK = std::chrono::steady_clock;
    const CLOCK::time_point m_past_tp {CLOCK::now()};

    mutable mutex_t m_mtx;
    bool m_closed {false};

    // sender and receiver wait queues.
    std::list<std::reference_wrapper<struct operation>> m_send_waitq;
    std::list<std::reference_wrapper<struct operation>> m_recv_waitq;

    // send and receive monitor queues.
    std::list<struct monitor_entry> m_send_monitors;
    std::list<struct monitor_entry> m_recv_monitors;

    // an event can happen before the addition of any monitor;
    // we need to track these so we can signal the true state of the channel
    // when a monitor joins.
    std::uint32_t m_state_mask = 0;
};

//

// Helper to efficiently monitor a number of channels for read/write -ability.
//
// NOTE:
// Callers must be prepared to handle (apparently) spurious wakeups, so they
// should use try_get/try_push so that failure can be handled. Otherwise if
// get()/push() are called, there's the risk of blocking on a
// non-{writable/readable} channel, despite the notification.
// See the comments for trunk::add_monitor() fmi.
//
// To make it possible to monitor a heterogenous collection of channels, trunks,
// and other higher level constructs, these are not stored in the monitor.
// Rather each is associated with a label that is then always returned to the
// caller in its place. This is normally some id or lookup key.
//  NOTE: the label/id must uniquely identify a channel/trunk etc in the
//  monitor.
class monitor {
private:
    using lockref = std::unique_lock<std::mutex> &;

    // We keep the monitor 'subscriptions' and associated state in a shared_ptr
    // to simplify correct lifetime management. In particular, the notifier may
    // outlive this monitor; giving it a shared pointer ensures there is no
    // use-after-free problems. The state is kept as long as it is needed and
    // its internal fields are mutex-guarded to ensure correct logic.
    struct state {
    public:
        binary_semaphore_t m_sem;
        std::mutex m_mtx;

        using flags_t = std::uint32_t;
        using interest_mask_t = flags_t;
        using id_t = std::uint32_t;

        // Subscription entry. flags_t is updated based on channel
        // notifications. interest_mask_t is the actual events the client is
        // interested in being notified about.
        std::unordered_map<id_t, std::pair<flags_t, interest_mask_t>>
          m_notified_map;

        // All the channels that are in a state of interest. I.e. their level
        // is 'high' --> used for level-triggered notifications.
        std::unordered_set<id_t> m_high;

        // All the channels that have entered a state of interest since last
        // time. Used for edge-triggered notifications.
        std::unordered_set<id_t> m_edged;
    };

    std::shared_ptr<struct state> m_state;

    // Mechanism via which a channel can notify the monitor of changes in the
    // channel state.
    class channel_notifier final : public notifier {
    public:
        channel_notifier(std::uint32_t id, std::shared_ptr<struct state> state)
            : m_lookup_key(id), m_state(std::move(state)) {}

        bool notify(uint32_t events, uint32_t action) override {
            bool wake = (action == APPLY);

            // monitor gone; return false to remove notifier.
            auto state = m_state.lock();
            if (!state) {
                return false;
            }

            {
                std::unique_lock l {state->m_mtx};

                // unsubscribed => remove notifier.
                auto found = state->m_notified_map.find(m_lookup_key);
                if (found == state->m_notified_map.end()) {
                    state->m_edged.erase(m_lookup_key);
                    state->m_high.erase(m_lookup_key);
                    return false;
                }

                using S = chanState;

                [[maybe_unused]] auto &[stored_events, interest_mask] =
                  found->second;
                std::uint32_t risen_states = 0;
                std::uint32_t high_states = 0;

                // Update existing state knowledge to latest.
                //
                // std::cerr << " * * * notification. id=" << m_lookup_key << "
                // events=" << events  << " apply=" << action << std::endl;

                if (action == APPLY) {
                    // Check for rising edges:
                    // state changes TO high=readable/writable
                    // from low=UN{readable/writable}
                    if (events & S::READABLE &&
                        !(stored_events & S::READABLE)) {
                        risen_states |= S::READABLE;
                    }
                    if (events & S::WRITABLE &&
                        !(stored_events & S::WRITABLE)) {
                        risen_states |= S::WRITABLE;
                    }

                    // else, chanState::CLOSED, or _not_ a rising edge (i.e.
                    // already high).
                    stored_events |= events;
                } else if (action == CLEAR) {
                    stored_events &= ~events;
                } else {
                    throw std::logic_error("BUG: unknown action");
                }

                // check if the current level is high (=readable/writable).
                high_states = (stored_events & (S::READABLE | S::WRITABLE));

                // if we got any rising-edge notifications for any events of
                // interest
                if (risen_states & interest_mask) {
                    state->m_edged.insert(m_lookup_key);
                }

                // if the level is high for any states of interest
                if (high_states & interest_mask) {
                    state->m_high.insert(m_lookup_key);
                } else {
                    // otherwise no edge and not high, so no notification at
                    // all.
                    state->m_high.erase(m_lookup_key);
                    state->m_edged.erase(m_lookup_key);
                }

                // chanState::CLOSED notifications are always passed through.
                if (stored_events & chanState::CLOSED) {
                    state->m_high.insert(m_lookup_key);
                    state->m_edged.insert(m_lookup_key);
                    wake = true;
                }
            }

            // new notification(s); unblock the wait.
            if (wake) {
                state->m_sem.release();
            }

            return true;
        }

    private:
        std::uint32_t m_lookup_key {0};
        std::weak_ptr<struct state> m_state;
    };

    // Get the latest list of ready channels.
    auto get_latest(lockref) {
        decltype(m_state->m_edged) edged;
        std::swap(edged, m_state->m_edged);

        std::list<std::pair<state::id_t, state::flags_t>> results;

        // NOTE: currently only level-triggered monitoring is implemented
        // in the public API. To support edge-triggered monitoring we simply
        // have to ONLY include a channel id in the result list IFF it appears
        // in the edged list. I.e. there is an unhandled edge-triggered
        // notification pending for it.
        // Otherwise, if the monitoring for this channel is level-triggered,
        // we ONLY put the entry in the results list IFF it appears in the
        // 'high' list.
        // Since currently we only support level-triggered monitoring, we simply
        // copy the 'high' list of notifications. Note m_high is a superset of
        // m_edged since all rising-edge notifications make the level 'high',
        // and therefore trigger a level notification as well.
        for (auto k : m_state->m_high) {
            auto found = m_state->m_notified_map.find(k);
            if (found == m_state->m_notified_map.end()) {
                throw std::logic_error("BUG: failed lookup");
            }

            // get the actual (latest) events
            auto [events, _] = found->second;

            if (!events) {
                continue;
            }

            // std::cerr << "EVENT PENDING: k=" << k << " events=" << events <<
            // std::endl;
            results.push_back({k, events});
        }

        return results;
    }

public:
    monitor() { m_state = std::make_shared<struct state>(); }

    // block until an event happens OR the deadline is reached; return list of
    // events.
    template<typename timepoint>
    auto wait_until(const timepoint &abs_time) {
        {
            std::unique_lock l {m_state->m_mtx};
            if (!m_state->m_high.empty()) {
                auto pending = get_latest(l);
                if (!pending.empty()) {
                    // std::cerr << "pending not empty, returning now\n";
                    return pending;
                }
            }
        }

        m_state->m_sem.try_acquire_until(abs_time);
        std::unique_lock l {m_state->m_mtx};
        return get_latest(l);
    }

    template<class Rep, class Period>
    auto wait_for(const std::chrono::duration<Rep, Period> &rel_time) {
        return wait_until(std::chrono::steady_clock::now() + rel_time);
    }

    // block until an event happens; return list of events.
    auto wait() {
        {
            std::unique_lock l {m_state->m_mtx};
            if (!m_state->m_high.empty()) {
                auto pending = get_latest(l);
                if (!pending.empty()) {
                    // std::cerr << "pending not empty, returning now\n";
                    return pending;
                }
            }
        }

        m_state->m_sem.acquire();
        std::unique_lock l {m_state->m_mtx};
        return get_latest(l);
    }

    // Add a monitor for channel state changes as specified in mask.
    // id is any useful label identifying the channel by which the user
    // can easily find the channel (see wait()). A subsequent call with the same
    // id will overwrite any existing entry that has that id. Therefore the id
    // must uniquely identify an entry inside the monitor.
    template<typename SOURCE>
    void watch(SOURCE &src, enum chanState mask, std::uint32_t id) {
        std::shared_ptr<channel_notifier> notifier;
        std::uint32_t pending_evs = 0;

        {
            std::unique_lock l {m_state->m_mtx};
            m_state->m_notified_map[id] = {0, mask};
            notifier = std::make_shared<channel_notifier>(id, m_state);
            pending_evs = src.add_monitor(notifier, mask);
        }

        // std::cerr << "added watch with mask " << static_cast<unsigned>(mask)
        // << "; pending evs=" << pending_evs << std::endl;

        // see trunk::add_monitor().
        if (pending_evs & mask) {
            notifier->notify(pending_evs, APPLY);
        }
    }

    // Stop monitoring the channel with the given id.
    void unwatch(std::uint32_t id) {
        std::unique_lock l {m_state->m_mtx};
        m_state->m_notified_map.erase(id);
        m_state->m_edged.erase(id);
        m_state->m_high.erase(id);
    }
};

// A single-producer multi-consumer (SPMC) event dispatcher.
//
// This can employed as a rudimentary publish-subscribe interface,
// i.e. the asynchronous counterpart to the observer (aka
// signals&slots) design pattern. The 'subscribers' can attach
// channels to the broadcaster, thus forming a broadcast group.
// An event enqueued to the broadcaster will then be written
// to every single attached channel.
//
// The broadcasting treats the channels as non-blocking by using
// try_push()/try_get(). This also means it is lossy. The client
// of a broadcaster cannot know how many or indeed if any channels
// received the broadcast.
//
// A class embedding an event_broadcaster therefore becomes
// an event publisher; the 'topic'/event is defined by whatever
// name is given to it by the publisher and by the data type
// of the event channel.
//
// NOTE that by definition the 'event' i.e. the data item must be
// copyable, since one event is cloned to be written to an
// arbitrary number of connected channels. Use an event_stream instead
// if move-only events must be communicated.
template<typename ts_policy, typename... types>
class event_broadcaster final {
    static_assert(((std::is_copy_assignable_v<types> ||
                    std::is_copy_constructible_v<types>) &&
                   ...));
    using lock_t = typename tarp::type_traits::ts_types<ts_policy>::lock_t;
    using mutex_t = typename tarp::type_traits::ts_types<ts_policy>::mutex_t;
    using event_channel_t = event_channel<ts_policy, types...>;
    using wchan_t = wchan<event_channel_t, types...>;

public:
    using payload_t = typename event_channel_t::payload_t;

    DISALLOW_COPY_AND_MOVE(event_broadcaster);

    // If autodispatch is enabled, then any event pushed to the
    // brodcaster is immediately broadcast. OTOH if autodispatch=false,
    // events are buffered into a first-stage queue and only get
    // broadcast when .dispatch() is explicitly invoked.
    event_broadcaster(bool autodispatch = false)
        : m_autodispatch(autodispatch) {}

    // Enqueue an event, pending broadcast.
    template<typename... event_data_t>
    void push(event_data_t &&...data) {
        m_event_buffer.try_push(std::forward<event_data_t>(data)...);

        // if autodispatch is enabled, then always flush immediately: do
        // not buffer.
        if (m_autodispatch) {
            dispatch();
        }
    }

    // Flush the event buffer: broadcast all buffered events.
    // Note this is lossy, i.e. reliable delivery cannot be guaranteed: maybe
    // there are no channels, maybe some channels have been closed, maybe the
    // channels are overfilled and do not (or do!) use ring-buffer semantics.
    void dispatch() {
        lock_t l {m_mtx};

        std::vector<std::shared_ptr<wchan_t>> channels;

        for (auto it = m_event_channels.begin();
             it != m_event_channels.end();) {
            auto channel = it->lock();

            // lazily remove dangling dead channels.
            if (!channel) {
                it = m_event_channels.erase(it);
                continue;
            }

            channels.emplace_back(channel);
            ++it;
        }

        auto events = m_event_buffer.get_all();
        std::size_t num_channels = channels.size();

        for (auto &event : events) {
            for (std::size_t i = 0; i < num_channels; ++i) {
                channels[i]->try_push(event);
            }
        }
    }

    // More convenient and expressive overloads for enqueuing an event.
    auto &operator<<(payload_t &&event_data) {
        push(std::move(event_data));
        return *this;
    }

    auto &operator<<(const payload_t &event_data) {
        push(event_data);
        return *this;
    }

    // Get the number of channels connected. This includes dangling
    // channels that are no longer alive.
    std::size_t num_channels() const {
        lock_t l {m_mtx};
        return m_event_channels.size();
    }

    // Join the broadcast group to receive events over the specified channel.
    void connect(std::weak_ptr<wchan_t> channel) {
        lock_t l {m_mtx};
        m_event_channels.push_back(channel);
    }

private:
    static inline constexpr std::size_t m_BUFFSZ {1000};
    mutable mutex_t m_mtx;
    const bool m_autodispatch {false};
    event_channel_t m_event_buffer {m_BUFFSZ, true};
    std::vector<std::weak_ptr<wchan_t>> m_event_channels;
};

//

// A stream that exposes a shared wchan to client producers.
// See event_stream fmi.
//
// Note the stream eventing is lossy. See event_broadcaster as well fmi
// and event_channel::try_push.
//
// Note the stream cannot be monitored because it is *always* writable.
template<typename ts_policy, typename... types>
class event_rstream : public event_stream<event_rstream<ts_policy, types...>> {
    //
    using lock_t = typename tarp::type_traits::ts_types<ts_policy>::lock_t;
    using mutex_t = typename tarp::type_traits::ts_types<ts_policy>::mutex_t;
    using event_channel_t = event_channel<ts_policy, types...>;
    using event_rchan_t = rchan<event_channel_t, types...>;
    using this_type = event_rstream<ts_policy, types...>;
    //
public:
    using payload_t = typename event_channel_t::payload_t;

    // Construct an rstream backed by a channel with the specified max capacity
    // and using ring-buffer semantics. See event_channel fmi.
    event_rstream(bool autoflush = false, std::size_t channel_capacity = 100)
        : m_autoflush(autoflush)
        , m_channel_capacity(channel_capacity)
        , m_event_buffer(channel_capacity, true) {}

    ~event_rstream() { close(); }

    // close the underlying event channel.
    void close() {
        m_event_buffer.close();
        auto chan = m_stream_channel.lock();
        if (chan) {
            chan->close();
        }
    }

    // Enqueue an event to the stream channel. This buffers the event
    // until flush() is called, or if autoflush=true, it dispatches the
    // event immediately. If buffered, keep in mind the size of the first-stage
    // queue has a maximum capacity specified at construction time.
    template<typename... event_data_t>
    void push(event_data_t &&...data) {
        // if autoflush enabled, then do not buffer.
        if (m_autoflush) {
            // Do not stream if no one is listening.
            auto chan = m_stream_channel.lock();
            if (!chan) {
                return;
            }
            push_to_channel(*chan, std::forward<event_data_t>(data)...);
            return;
        }

        // else buffer
        push_to_channel(m_event_buffer, std::forward<event_data_t>(data)...);
    }

    // Send off all buffered events.
    void flush() {
        lock_t l {m_mtx};

        auto events = m_event_buffer.get_all();

        auto chan = m_stream_channel.lock();

        // no one is listening, so no point streaming any events!;
        // events already enqueued get silently dropped.
        if (!chan) {
            return;
        }

        while (!events.empty()) {
            if constexpr (((std::is_copy_assignable_v<types> ||
                            std::is_copy_constructible_v<types>) &&
                           ...)) {
                chan->push(events.front());
            } else {
                chan->push(std::move(events.front()));
            }
            events.pop_front();
        }
    }

    // More convenient and expressive overloads for enqueuing
    // and dequeueing an element.
    auto &operator<<(payload_t &&event_data) {
        push(std::move(event_data));
        return *this;
    }

    auto &operator<<(const payload_t &event_data) {
        push(event_data);
        return *this;
    }

    // Return the underlying stream channel (create it if it does not exist).
    std::shared_ptr<event_rchan_t> channel() {
        auto chan = m_stream_channel.lock();
        if (chan) {
            return chan;
        }

        chan = std::make_shared<event_channel_t>(m_channel_capacity, true);
        m_stream_channel = chan;
        return chan;
    }

    // Return a restricted stream interface suitable
    // for API clients. This only allows the client to get
    // the stream (r)channel.
    auto &interface() {
        return static_cast<interfaces::event_stream<this_type> &>(*this);
    }

private:
    template<typename C, typename... T>
    void push_to_channel(C &chan, T &&...data) {
        chan.try_push(std::forward<T>(data)...);
    }

    mutable mutex_t m_mtx;
    const bool m_autoflush {false};
    const std::size_t m_channel_capacity {0};
    event_channel_t m_event_buffer;
    std::weak_ptr<event_channel_t> m_stream_channel;
};

//

// A stream that exposes a shared rchan to client producers.
// See event_stream fmi.
//
// Note the stream eventing is lossy. If producers are faster than we can
// dequeue, the max capacity and ring-buffer semantics of the underlying
// channel mean events will be lost.
//
// The underlying channel can be monitored for readability.
template<typename ts_policy, typename... types>
class event_wstream
    : public interfaces::event_stream<event_wstream<ts_policy, types...>> {
    //
    using event_channel_t = event_channel<ts_policy, types...>;
    using event_wchan_t = wchan<event_channel_t, types...>;
    using this_type = event_wstream<ts_policy, types...>;
    //
public:
    using payload_t = typename event_channel_t::payload_t;

    // Construct a stream backed by a channel with ring-buffer semantics and
    // the given max capacity.
    event_wstream(std::size_t chancap = 100) {
        m_stream_channel = std::make_shared<event_channel_t>(chancap, true);
    }

    ~event_wstream() { close(); }

    void close() { m_stream_channel->close(); }

    std::optional<payload_t> try_get() { return m_stream_channel->try_get(); }

    std::deque<payload_t> get_all() { return m_stream_channel->get_all(); }

    auto &operator>>(std::optional<payload_t> &event) {
        m_stream_channel->operator>>(event);
        return *this;
    }

    std::shared_ptr<event_wchan_t> channel() { return m_stream_channel; }

    // Return a restricted stream interface suitable
    // for API clients. This only allows the client to get
    // the stream channel.
    auto &interface() {
        return static_cast<interfaces::event_stream<this_type> &>(*this);
    }

    // Monitor the underlying channel for readability (and, implicitly,
    // chanState::CLOSED events).
    std::uint32_t add_monitor(std::shared_ptr<notifier> notifier,
                              std::uint32_t states = chanState::READABLE) {
        if (!(states & chanState::READABLE)) {
            throw std::invalid_argument(
              "Unacceptable state specified for monitoring");
        }
        return m_stream_channel->add_monitor(notifier, chanState::READABLE);
    }

private:
    std::shared_ptr<event_channel_t> m_stream_channel;
};

// A convenient interface for creating and storing event channels by an
// associated lookup key. The main purpose is to provide a composite
// interface for a readable channel. Dequeing from the event_aggregator
// dequeues from _one of_ its associated channels. When the event_aggregator
// is readable, it means any one of its channels is readable.
template<typename ts_policy, typename key_t, typename... types>
class event_aggregator final {
    //
    using lock_t = typename tarp::type_traits::ts_types<ts_policy>::lock_t;
    using mutex_t = typename tarp::type_traits::ts_types<ts_policy>::mutex_t;
    using event_channel_t = event_channel<ts_policy, types...>;
    using event_wchan_t = wchan<event_channel_t, types...>;
    using payload_t = typename event_channel_t::payload_t;
    using this_type = event_rstream<ts_policy, types...>;

    // This is meant to be stored in a shared_ptr to simplify lifetime
    // management. This is in order to prevent a notifier that outlives the
    // event_aggregator from committing use-after-free.
    struct state {
    public:
        std::mutex m_mtx;
        bool m_closed {false};

        // all the member channels that are readable. (we are only interested
        // in readability here). When this set is non-empty, we know the
        // event_aggregator as a whole is readable.
        std::unordered_set<key_t> m_readable;

        // keys tracking the alive-ness of notifiers used between
        // event_aggregator and managed channels.
        std::unordered_set<key_t> m_subscriptions;

        // Notifiers used between event_aggregator and its clients.
        std::list<std::shared_ptr<notifier>> m_notifiers;

        std::size_t m_idx {0};
        std::vector<std::shared_ptr<event_channel_t>> m_channels;
        std::unordered_map<key_t, std::weak_ptr<event_channel_t>> m_index;

        // convert the key_t keys to uint32_t keys for internal use.
        std::unordered_map<key_t, std::uint32_t> m_key_map;
        std::uint32_t m_last_key {0};
    };

    std::shared_ptr<struct state> m_state;

    // event_aggregator sets up notifiers to monitor the readability of each
    // managed channel. event_aggregator clients can also monitor the
    // readability of the entire event_aggregator composite as a whole. To do
    // this, they specify notifiers as well, but these notifiers are used
    // between the event_aggregator and its clients, **not** between
    // event_aggregator and its managed channels.
    class channel_notifier final : public notifier {
    public:
        channel_notifier(std::uint32_t id, std::shared_ptr<struct state> state)
            : m_lookup_key(id), m_state(std::move(state)) {}

        bool notify(uint32_t events, uint32_t action) override {
            // event_aggregator gone; return false to remove notifier.
            auto state = m_state.lock();
            if (!state) {
                return false;
            }

            std::unique_lock l {state->m_mtx};

            // channel no longer part of the event_aggregator => remove
            // notifier.
            if (state->m_subscriptions.count(m_lookup_key) == 0) {
                return false;
            }

            if (state->m_closed) {
                return false;
            }

            using S = chanState;

            bool rising_edge = false;
            bool falling_edge = false;

            /// if m_readable goes from empty to non-empty, then we have
            /// a rising edge: the whole event_aggregator goes from
            /// non-readable to readable.
            if ((action == APPLY) and (events & chanState::READABLE)) {
                rising_edge = state->m_readable.empty();
                state->m_readable.insert(m_lookup_key);
            }

            // if m_readable goes from non-empty to empty, then we have
            // a falling edge: the whole event_aggregator goes from
            // readable to non-readable.
            else if ((action == CLEAR) and (events & chanState::READABLE)) {
                auto old_size = state->m_readable.size();
                state->m_readable.erase(m_lookup_key);
                if (old_size > 0 and state->m_readable.empty()) {
                    falling_edge = true;
                }
            }

            // NOTE: We only notify on rising or falling edges
            if (rising_edge) {
                invoke_notifiers(state->m_notifiers, S::READABLE, APPLY);
            } else if (falling_edge) {
                invoke_notifiers(state->m_notifiers, S::READABLE, CLEAR);
            }

            return true;
        }

    private:
        std::uint32_t m_lookup_key {0};
        std::weak_ptr<struct state> m_state;
    };

    template<typename notifiers_list>
    static void invoke_notifiers(notifiers_list &ls,
                                 std::uint32_t flags,
                                 std::uint32_t action) {
        for (auto it = ls.begin(); it != ls.end();) {
            auto &notifier = *it;
            bool must_remove = notifier->notify(flags, action);
            if (must_remove) {
                it = ls.erase(it);
                continue;
            }
            ++it;
        }
    }

    //
public:
    DISALLOW_COPY_AND_MOVE(event_aggregator);

    event_aggregator() { m_state = std::make_shared<struct state>(); }

    // Create a channel if it does not exist and associate it with the given k.
    // Otherwise if a channel with key k exists, return it.
    // The channel will have the specified max capacity and will use ring-buffer
    // semantics.
    std::shared_ptr<event_wchan_t>
    channel(const key_t &k, unsigned int chancap = m_DEFAULT_CHANCAP) {
        std::uint32_t pending {0};
        std::shared_ptr<channel_notifier> notifier;
        auto &S = *m_state;
        std::shared_ptr<event_wchan_t> channel;

        {
            lock_t l {S.m_mtx};

            auto found = S.m_index.find(k);
            if (found != S.m_index.end()) {
                auto chan = found->second.lock();
                if (chan) {
                    return chan;
                }
            }

            if (S.m_closed) {
                return nullptr;
            }

            auto chan = std::make_shared<event_channel_t>(chancap, true);
            S.m_index[k] = chan;

            std::uint32_t internal_key = S.m_last_key++;
            S.m_key_map[k] = internal_key;

            S.m_channels.push_back(chan);

            notifier =
              std::make_shared<channel_notifier>(internal_key, m_state);
            m_state->m_subscriptions.insert(internal_key);
            pending = chan->add_monitor(notifier, chanState::READABLE);

            channel = chan;
        }

        if (pending & chanState::READABLE) {
            notifier->notify(pending, APPLY);
        }

        return channel;
    }

    // Remove the channel with key k if found.
    void remove(const key_t &k) {
        auto &S = *m_state;
        lock_t l {S.m_mtx};

        std::shared_ptr<event_channel_t> chan;

        auto found = S.m_index.find(k);
        if (found != S.m_index.end()) {
            chan = found->second.lock();
            S.m_index.erase(found);
        }

        if (!chan) {
            return;
        }

        for (auto it = S.m_channels.begin(); it != S.m_channels.end();) {
            if (*it == chan) {
                it = S.m_channels.erase(it);
                continue;
            }
            ++it;
        }

        auto internal_key = S.m_key_map[k];
        S.m_key_map.erase(k);
        S.m_subscriptions.erase(internal_key);
        auto size_before = S.m_readable.size();
        S.m_readable.erase(internal_key);

        // falling edge.
        if (size_before > 0 && S.m_readable.empty()) {
            invoke_notifiers(S.notifiers, chanState::READABLE, CLEAR);
        }
    }

    // Get an event from one of the channels. If there are no channels, or
    // if no channels have any events, then return std::nullopt;
    // Otherwise use a round-robin discpline when dequeuing in order to avoid
    // dequeuing from the same single channel all the time.
    std::optional<payload_t> try_get() {
        auto &S = *m_state;
        auto &m_idx = S.m_idx;

        lock_t l {S.m_mtx};

        if (S.m_channels.empty()) {
            return std::nullopt;
        }

        std::size_t n = S.m_channels.size();
        m_idx = m_idx % n;

        for (unsigned i = 0; i < n; ++i) {
            auto res = S.m_channels[m_idx]->try_get();
            m_idx = (m_idx + 1) % n;
            if (res.has_value()) {
                return res;
            }
        }
        return std::nullopt;
    }

    auto &operator>>(std::optional<payload_t> &event) {
        event = try_get();
        return *this;
    }

    // Get all events from across all channels. The events in the list returned
    // are intermixed by dequeing from each channel in round-robin order.
    std::deque<payload_t> get_all() {
        auto &S = *m_state;
        decltype(S.m_channels) channels;
        std::uint32_t idx {0};

        {
            lock_t l {S.m_mtx};

            // We need to copy all channel pointers so we can loop over the list
            // without locking the mutex. Otherwise we deadlock, since
            // some_channel.get_all() may trigger a notifier call, which will
            // then try to grab this same lock.
            channels = S.m_channels;

            idx = S.m_idx;
        }

        if (channels.empty()) {
            return {};
        }

        std::size_t n = channels.size();
        std::vector<std::deque<payload_t>> events;
        std::deque<payload_t> results;

        // get all events currently sitting in the queues
        for (std::size_t i = 0; i < n; ++i) {
            events.emplace_back(channels[i]->get_all());
        }

        // round-robin over all event channels, taking one event from
        // each channel.
        while (!events.empty()) {
            idx = idx % events.size();

            if (events[idx].empty()) {
                events.erase(events.begin() + idx);
                idx++;  // for round-robin.
                continue;
            }

            results.push_back(std::move(events[idx].front()));
            events[idx].pop_front();
            idx++;
        }

        {
            lock_t l {S.m_mtx};
            S.m_idx = idx;
        }

        return results;
    }

    // Monitor the event_aggregator for readability. The aggregator is:
    // - readable if any of the managed channels are readable
    // - unreadable if none of the managed channels are readable.
    std::uint32_t add_monitor(std::shared_ptr<notifier> notifier,
                              std::uint32_t states = chanState::READABLE) {
        if (!(states & chanState::READABLE)) {
            throw std::invalid_argument(
              "Unacceptable state specified for monitoring");
        }

        auto &S = *m_state;
        lock_t l {S.m_mtx};

        if (S.m_closed) {
            return chanState::CLOSED;
        }

        S.m_notifiers.push_back(notifier);

        if (!S.m_readable.empty()) {
            return chanState::READABLE;
        }

        return 0;
    }

    ~event_aggregator() { close(); }

    void close() {
        auto &S = *m_state;
        decltype(S.m_notifiers) notifiers;
        decltype(S.m_channels) channels;

        {
            lock_t l {S.m_mtx};
            S.m_closed = true;
            std::swap(notifiers, S.m_notifiers);
            std::swap(channels, S.m_channels);
        }

        for (auto &chan : channels) {
            chan->close();
        }

        invoke_notifiers(notifiers, chanState::CLOSED, APPLY);

        {
            lock_t l {S.m_mtx};
            S.m_channels.clear();
            S.m_index.clear();
            S.m_readable.clear();
            S.m_subscriptions.clear();
            S.m_key_map.clear();
        }
    }

    bool closed() const { return m_state->m_closed; }

private:
    static inline unsigned int m_DEFAULT_CHANCAP {100};
};

}  // namespace impl

//

// Thread-safe version of all the classes
namespace ts {
namespace interfaces = evchan::interfaces;

template<typename... types>
using event_channel =
  impl::event_channel<tarp::type_traits::thread_safe, types...>;

template<typename... types>
using event_broadcaster =
  impl::event_broadcaster<tarp::type_traits::thread_safe, types...>;

template<typename key_t, typename... types>
using event_aggregator =
  impl::event_aggregator<tarp::type_traits::thread_safe, key_t, types...>;

template<typename... types>
using event_rstream =
  impl::event_rstream<tarp::type_traits::thread_safe, types...>;

template<typename... types>
using event_wstream =
  impl::event_wstream<tarp::type_traits::thread_safe, types...>;

template<typename... types>
using trunk = impl::trunk<types...>;

using chanState = evchan::chanState;

using monitor = impl::monitor;

}  // namespace ts

//

// Thread-unsafe (i.e. non-thread-safe) version of all the classes.
namespace tu {

namespace interfaces = evchan::interfaces;

template<typename... types>
using event_channel =
  impl::event_channel<tarp::type_traits::thread_unsafe, types...>;

template<typename... types>
using event_broadcaster =
  impl::event_broadcaster<tarp::type_traits::thread_unsafe, types...>;

template<typename key_t, typename... types>
using event_aggregator =
  impl::event_aggregator<tarp::type_traits::thread_safe, key_t, types...>;

template<typename... types>
using event_rstream =
  impl::event_rstream<tarp::type_traits::thread_unsafe, types...>;

template<typename... types>
using event_wstream =
  impl::event_wstream<tarp::type_traits::thread_unsafe, types...>;

}  // namespace tu

}  // namespace evchan
}  // namespace tarp
