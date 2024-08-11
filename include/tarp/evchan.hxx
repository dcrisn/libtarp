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

#include <iostream>

//

namespace tarp {

//

#ifdef REAL
#error "#define REAL would overwrite existing definition"
#endif

#define REAL static_cast<C *>(this)

template<typename C, typename... types>
class wchan {
    using payload_t = tarp::type_traits::type_or_tuple_t<types...>;

public:
    std::uint32_t get_id() const { return REAL->get_id(); }

    void enqueue(const types &...event_data) {
        return REAL->enqueue(event_data...);
    }

    template<typename T>
    void enqueue(T &&event_data) {
        return REAL->enqueue(std::forward<T>(event_data));
    }

    // More convenient and expressive overloads for enqueuing
    // and dequeueing an element.
    auto &operator<<(const payload_t &event) { return REAL->operator<<(event); }

    auto &operator<<(payload_t &&event) {
        return REAL->operator<<(std::move(event));
    }
};

//

template<typename C, typename... types>
class rchan {
    using payload_t = tarp::type_traits::type_or_tuple_t<types...>;

public:
    std::uint32_t get_id() const { return REAL->get_id(); }

    // True if there are no queued items.
    bool empty() const { return REAL->empty(); }

    // Return the number of events currently enqueued.
    std::size_t size() const { return REAL->size(); }

    std::optional<payload_t> get() { return REAL->get(); }

    std::deque<payload_t> get_all() { return REAL->get_all(); }

