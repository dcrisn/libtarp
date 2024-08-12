#pragma once

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
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

    void enqueue(const types &...event_data) {
        return REAL->enqueue(event_data...);
    }

    template<typename T>
    void enqueue(T &&event_data) {
        return REAL->enqueue(std::forward<T>(event_data));
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

    std::optional<payload_t> get() { return REAL->get(); }

    std::deque<payload_t> get_all() { return REAL->get_all(); }

    auto &operator>>(std::optional<payload_t> &event) {
        return REAL->operator>>(event);
    }
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

    //
    void enqueue_do_misc() {
        // If there is an upper limit to the capacity of the channel,
        // we discard the oldest entry to make room. Conversely, we could
        // block instead and wait until there is room, but this is typically
        // unacceptable, since a slow consumer may this way inadvertently
        // cause a DOS for all and any other consumers.
        if (m_max_buffsz.has_value()) {
            if (m_event_data_items.size() > *m_max_buffsz) {
                // discard oldest
                m_event_data_items.pop_front();
            }
        }

        // If a semaphore was specified, post it.
        if (m_event_sink_notifier) {
            m_event_sink_notifier->release();
        }
    }

public:
    using payload_t = tarp::type_traits::type_or_tuple_t<types...>;

    DISALLOW_COPY_AND_MOVE(event_channel);

    // NOTE: semaphore can be null if not necessary e.g. if the
    // channel is merely used as a temporary thread-safe buffer and
    // no notification is needed.
    event_channel(std::optional<std::uint32_t> max_buffer_size = std::nullopt,
                  std::shared_ptr<tarp::binary_semaphore> sem = nullptr)
        : m_max_buffsz(max_buffer_size), m_event_sink_notifier(sem) {
        if (m_max_buffsz.has_value() && *m_max_buffsz == 0) {
            throw std::logic_error("nonsensical max limit of 0");
        }
    }

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
    void enqueue(const types &...event_data) {
        lock_t l {m_mtx};

        // if the payload is made up of multiple elements, treat it as a tuple.
        // if constexpr (tarp::type_traits::is_tuple_v<types...>) {
        if constexpr (is_tuple::value) {
            m_event_data_items.push_back(std::make_tuple(event_data...));
        }

        // else it is a single element
        else {
            m_event_data_items.push_back(event_data...);
        }

        enqueue_do_misc();
    }

    // This overload takes an actual tuple as the argument
    // -- IFF payload_t is an actual tuple (i.e. if is_tuple::value
    // is true).
    template<typename T>
    void enqueue(T &&event_data) {
        static_assert(
          std::is_same_v<std::remove_reference_t<T>, payload_t> ||
          std::is_convertible_v<std::remove_reference<T>, payload_t>);

        lock_t l {m_mtx};
        m_event_data_items.emplace_back(std::forward<T>(event_data));
        enqueue_do_misc();
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

    // Return a std::optional result; this is empty if there is
    // nothing in the channel; otherwise it's the oldest data item
    // in the channel. If the data item will be copied into the
    // optional if copiable else moved if movable.
    std::optional<payload_t> get() {
        lock_t l {m_mtx};
        if (m_event_data_items.empty()) {
            return std::nullopt;
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

    // Return a deque of all data items currently in the channel.
    // The data items are removed from the channel.
    std::deque<payload_t> get_all() {
        lock_t l {m_mtx};
        decltype(m_event_data_items) events;
        std::swap(events, m_event_data_items);
        return events;
    }

    // More convenient and expressive overloads for enqueuing
    // and dequeueing an element.
    auto &operator<<(const payload_t &event) {
        enqueue(event);
        return *this;
    }

    auto &operator<<(payload_t &&event) {
        enqueue(std::move(event));
        return *this;
    }

    auto &operator>>(std::optional<payload_t> &event) {
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
    static inline std::atomic<std::uint32_t> m_next_event_buffer_id {0};

    const std::uint32_t m_id {m_next_event_buffer_id++};
    const std::optional<std::uint32_t> m_max_buffsz;

    mutable mutex_t m_mtx;
    std::deque<payload_t> m_event_data_items;

    std::shared_ptr<tarp::binary_semaphore> m_event_sink_notifier;
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

public:
    DISALLOW_COPY_AND_MOVE(event_broadcaster);

    // If autodispatch is enabled, then any event pushed to the brodacaster
    // is immediately broadcast. OTOH if autodispatch=false, events are
    // buffered into a first-stage queue and only get broadcast when
    // .dispatch() is invoked.
    event_broadcaster(bool autodispatch = false)
        : m_autodispatch(autodispatch) {}

    // Enqueue an event, pending broadcast.
    template<typename... event_data_t>
    void enqueue(event_data_t &&...data) {
        m_event_buffer.enqueue(std::forward<event_data_t>(data)...);

        // if autodispatch is enabled, then always flush immediately: do not
        // buffer.
        if (m_autodispatch) {
            dispatch();
        }
    }

    // Flush the event buffer: broadcast all buffered events.
    void dispatch() {
        lock_t l {m_mtx};

        std::vector<std::shared_ptr<event_channel_t>> channels;

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
                channels[i]->enqueue(event);
            }
        }
    }

    // More convenient and expressive overloads for enqueuing
    // and dequeueing an element.

    template<typename... event_data_t>
    auto &operator<<(event_data_t &&...event_data) {
        enqueue(std::forward<event_data_t>(event_data)...);
        return *this;
    }

    // Get the number of channels connected. This includes dangling
    // channels that are no longer alive.
    std::size_t num_channels() const {
        lock_t l {m_mtx};
        return m_event_channels.size();
    }

    void connect(std::weak_ptr<event_channel<types...>> channel) {
        lock_t l {m_mtx};
        m_event_channels.push_back(channel);
    }

private:
    mutable mutex_t m_mtx;
    const bool m_autodispatch {false};
    event_channel_t m_event_buffer;
    std::vector<std::weak_ptr<event_channel_t>> m_event_channels;
};

//

#if 0
NOTE: the streamer returns a channel but there is no form of efficient synchrnoization. I.e. no semaphore gets incremented. TODO think about this: does the caller provide a semaphore? Do we keep a vector of weak semaphore pointers? Do we RETURN a semaphore?
#endif

//

template<typename ts_policy, typename... types>
class event_rstream
    : public event_stream<event_rstream<ts_policy, types...>> {
    //
    using lock_t = typename tarp::type_traits::ts_types<ts_policy>::lock_t;
    using mutex_t = typename tarp::type_traits::ts_types<ts_policy>::mutex_t;
    using event_channel_t = event_channel<ts_policy, types...>;
    using event_rchan_t = rchan<event_channel_t, types...>;
    using this_type = event_rstream<ts_policy, types...>;
    //
public:
    event_rstream(bool autoflush = false) : m_autoflush(autoflush) {}

    // Enqueue an event to the stream channel. This buffers the event
    // until flush() is called, or if autoflush=true, it dispatches the
    // event immediately.
    template<typename... event_data_t>
    void enqueue(event_data_t &&...data) {
        // if autoflush enabled, then do not buffer.
        if (m_autoflush) {
            auto chan = m_stream_channel.lock();
            if (!chan) {
                return;
            }
            push_to_channel(data..., *chan);
            return;
        }

        // else buffer
        m_event_buffer.enqueue(std::forward<event_data_t>(data)...);
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
                chan->enqueue(events.front());
            } else {
                chan->enqueue(std::move(events.front()));
            }
            events.pop_front();
        }
    }

    // More convenient and expressive overloads for enqueuing
    // and dequeueing an element.

    template<typename... event_data_t>
    auto &operator<<(event_data_t &&...event_data) {
        enqueue(std::forward<event_data_t>(event_data)...);
        return *this;
    }

    std::shared_ptr<event_rchan_t> channel() {
        auto chan = m_stream_channel.lock();
        if (chan) {
            return chan;
        }

        chan = std::make_shared<event_channel_t>();
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
    template<typename T, typename C>
    void push_to_channel(T &&data, C &chan) {
        if constexpr (((std::is_copy_assignable_v<types> ||
                        std::is_copy_constructible_v<types>) &&
                       ...)) {
            chan.enqueue(data);
        } else {
            chan.enqueue(std::move(std::move(data)));
        }
    }

    mutable mutex_t m_mtx;
    const bool m_autoflush {false};
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
    event_wstream() { m_stream_channel = std::make_shared<event_channel_t>(); }

    std::optional<payload_t> get() { return m_stream_channel->get(); }

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

private:
    std::shared_ptr<event_channel_t> m_stream_channel;
};

//
// dequeue()
// get_channel (and associated it with a label in a lookup dict)
// remove_channel
// associate it with a semaphore that gets posted when ANY or ALL
// of the channels have items enqueued in them.

template<typename... types>
class event_aggregator {};

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

template<typename... types>
using event_rstream =
  impl::event_rstream<tarp::type_traits::thread_unsafe, types...>;

template<typename... types>
using event_wstream =
  impl::event_wstream<tarp::type_traits::thread_unsafe, types...>;
}  // namespace tu

}  // namespace evchan
}  // namespace tarp
