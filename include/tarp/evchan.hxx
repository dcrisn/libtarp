#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <limits>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <vector>

#include <iostream>

#include <tarp/cxxcommon.hxx>
#include <tarp/semaphore.hxx>
#include <tarp/type_traits.hxx>

// TODO: add note:
// the capacity of the channel should be the number of last events you are
// interested in. The channel is lossy and designed as a circular buffer
// so when the channel is full, the oldest item is forced out to make room.

//

namespace tarp {
namespace evchan {
//

#ifdef REAL
#error "#define REAL would overwrite existing definition"
#endif

#define REAL static_cast<C *>(this)

//

namespace interfaces {

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

    std::uint32_t get_id() const { return REAL->get_id(); }

    //

    void push(const types &...event_data) { return REAL->push(event_data...); }

    void push(types &&...event_data) {
        return REAL->push(std::move(event_data)...);
    }

    template<typename T>
    void push(T &&event_data) {
        return REAL->push(std::forward<T>(event_data));
    }

    //

    bool try_push(const types &...event_data) {
        return REAL->push(event_data...);
    }

    bool try_push(types &&...event_data) {
        return REAL->push(std::move(event_data)...);
    }

    template<typename T>
    bool try_push(T &&event_data) {
        return REAL->push(std::forward<T>(event_data));
    }

    //

    template<typename timepoint>
    bool try_push_until(const timepoint &abs_time, const types &...event_data) {
        return REAL->try_push_until(abs_time, event_data...);
    }

    template<typename timepoint>
    bool try_push_until(const timepoint &abs_time, types &&...event_data) {
        return REAL->try_push_until(abs_time, std::move(event_data)...);
    }

    template<typename timepoint, typename T>
    bool try_push_until(const timepoint &abs_time, T &&event_data) {
        return REAL->try_push_until(abs_time, std::forward<T>(event_data));
    }

    //

    template<class Rep, class Period>
    bool try_push_for(const std::chrono::duration<Rep, Period> &rel_time,
                      const types &...event_data) {
        return REAL->try_push_for(rel_time, event_data...);
    }

    template<class Rep, class Period>
    bool try_push_for(const std::chrono::duration<Rep, Period> &rel_time,
                      types &&...event_data) {
        return REAL->try_push_for(rel_time, std::move(event_data)...);
    }

    template<class Rep, class Period, typename T>
    bool try_push_for(const std::chrono::duration<Rep, Period> &rel_time,
                      T &&event_data) {
        return REAL->try_push_until(rel_time, std::forward<T>(event_data));
    }

    auto &operator<<(const payload_t &event) { return REAL->operator<<(event); }

    auto &operator<<(payload_t &&event) {
        return REAL->operator<<(std::move(event));
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

    std::uint32_t get_id() const { return REAL->get_id(); }

    bool empty() const { return REAL->empty(); }

    std::size_t size() const { return REAL->size(); }

    payload_t get() { return REAL->get(); }

    std::optional<payload_t> try_get() { return REAL->get(); }

    template<typename timepoint>
    std::optional<payload_t> try_get_until(const timepoint &abs_time) {
        return REAL->try_get_until(abs_time);
    }

    template<class Rep, class Period>
    std::optional<payload_t>
    try_get_for(const std::chrono::duration<Rep, Period> &rel_time) {
        return REAL->try_get_for(rel_time);
    }

    auto &operator>>(payload_t &event) { return REAL->operator>>(event); }
};

//

// Restricted interface for an event stream.
//
// A single-producer-multi-consumer (SPMC) or multi-producer-single-consumer
// (MPSC) event channel, + a thin interface layer on top.
//
// Unlike the event_broadcaster, which multiple channels can be attached to,
// forming a broadcast group, the event_stream consists of a singular channel
// that carries a stream of events, hence the name of the class.
// The stream's directionality is given by that of the underlying channel:
// an rstream uses an rchan, and a wstream uses a wchan.
//
// Unlike the event_broadcaster, since events are always enqueued to a single
// channel (the stream channel) and only dequeued from this one channel once,
// events needs not be copiable: move-only objects such as std::unique_ptr
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
// of an arbitrary number of consumers. This is since the there is a single
// channel and since the data item could be move-only (e.g. a std::unique_ptr).
// A suitable use case would be delivering tasks to a thread pool where it
// is acceptable for any one of a number of workers to get whichever task.
//   *
// NOTE: CRTP is used for interface inheritance to enable static dispatch.
template<typename T>
class event_stream {
public:
    DISALLOW_COPY_AND_MOVE(event_stream);
    event_stream<T>() = default;
    virtual ~event_stream() = default;