    auto &operator>>(std::optional<payload_t> &event) {
        return REAL->operator>>(event);
    }
};

#undef REAL

//

// TODO: for maximum genericity, we could even intialize with a LIST of
// semaphores to be posted and use an improved tarp/semahore that
// also supports an eventfd backend etc.
template<typename... types>
class event_channel
    : public wchan<event_channel<types...>, types...>
    , public rchan<event_channel<types...>, types...> {
    using is_tuple =
      typename tarp::type_traits::type_or_tuple<types...>::is_tuple;

    void do_enqueue_misc() {
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

    DISALLOW_COPY_AND_MOVE(event_channel<types...>);

    // NOTE: semaphore can be null if not necessary i.e. if the
    // channel is merely used as a temporary thread-safe buffer and no
    // notification is needed.
    event_channel(std::optional<std::uint32_t> max_buffer_size = std::nullopt,
                  std::shared_ptr<tarp::binary_semaphore> sem = nullptr)
        : m_max_buffsz(max_buffer_size), m_event_sink_notifier(sem) {
        if (m_max_buffsz.has_value() && *m_max_buffsz == 0) {
            throw std::logic_error("nonsensical max limit of 0");
        }
    }

    // Get an id uniquely identifying the event channel. This is unique across
    // all instances of this class at any given time.
    std::uint32_t get_id() const { return m_id; }

    // Enqueue an event into the channel.
    // If the payload type is a tuple made up of various items,
    // this function is a convenience that allows passing discrete
    // elements for the parameters. These will be implicitly tied together
    // into a std::tuple: enqueue(a,b,c) => enqueue({a,b,c}).
    // NOTE: the other overload where std::tuple would be passed
    // explicitly will be more performant when *moving* or forwarding
    // the parameters into the function, since it is a template.
    void enqueue(const types &...event_data) {
        std::unique_lock l {m_mtx};

        // if the payload is made up of multiple elements, treat it as a tuple.
        if constexpr (tarp::type_traits::is_tuple_v<types...>) {
            m_event_data_items.push_back(std::make_tuple(event_data...));
        }

        // else it is a single element
        else {
            m_event_data_items.push_back(event_data...);
        }

        do_enqueue_misc();
    }

    // This overload takes an actual tuple as the argument
    // -- IFF payload_t is an actual tuple.
    template<typename T>
    void enqueue(T &&event_data) {
        static_assert(
          std::is_same_v<std::remove_reference_t<T>, payload_t> ||
          std::is_convertible_v<std::remove_reference<T>, payload_t>);

        std::unique_lock l {m_mtx};
        m_event_data_items.emplace_back(std::forward<T>(event_data));
        do_enqueue_misc();
    }

    // True if there are no queued items.
    bool empty() const {
        std::unique_lock l {m_mtx};
        return m_event_data_items.empty();
    }

    // Return the number of events currently enqueued.
    std::size_t size() const {
        std::unique_lock l {m_mtx};
        return m_event_data_items.size();
    }

    std::optional<payload_t> get() {
        std::unique_lock l {m_mtx};
        if (m_event_data_items.empty()) {
            return std::nullopt;
        }

        std::optional<payload_t> data;

        // this is to cover both copy-assignable/copy-construcible and
        // move-only semantics objects. Either copying or moving must be
        // supported, otherwise the channel cannot be used.
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

    std::deque<payload_t> get_all() {
        std::unique_lock l {m_mtx};
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

    mutable std::mutex m_mtx;
    std::deque<payload_t> m_event_data_items;

    std::shared_ptr<tarp::binary_semaphore> m_event_sink_notifier;
};

//

template<typename... types>
class event_broadcaster {
    static_assert(((std::is_copy_assignable_v<types> ||
                    std::is_copy_constructible_v<types>) &&
                   ...));

public:
    event_broadcaster(bool autodispatch = false)
        : m_autodispatch(autodispatch) {}

    template<typename... event_data_t>
    void enqueue(event_data_t &&...data) {
        m_event_buffer.enqueue(std::forward<event_data_t>(data)...);

        // if autodispatch is enabled, then always flush immediately: do not
        // buffer.
        if (m_autodispatch) {
            dispatch();
        }
    }

    void dispatch() {
        std::unique_lock l {m_mtx};

        std::vector<std::shared_ptr<event_channel<types...>>> channels;

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
    // channels that  are no longer alive.
    std::size_t num_channels() const {
        std::unique_lock l {m_mtx};
        return m_event_channels.size();
    }

    void connect(std::weak_ptr<event_channel<types...>> channel) {
        std::unique_lock l {m_mtx};
        m_event_channels.push_back(channel);
    }

private:
    mutable std::mutex m_mtx;
    const bool m_autodispatch {false};
    event_channel<types...> m_event_buffer;
    std::vector<std::weak_ptr<event_channel<types...>>> m_event_channels;
};

//

template<typename T>
class event_stream_pubif {
public:
    DISALLOW_COPY_AND_MOVE(event_stream_pubif<T>);
    event_stream_pubif<T>() = default;

    // std::shared_ptr<rchan<event_channel<types...>, types...>> channel() {
    //     return static_cast<T *>(this)->channel();
    // }

    auto channel() { return static_cast<T *>(this)->channel(); }
};

template<typename... types>
class event_stream : public event_stream_pubif<event_stream<types...>> {
public:
    event_stream(bool autoflush = false) : m_autoflush(autoflush) {}

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

        m_event_buffer.enqueue(std::forward<event_data_t>(data)...);
    }

    void flush() {
        std::unique_lock l {m_mtx};

        auto events = m_event_buffer.get_all();

        auto chan = m_stream_channel.lock();

        // no one is listening, so no point streaming any events!
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

    std::shared_ptr<rchan<event_channel<types...>, types...>> channel() {
        auto chan = m_stream_channel.lock();
        if (chan) {
            return chan;
        }

        chan = std::make_shared<event_channel<types...>>();
        m_stream_channel = chan;
        return chan;
    }

    auto &interface() {
        return dynamic_cast<event_stream_pubif<event_stream<types...>> &>(
          *this);
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

    mutable std::mutex m_mtx;
    const bool m_autoflush {false};
    event_channel<types...> m_event_buffer;
    std::weak_ptr<event_channel<types...>> m_stream_channel;
};

//

//
  // dequeue()
  // get_channel (and associated it with a label in a lookup dict)
  // remove_channel
  // associate it with a semaphore that gets posted when ANY or ALL
  // of the channels have items enqueued in them.

template<typename... types>
class event_aggregator {};


}  // namespace tarp