    auto channel() { return static_cast<T *>(this)->channel(); }
};


}  // namespace interfaces

#undef REAL

//

namespace impl {
using namespace interfaces;

//

// An event channel is a typed queue that stores data items of the specified
// type.
//   *
// A channel can be enqueued into and dequeued from. Some other basic
// operations are provided. On dequeuing/enqueueing, the specified notifier
// is signaled.
//   *
// A channel can be bounded/unbounded. If bounded, once the maximum capacity
// is reached, a subsequent enqueueing will be made room for by discarding
// the oldest data item in the channel.
//   *
// ts_policy is used to compile in/out mutex-locking calls in order to
// add or remove thread-safety.
//   *
// Channel directionality (send/write or receive/read) is achieved/enforced
// by casting the event_channel to an rchan (read channel) or wchan
// (write-channel). Note the event_channel inherits from both of those
// two interfaces using CRTP. CRTP makes it possible to use static dispatch
// thus avoiding the overhead of dynamic dispatch and dynamic binding.
// And iheriting these interfaces enables implicit conversion from an
// event_channel to either interface (upcasting does not need an explicit
// cast).
//
// NOTE: producer-consumer and/or pub-sub etc semantics are implemented
// by higher-level classes and by using the rchan & wchan interfaces.
//
// TODO: for maximum genericity, we could even intialize with a LIST of
// semaphores to be posted and use an improved tarp/semahore that
// also supports an eventfd backend etc.
template<typename ts_policy, typename... types>
class event_channel
    : public wchan<event_channel<ts_policy, types...>, types...>
    , public rchan<event_channel<ts_policy, types...>, types...> {
    //

    using is_tuple =
      typename tarp::type_traits::type_or_tuple<types...>::is_tuple;
    using mutex_t = typename tarp::type_traits::ts_types<ts_policy>::mutex_t;
    using lock_t = typename tarp::type_traits::ts_types<ts_policy>::lock_t;
    using this_type = event_channel<ts_policy, types...>;

public:
    using payload_t = tarp::type_traits::type_or_tuple_t<types...>;
    using Ts = std::tuple<types...>;

    DISALLOW_COPY_AND_MOVE(event_channel);

    // Ctor for buffered channel. When the channel is filled to its max
    // capacity, the behavior is as follows:
    //  - if circular=false, writes block until reads make room
    //  - if circular=true, each new write forces out the oldest item
    //  in the channel. IOW, the channel uses ring-buffer semantics.
    event_channel(std::uint32_t channel_capacity, bool circular)
        : m_buffered(true)
        , m_circular(circular)
        , m_channel_capacity(channel_capacity)
        , m_read_sem(channel_capacity, 0)
        , m_write_sem(channel_capacity, channel_capacity) {
        if (m_channel_capacity == 0) {
            throw std::logic_error("nonsensical max limit of 0");
        }
    }

    // Ctor for unbuffered channel. Writes block until a corresponding
    // read is done.
    event_channel()
        : m_buffered(false)
        , m_circular(false)
        , m_channel_capacity(0)
        , m_read_sem()
        , m_write_sem() {}

    // TODO: add a notifier e.g. a semaphore, condition variable etc to be
    // signaled when an event occurs: use a bit mask to specify one or more
    // events  --> channel is closed, channel is readable, channel is writable
    // etc.
    void add_notifier();

    // Get an id uniquely identifying the event channel. This is unique
    // across all instances of this class at any given time.
    std::uint32_t get_id() const { return m_id; }

    // Enqueue an event into the channel.
    // If the payload type is a tuple made up of various items,
    // this function is a convenience that allows passing discrete
    // elements for the parameters. These will be implicitly tied together
    // into a std::tuple: enqueue(a,b,c) => enqueue({a,b,c}).
    // NOTE: the other overload where std::tuple would be passed
    // explicitly will be more performant when *moving* or forwarding
    // the parametersinto the function, since it is a template.
    void push(const types &...event_data) {
        if (m_buffered) {
            push_buffered(event_data...);
            return;
        }

        push_unbuffered(event_data...);
    }

    void push(types &&...event_data) {
        if (m_buffered) {
            push_buffered(std::move(event_data...));
            return;
        }

        push_unbuffered(std::move(event_data...));
    }

    // This overload takes an actual tuple as the argument
    // -- IFF payload_t is an actual tuple (i.e. if is_tuple::value
    // is true).
    template<typename T>
    void push(T &&event_data) {
        static_assert(
          std::is_same_v<std::remove_reference_t<T>, payload_t> ||
          std::is_convertible_v<std::remove_reference<T>, payload_t>);

        if (m_buffered) {
            push_buffered(std::forward<T>(event_data));
            return;
        }

        push_unbuffered(std::forward<T>(event_data));
    }

    //

    // Enqueue an event into the channel.
    // If the payload type is a tuple made up of various items,
    // this function is a convenience that allows passing discrete
    // elements for the parameters. These will be implicitly tied together
    // into a std::tuple: enqueue(a,b,c) => enqueue({a,b,c}).
    // NOTE: the other overload where std::tuple would be passed
    // explicitly will be more performant when *moving* or forwarding
    // the parametersinto the function, since it is a template.
    void try_push(const types &...event_data) {
        try_push_until(m_past_tp, event_data...);
    }

    void try_push(types &&...event_data) {
        try_push_until(m_past_tp, event_data...);
    }

    template<typename T>
    void try_push(T &&event_data) {
        static_assert(
          std::is_same_v<std::remove_reference_t<T>, payload_t> ||
          std::is_convertible_v<std::remove_reference<T>, payload_t>);
        try_push_until(m_past_tp, std::forward<T>(event_data));
    }

    //

    // Enqueue an event into the channel.
    // If the payload type is a tuple made up of various items,
    // this function is a convenience that allows passing discrete
    // elements for the parameters. These will be implicitly tied together
    // into a std::tuple: enqueue(a,b,c) => enqueue({a,b,c}).
    // NOTE: the other overload where std::tuple would be passed
    // explicitly will be more performant when *moving* or forwarding
    // the parametersinto the function, since it is a template.
    template<typename timepoint>
    bool try_push_until(const timepoint &abs_time, const types &...event_data) {
        if (m_buffered) {
            return try_push_buffered_until(abs_time, event_data...);
        }

        return try_push_unbuffered_until(abs_time, event_data...);
    }

    template<typename timepoint>
    bool try_push_until(const timepoint &abs_time, types &&...event_data) {
        if (m_buffered) {
            return try_push_buffered_until(abs_time, std::move(event_data)...);
        }

        return try_push_unbuffered_until(abs_time, std::move(event_data)...);
    }

    template<typename timepoint, typename T>
    bool try_push_until(const timepoint &abs_time, T &&event_data) {
        static_assert(
          std::is_same_v<std::remove_reference_t<T>, payload_t> ||
          std::is_convertible_v<std::remove_reference<T>, payload_t>);

        if (m_buffered) {
            return try_push_buffered_until(abs_time,
                                           std::forward<T>(event_data));
        }

        return try_push_unbuffered_until(abs_time, std::forward<T>(event_data));
    }

    //
    template<class Rep, class Period>
    bool try_push_for(const std::chrono::duration<Rep, Period> &rel_time,
                      const types &...event_data) {
        if (m_buffered) {
            return try_push_buffered_for(rel_time, event_data...);
        }
        return try_push_unbuffered_for(rel_time, event_data...);
    }

    template<class Rep, class Period>
    bool try_push_for(const std::chrono::duration<Rep, Period> &rel_time,
                      types &&...event_data) {
        if (m_buffered) {
            return try_push_buffered_for(rel_time, std::move(event_data)...);
        }
        return try_push_unbuffered_for(rel_time, std::move(event_data)...);
    }

    template<class Rep, class Period, typename T>
    bool try_push_for(const std::chrono::duration<Rep, Period> &rel_time,
                      T &&event_data) {
        static_assert(
          std::is_same_v<std::remove_reference_t<T>, payload_t> ||
          std::is_convertible_v<std::remove_reference<T>, payload_t>);

        if (m_buffered) {
            return try_push_buffered_for(rel_time, std::forward<T>(event_data));
        }
        return try_push_unbuffered_for(rel_time, std::forward<T>(event_data));
    }

    // True if there are no queued items.
    bool empty() const {
        lock_t l {m_mtx};
        return m_event_data_items.empty();
    }

    // Return the number of events currently enqueued.
    std::size_t size() const {
        lock_t l {m_mtx};
        return m_event_data_items.size();
    }

    void clear() {
        lock_t l {m_mtx};
        m_event_data_items.clear();
        m_read_sem.reset();
        m_write_sem.reset();
    }

    // Return a std::optional result; this is empty if there is
    // nothing in the channel; otherwise it's the oldest data item
    // in the channel. If the data item will be copied into the
    // optional if copiable else moved if movable.
    std::optional<payload_t> try_get() {
        if (m_buffered) {
            return try_get_buffered();
        }

        return try_get_unbuffered();
    }

    template<typename timepoint>
    std::optional<payload_t> try_get_until(const timepoint &abs_time) {
        if (m_buffered) {
            return try_get_buffered_until(abs_time);
        }
        return try_get_unbuffered(abs_time);
    }

    template<class Rep, class Period>
    std::optional<payload_t>
    try_get_for(const std::chrono::duration<Rep, Period> &rel_time) {
        if (m_buffered) {
            return try_get_buffered_for(rel_time);
        }
        return try_get_unbuffered_for(rel_time);
    }

    payload_t get() {
        std::optional<payload_t> res;
        m_read_sem.acquire();
        res = try_get();

        if (!res.has_value()) {
            throw std::logic_error("BUG: semaphore acquired, but no resource.");
        }

        return std::move(res.value());
    }

    // Return a deque of all data items currently in the channel.
    // The data items are removed from the channel.
    std::deque<payload_t> get_all() {
        lock_t l {m_mtx};
        decltype(m_event_data_items) events;
        std::swap(events, m_event_data_items);
        m_read_sem.reset();
        m_write_sem.reset();
        return events;
    }

    // More convenient and expressive overloads for enqueuing
    // and dequeueing an element.
    auto &operator<<(const payload_t &event) {
        push(event);
        return *this;
    }

    auto &operator<<(payload_t &&event) {
        push(std::move(event));
        return *this;
    }

    auto &operator>>(payload_t &event) {
        if constexpr (((std::is_copy_assignable_v<types> ||
                        std::is_copy_constructible_v<types>) &&
                       ...)) {
            event = get();
        } else {
            event = std::move(get());
        }
        return *this;
    }

private:
    template<bool buffered,
             typename buffered_func,
             typename unbuffered_func,
             typename... vargs>
    auto call_buffered_or_unbuffered(buffered_func &&buffered_f,
                                     unbuffered_func &&unbuffered_f,
                                     vargs &&...params) {
        if constexpr (buffered) {
            return buffered_f(std::forward<vargs>(params)...);
        }
        return unbuffered_f(std::forward<vargs>(params)...);
    }

    // A push is only possible when there is a corresponding get().
    // Block waiting until that condition is satisfied.
    template<typename... vargs>
    void push_unbuffered(vargs &&...event_data) {
        m_write_sem.acquire();
        do_push_unbuffered(std::forward<vargs>(event_data)...);
    }

    // If there is a corresponding get(), perform the push immediately
    // and return true. Else, wait until at most abs_time for that
    // condition to be satisfied and return true if it was (and the
    // push happened), otherwise false.
    template<typename timepoint, typename... vargs>
    bool try_push_unbuffered_until(const timepoint &abs_time,
                                   vargs &&...event_data) {
        std::cerr
          << "Trying to acquire write semaphore in try_push_buffered_until"
          << std::endl;

        if (!m_write_sem.try_acquire_until(abs_time)) {
            std::cerr
              << "Failed to acquire write semaphore in try_push_buffered_until"
              << std::endl;

            return false;
        }

        std::cerr
          << "Managed to acquire write semaphore in try_push_buffered_until"
          << std::endl;

        do_push_unbuffered(std::forward<vargs>(event_data)...);
        return true;
    }

    // See try_push_unbuffered_until. This is a wrapper so a duration
    // can be passed instead of an time point.
    template<class Rep, class Period, typename... vargs>
    bool
    try_push_unbuffered_for(const std::chrono::duration<Rep, Period> &rel_time,
                            vargs &&...event_data) {
        return try_push_unbuffered_until(std::chrono::steady_clock::now() +
                                           rel_time,
                                         std::forward<vargs>(event_data)...);
    }

    // This must only be called when m_write_sem has been acquired.
    template<typename... vargs>
    void do_push_unbuffered(vargs &&...event_data) {
        lock_t l {m_mtx};

        // if the payload is made up of multiple elements, treat it as a
        // tuple. if constexpr (tarp::type_traits::is_tuple_v<types...>)
        // {
        if constexpr (is_tuple::value) {
            m_event_data_items.emplace_back(
              std::make_tuple(std::forward<vargs>(event_data)...));
        }

        // else it is a single element
        else {
            m_event_data_items.emplace_back(std::forward<vargs>(event_data)...);
        }

        // unblock a get().
        m_read_sem.release();
    }

    // tuple overload.
    template<typename T>
    void do_push_unbuffered(T &&event_data) {
        {
            lock_t l {m_mtx};
            m_event_data_items.emplace_back(std::forward<T>(event_data));
        }
        m_read_sem.release();
    }

    std::optional<payload_t> do_try_get(bool throw_if_empty) {
        lock_t l {m_mtx};

        if (throw_if_empty && m_event_data_items.empty()) {
            throw std::logic_error(
              "BUG: channel unexpectedly empty, failed get()");
        }

        std::optional<payload_t> data;

        // this is to cover both copy-assignable/copy-construcible and
        // move-only semantics objects. Either copying or moving must be
        // supported, otherwise the channel cannot be used.
        // The data items are removed from the channel.
        if constexpr (((std::is_copy_assignable_v<types> ||
                        std::is_copy_constructible_v<types>) &&
                       ...)) {
            data = m_event_data_items.front();
        } else {
            data = std::move(m_event_data_items.front());
        }

        m_event_data_items.pop_front();
        return data;
    }

    template<typename timepoint>
    std::optional<payload_t>
    try_get_unbuffered_until(const timepoint &abs_time) {
        // unblock push()
        std::cerr << "Releasing m_write_sem" << std::endl;
        m_write_sem.release();

        std::this_thread::yield();

        // if push actually happens.
        std::cerr << "trying to acquire read_sem" << std::endl;
        if (m_read_sem.try_acquire_until(abs_time)) {
            std::cerr << "managed to acquire read_sem" << std::endl;
            return do_try_get(true);
        }

        std::cerr << "failed to acquire read_sem" << std::endl;

        // else if no push happened, decrement m_write_sem
        // so no push actually ever happens, until we make
        // *another* get() call -- since we are **unbuffered**.

        std::cerr << "trying to decrement m_write_sem" << std::endl;
        if (m_write_sem.try_acquire()) {
            std::cerr << "managed to decrement m_write_sem" << std::endl;
            return std::nullopt;
        }

        std::cerr << "failed to decrement m_write_sem" << std::endl;

        // However, a push could've happened: that is the case
        // if we failed to acquire m_write_sem above (a push() ,
        // beat us to it).
        m_read_sem.acquire();
        return do_try_get(true);
    }

    template<class Rep, class Period>
    std::optional<payload_t>
    try_get_unbuffered_for(const std::chrono::duration<Rep, Period> &rel_time) {
        return try_get_unbuffered_until(std::chrono::steady_clock::now() +
                                        rel_time);
    }

    // Return a std::optional result; this is empty if there is
    // nothing in the channel; otherwise it's the oldest data item
    // in the channel. If the data item will be copied into the
    // optional if copiable else moved if movable.
    std::optional<payload_t> try_get_unbuffered() {
        return try_get_unbuffered_until(m_past_tp);
    }

    payload_t get_unbuffered() {
        std::optional<payload_t> res;

        constexpr std::chrono::seconds WAIT_TIME {100};

        while (!res.has_value()) {
            res = try_get_unbuffered_for(WAIT_TIME);
        }

        return res.value();
    }

    void throw_on_empty() const {
        lock_t l {m_mtx};
        if (m_event_data_items.empty()) {
            throw std::logic_error("BUG: channel unexpectedly empty");
        }
    }

    void throw_on_not_empty() const {
        lock_t l {m_mtx};
        if (!m_event_data_items.empty()) {
            throw std::logic_error("BUG: channel unexpectedly non-empty");
        }
    }

    //
    //
    //
    //

    // A push is only possible when there is a corresponding get().
    // Block waiting until that condition is satisfied.
    template<typename... vargs>
    void push_buffered(vargs &&...event_data) {
        if (!m_circular) {
            m_write_sem.acquire();
            lock_t l {m_mtx};
            do_push_buffered(l, std::forward<vargs>(event_data)...);
            return;
        }

        lock_t l {m_mtx};

        if (m_event_data_items.size() >= m_channel_capacity) {
            m_event_data_items.pop_front();
            do_push_buffered(l, std::forward<vargs>(event_data)...);
            m_event_data_items.emplace_back(std::forward<vargs>(event_data)...);
            return;
        }

        if (!m_write_sem.try_acquire()) {
            throw std::logic_error(
              "BUG: unexpectedly failed to acquire m_write_sem");
        }

        do_push_buffered(l, std::forward<vargs>(event_data)...);
        m_read_sem.release();
        return;
    }

    template<typename timepoint, typename... vargs>
    bool try_push_buffered_until(const timepoint &abs_time,
                                 vargs &&...event_data) {
        if (m_read_sem.try_acquire_until(abs_time)) {
            lock_t l {m_mtx};
            if (m_event_data_items.size() >= m_channel_capacity) {
                throw std::logic_error(
                  "BUG: got semaphore, but max buffsz reached");
            }
            do_push_buffered(l, std::forward<vargs>(event_data)...);
            m_read_sem.release();
            return true;
        }

        if (!m_circular) {
            return false;
        }

        lock_t l {m_mtx};
        do_push_buffered(l, std::forward<vargs>(event_data)...);
        m_event_data_items.pop_front();
        if (!m_event_data_items.size() < m_channel_capacity) {
            throw std::logic_error(
              "BUG: failed to acquire semaphore even though buffsize < max");
        }
        return true;
    }

    // See try_push_unbuffered_until. This is a wrapper so a duration
    // can be passed instead of an time point.
    template<class Rep, class Period, typename... vargs>
    bool
    try_push_buffered_for(const std::chrono::duration<Rep, Period> &rel_time,
                          vargs &&...event_data) {
        return try_push_buffered_until(std::chrono::steady_clock::now() +
                                         rel_time,
                                       std::forward<vargs>(event_data)...);
    }

    // This must only be called when m_write_sem has been acquired.
    template<typename... vargs>
    void do_push_buffered(lock_t &, vargs &&...event_data) {
        // if the payload is made up of multiple elements, treat it as a
        // tuple. if constexpr (tarp::type_traits::is_tuple_v<types...>)
        // {
        if constexpr (is_tuple::value) {
            m_event_data_items.emplace_back(
              std::make_tuple(std::forward<vargs>(event_data)...));
        }

        // else it is a single element
        else {
            m_event_data_items.emplace_back(std::forward<vargs>(event_data)...);
        }
    }

    // tuple overload.
    template<typename T>
    void do_push_buffered(lock_t &, T &&event_data) {
        m_event_data_items.emplace_back(std::forward<T>(event_data));
    }

    //

    template<typename timepoint>
    std::optional<payload_t> try_get_buffered_until(const timepoint &abs_time) {
        if (!m_read_sem.try_acquire_until(abs_time)) {
            return std::nullopt;
        }

        auto res = do_try_get(true);
        m_write_sem.release();
        return res;
    }

    template<class Rep, class Period>
    std::optional<payload_t>
    try_get_buffered_for(const std::chrono::duration<Rep, Period> &rel_time) {
        return try_get_unbuffered_until(std::chrono::steady_clock::now() +
                                        rel_time);
    }

    std::optional<payload_t> try_get_buffered() {
        return try_get_buffered_until(m_past_tp);
    }

    payload_t get_buffered() {
        std::optional<payload_t> res;

        constexpr std::chrono::seconds WAIT_TIME {100};

        while (!res.has_value()) {
            res = try_get_buffered_for(WAIT_TIME);
        }

        return res.value();
    }

private:
    static inline std::atomic<std::uint32_t> m_next_event_buffer_id = 0;

    const std::uint32_t m_id = m_next_event_buffer_id++;
    const bool m_buffered = false;
    const bool m_circular = false;
    const std::uint32_t m_channel_capacity = 0;

    using CLOCK = std::chrono::steady_clock;
    const CLOCK::time_point m_past_tp {CLOCK::now()};

    mutable mutex_t m_mtx;
    std::deque<payload_t> m_event_data_items;

    // std::shared_ptr<tarp::binary_semaphore> m_event_sink_notifier;
    //  add here m_notifiers.

    tarp::semaphore m_write_sem;
    tarp::semaphore m_read_sem;
};

//

template<typename... types>
class trunk {
    //
    using is_tuple =
      typename tarp::type_traits::type_or_tuple<types...>::is_tuple;
    using this_type = event_channel<types...>;
    using lock_t = std::unique_lock<std::mutex>;
    using mutex_t = std::mutex;
    using payload_type = tarp::type_traits::type_or_tuple_t<types...>;

    struct operation {
        template<typename... T>
        operation(T &&...d) {
            if constexpr (type_traits::is_tuple_v<T...>) {
                data.emplace(std::make_tuple(std::forward<T>(d)...));
            } else {
                data.emplace(std::forward<T>(d)...);
            }
        }

        operation() = default;

        std::optional<payload_type> data;
        bool done = false;
    };

public:
    using payload_t = payload_type;
    using Ts = std::tuple<types...>;

    DISALLOW_COPY_AND_MOVE(trunk);
    trunk() = default;

    template<typename... T>
    auto &operator<<(T &&...data) {
        push(std::forward<T>(data)...);
        return *this;
    }

    auto &operator>>(std::optional<payload_t> &event) {
        event.emplace(get());
        return *this;
    }

    void close() {
        lock_t l {m_mtx};
        m_closed = true;
        m_recvq.clear();
        m_sendq.clear();
        m_send_cond_var.notify_all();
        m_recv_cond_var.notify_all();
    }

    template<typename... T>
    bool push(T &&...data) {
        return do_try_push_until(m_past_tp, false, std::forward<T>(data)...);
    }

    template<typename... T>
    bool try_push(T &&...data) {
        lock_t l {m_mtx};

        if (m_closed) {
            return false;
        }

        if (!m_recvq.empty()) {
            pass_data(l, std::forward<T>(data)...);
            return true;
        }

        return false;
    }

    template<typename timepoint, typename... T>
    bool try_push_until(const timepoint &abs_time, T &&...data) {
        return do_try_push_until(abs_time, true, std::forward<T>(data)...);
    }

    template<class Rep, class Period, typename... T>
    bool try_push_for(const std::chrono::duration<Rep, Period> &rel_time,
                      T &&...data) {
        auto deadline = CLOCK::now() + rel_time;
        return try_push_until(deadline, std::forward<T>(data)...);
    }

    std::optional<payload_t> get() {
        return do_try_get_until(m_past_tp, false);
    }

    std::optional<payload_t> try_get() {
        lock_t l {m_mtx};

        if (m_closed) {
            return std::nullopt;
        }

        if (!m_sendq.empty()) {
            return get_data(l);
        }

        std::cerr << "try_get(): m_sendq empty, impossible to get" << std::endl;
        return std::nullopt;
    }

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
    template<typename timepoint, typename... T>
    bool do_try_push_until(const timepoint &abs_time,
                           bool use_deadline,
                           T &&...data) {
        lock_t l {m_mtx};
        std::cerr << "try_push got mutex " << std::endl;

        if (m_closed) {
            return false;
        }

        if (!m_recvq.empty()) {
            pass_data(l, std::forward<T>(data)...);
            return true;
        }

        struct operation op(std::forward<T>(data)...);
        add_sender(l, op);

        auto check = [this] {
            return true;
        };

        while (true) {
            // std::cerr << "[PUSH] will wait on condvar" << std::endl;
            if (use_deadline) {
                // m_send_cond_var.wait_until(l, abs_time, check);
                m_send_cond_var.wait_until(l, abs_time);
            } else {
                // m_send_cond_var.wait(l, check);
                m_send_cond_var.wait(l);
            }
            // std::cerr << "[PUSH] after wait on condvar" << std::endl;

            if (op.done) {
                return true;
            }

            if (m_closed) {
                remove_sender(l, op);
                return false;
            }

            if (use_deadline && abs_time <= CLOCK::now()) {
                remove_sender(l, op);
                return false;
            }
        }
    }

    template<typename timepoint>
    std::optional<payload_t> do_try_get_until(const timepoint &abs_time,
                                              bool use_deadline) {
        lock_t l {m_mtx};
        std::cerr << "Get got mutex" << std::endl;

        if (m_closed) {
            return std::nullopt;
        }

        if (!m_sendq.empty()) {
            return get_data(l);
        }

        struct operation op;
        add_receiver(l, op);

        auto check = [this] {
            return true;
        };

        while (true) {
            // std::cerr << "[GET] will wait on condvar" << std::endl;
            if (use_deadline) {
                // m_recv_cond_var.wait_until(l, abs_time, check);
                m_recv_cond_var.wait_until(l, abs_time);
            } else {
                // m_recv_cond_var.wait(l, check);
                m_recv_cond_var.wait(l);
            }
            // std::cerr << "[GET] after wait on condvar" << std::endl;

            if (op.done) {
                return std::move(op.data);
            }

            if (m_closed) {
                remove_receiver(l, op);
                return std::nullopt;
            }

            if (use_deadline && abs_time < CLOCK::now()) {
                remove_receiver(l, op);
                return std::nullopt;
            }
        }
    }

    template<typename... T>
    void pass_data(lock_t &, T &&...data) {
        struct operation &receiver = m_recvq.front();
        receiver.data.emplace(std::forward<T>(data)...);
        receiver.done = true;
        m_recvq.pop_front();
        m_recv_cond_var.notify_all();
    }

    std::optional<payload_t> get_data(lock_t &) {
        struct operation &sender = m_sendq.front();
        auto data = std::move(sender.data);
        sender.done = true;
        m_sendq.pop_front();
        m_send_cond_var.notify_all();
        return data;
    }

    void add_receiver(lock_t &, struct operation &op) {
        m_recvq.emplace_back(op);
    }

    void add_sender(lock_t &, struct operation &op) {
        m_sendq.emplace_back(op);
    }

    void remove_sender(lock_t &, struct operation &op) {
        m_sendq.remove_if([&op](auto &i) {
            return std::addressof(i.get()) == std::addressof(op);
        });
        std::cerr << "sender removed" << std::endl;
    }

    void remove_receiver(lock_t &, struct operation &op) {
        m_recvq.remove_if([&op](auto &i) {
            return std::addressof(i.get()) == std::addressof(op);
        });
        std::cerr << "receiver removed" << std::endl;
    }

private:
    using CLOCK = std::chrono::steady_clock;
    const CLOCK::time_point m_past_tp {CLOCK::now()};

    mutable mutex_t m_mtx;
    bool m_closed {false};

    // std::shared_ptr<tarp::binary_semaphore> m_event_sink_notifier;
    //  add here m_notifiers.

    std::condition_variable m_send_cond_var;
    bool m_senders_signaled {false};

    std::condition_variable m_recv_cond_var;
    bool m_receivers_signaled {false};

    std::list<std::reference_wrapper<struct operation>> m_sendq;
    std::list<std::reference_wrapper<struct operation>> m_recvq;
};

//

// A single-producer multi-consumer (SPMC) event dispatcher.
//
// This can employed as a rudimentary publish-subscribe interface,
// i.e. the asynchronous counterpart to the observer (aka
// signals&slots) design pattern. The 'subscribers' can attach
// channels to the broadcaster, thus forming a broadcast group.
// An event enqueued to the broadcaster will then be written
// to every single attached channel.
//
// A class embedding an event_broadcaster therefore becomes
// an event publisher; the 'topic'/event is defined by whatever
// name is given to it by the publisher and by the data type
// of the event channel.
//
// NOTE that by definition the 'event' i.e. the data item
// must be copyable, since one event is cloned to be written
// to an arbitrary number of connected channels.
template<typename ts_policy, typename... types>
class event_broadcaster {
    static_assert(((std::is_copy_assignable_v<types> ||
                    std::is_copy_constructible_v<types>) &&
                   ...));
    using lock_t = typename tarp::type_traits::ts_types<ts_policy>::lock_t;
    using mutex_t = typename tarp::type_traits::ts_types<ts_policy>::mutex_t;
    using event_channel_t = event_channel<ts_policy, types...>;
    using wchan_t = wchan<event_channel_t, types...>;

public:
    DISALLOW_COPY_AND_MOVE(event_broadcaster);

    // If autodispatch is enabled, then any event pushed to the
    // brodacaster is immediately broadcast. OTOH if autodispatch=false,
    // events are buffered into a first-stage queue and only get
    // broadcast when .dispatch() is invoked.
    event_broadcaster(bool autodispatch = false)
        : m_autodispatch(autodispatch) {}

    // Enqueue an event, pending broadcast.
    template<typename... event_data_t>
    void push(event_data_t &&...data) {
        m_event_buffer.push(std::forward<event_data_t>(data)...);

        // if autodispatch is enabled, then always flush immediately: do
        // not buffer.
        if (m_autodispatch) {
            dispatch();
        }
    }

    // Flush the event buffer: broadcast all buffered events.
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

    // More convenient and expressive overloads for enqueuing
    // and dequeueing an element.

    template<typename... event_data_t>
    auto &operator<<(event_data_t &&...event_data) {
        push(std::forward<event_data_t>(event_data)...);
        return *this;
    }

    // Get the number of channels connected. This includes dangling
    // channels that are no longer alive.
    std::size_t num_channels() const {
        lock_t l {m_mtx};
        return m_event_channels.size();
    }

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

#if 0
NOTE: the streamer returns a channel but there is no form of efficient synchrnoization. I.e. no semaphore gets incremented. TODO think about this: does the caller provide a semaphore? Do we keep a vector of weak semaphore pointers? Do we RETURN a semaphore?
#endif

//

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
    event_rstream(bool autoflush = false, std::size_t channel_capacity = 100)
        : m_autoflush(autoflush)
        , m_channel_capacity(channel_capacity)
        , m_event_buffer(channel_capacity, true) {}

    // Enqueue an event to the stream channel. This buffers the event
    // until flush() is called, or if autoflush=true, it dispatches the
    // event immediately.
    template<typename... event_data_t>
    void push(event_data_t &&...data) {
        // if autoflush enabled, then do not buffer.
        if (m_autoflush) {
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

    template<typename... event_data_t>
    auto &operator<<(event_data_t &&...event_data) {
        push(std::forward<event_data_t>(event_data)...);
        return *this;
    }

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
    // the stream channel.
    auto &interface() {
        return static_cast<interfaces::event_stream<this_type> &>(*this);
    }

private:
    template<typename C, typename... T>
    void push_to_channel(C &chan, T &&...data) {
        chan.push(std::forward<T>(data)...);
    }

    mutable mutex_t m_mtx;
    const bool m_autoflush {false};
    const std::size_t m_channel_capacity;
    event_channel_t m_event_buffer;
    std::weak_ptr<event_channel_t> m_stream_channel;
};

//

template<typename ts_policy, typename... types>
class event_wstream
    : public interfaces::event_stream<event_wstream<ts_policy, types...>> {
    //
    using event_channel_t = event_channel<ts_policy, types...>;
    using event_wchan_t = wchan<event_channel_t, types...>;
    using payload_t = typename event_channel_t::payload_t;
    using this_type = event_rstream<ts_policy, types...>;
    //
public:
    // TODO: take notifier, propagate it to channel.
    event_wstream(std::size_t chancap = 100) {
        m_stream_channel = std::make_shared<event_channel_t>(chancap, true);
    }

    std::optional<payload_t> try_get() { return m_stream_channel->try_get(); }

    payload_t get() { return m_stream_channel->get(); }

    std::deque<payload_t> get_all() { return m_stream_channel->get_all(); }

    auto &operator>>(payload_t &event) {
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

private:
    std::shared_ptr<event_channel_t> m_stream_channel;
};

//
// dequeue()
// get_channel (and associated it with a label in a lookup dict)
// remove_channel
// associate it with a semaphore that gets posted when ANY or ALL
// of the channels have items enqueued in them.

template<typename ts_policy, typename key_t, typename... types>
class event_aggregator {
    //
    using lock_t = typename tarp::type_traits::ts_types<ts_policy>::lock_t;
    using mutex_t = typename tarp::type_traits::ts_types<ts_policy>::mutex_t;
    using event_channel_t = event_channel<ts_policy, types...>;
    using event_wchan_t = wchan<event_channel_t, types...>;
    using payload_t = typename event_channel_t::payload_t;
    using this_type = event_rstream<ts_policy, types...>;
    //
public:
    DISALLOW_COPY_AND_MOVE(event_aggregator);

    event_aggregator() {};

    std::shared_ptr<event_wchan_t>
    channel(const key_t &k, unsigned int chancap = m_DEFAULT_CHANCAP) {
        lock_t l {m_mtx};

        auto found = m_index.find(k);
        if (found != m_index.end()) {
            auto chan = found->second.lock();
            if (chan) {
                return chan;
            }
        }

        auto chan = std::make_shared<event_channel_t>(chancap, true);
        m_index[k] = chan;
        m_channels.push_back(chan);
        return chan;
    }

    void remove(const key_t &k) {
        lock_t l {m_mtx};

        std::shared_ptr<event_channel_t> chan;

        auto found = m_index.find(k);
        if (found != m_index.end()) {
            chan = found->second.lock();
            m_index.erase(found);
        }

        if (!chan) {
            return;
        }

        for (auto it = m_channels.begin(); it != m_channels.end();) {
            if (*it == chan) {
                it = m_channels.erase(it);
                continue;
            }
            ++it;
        }
    }

    std::optional<payload_t> try_get() {
        lock_t l {m_mtx};
        if (m_channels.empty()) {
            return std::nullopt;
        }

        std::size_t n = m_channels.size();
        m_idx = m_idx % n;

        for (unsigned i = 0; i < n; ++i) {
            auto res = m_channels[m_idx]->try_get();
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

    std::deque<payload_t> get_all() {
        lock_t l {m_mtx};

        if (m_channels.empty()) {
            return {};
        }

        std::size_t n = m_channels.size();
        std::vector<std::deque<payload_t>> events;
        std::deque<payload_t> results;

        // get all events currently sitting in the queues
        for (std::size_t i = 0; i < n; ++i) {
            events.emplace_back(m_channels[i].get_all());
        }

        m_idx = m_idx % n;

        // round-robin over all event channels, taking one event from
        // each channel.
        while (!events.empty()) {
            auto i = m_idx;

            if (events[i].empty()) {
                events.erase(events.begin() + i);
                m_idx %= n;
                continue;
            }

            results.emplace_back(std::move(events.front()));
            events.pop_front();
            m_idx = (m_idx + 1) % n;
        }

        return results;
    }

private:
    static inline unsigned int m_DEFAULT_CHANCAP {100};
    mutable std::mutex m_mtx;
    std::size_t m_idx {0};
    std::vector<std::shared_ptr<event_channel_t>> m_channels;
    std::unordered_map<key_t, std::weak_ptr<event_channel_t>> m_index;
};

}  // namespace impl

//

// Thread-safe version of all the classes
namespace ts {
template<typename... types>
using event_channel =
  impl::event_channel<tarp::type_traits::thread_safe, types...>;

template<typename... types>
using event_broadcaster =
  impl::event_channel<tarp::type_traits::thread_safe, types...>;

template<typename key_t, typename... types>
using event_aggregator =
  impl::event_aggregator<tarp::type_traits::thread_safe, key_t, types...>;

template<typename... types>
using event_rstream =
  impl::event_rstream<tarp::type_traits::thread_safe, types...>;

template<typename... types>
using event_wstream =
  impl::event_wstream<tarp::type_traits::thread_safe, types...>;
}  // namespace ts

//

// Thread-unsafe (i.e. non-thread-safe) version of all the classes.
namespace tu {
template<typename... types>
using event_channel =
  impl::event_channel<tarp::type_traits::thread_unsafe, types...>;

template<typename... types>
using event_broadcaster =
  impl::event_channel<tarp::type_traits::thread_unsafe, types...>;

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
