#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>

namespace tarp {

class cancellation_token_source;

// A class that can be passed to operations that need to be
// asynchronously cancellable.
// This class is cheap to copy. Any given copy may only be
// accessed by one thread at a time. Different copies may be
// accessed by different threads at the same time.
// Using one single copy in multiple threads without synchronization
// is not thread safe.
class cancellation_token final {
public:
    explicit cancellation_token();

    ~cancellation_token() {
        if (m_observer_id.has_value()) {
            m_token->remove_observer(*m_observer_id);
        }
    }

    // cheap to copy and move. Meant to be passed by copy.
    cancellation_token(const cancellation_token &) = default;
    cancellation_token(cancellation_token &&) = default;
    cancellation_token &operator=(cancellation_token &&) = default;
    cancellation_token &operator=(cancellation_token &) = default;

    // check if the token has been signaled.
    bool canceled() const { return m_token->canceled(); }

    // key granting access to the cancel() method; a token
    // may therefore only be canceled by the cancellation_token_source
    // that produced it.
    class cancellation_key {
    private:
        friend class cancellation_token_source;
        cancellation_key() = default;
    };

    // Signal the cancellation token.
    void cancel(cancellation_key) { m_token->cancel(); }

    // Register a callback to be invoked when the cancellation_token
    // is signaled. Return true if successful. Otherwise, if the
    // cancellation_token has *already* been signaled, return false; in that
    // case, the  callback is *not* added since it would never be invoked as
    // the cancellation event has already happened.
    //
    // NOTE: only one single callback can be added. Multiple invocations of
    // add_notifier will replace the previous callback added.
    //
    // NOTE: the callback will be invoked from the thread in which
    // cancellation_token_source invokes cancel() on the token. Beware
    // of thread-unsafety and deadlocks!
    bool add_notifier(std::function<void()> notifier) {
        m_observer_id = m_token->add_observer(std::move(notifier));
        return m_observer_id.has_value();
    }

    // Remove the callback that was added, if any. If not called,
    // this will automatically be done in the destructor.
    void remove_notifier() {
        if (m_observer_id.has_value()) {
            m_token->remove_observer(*m_observer_id);
        }
    }

private:
    class token final {
    public:
        bool canceled() const {
            std::unique_lock l {m_mtx};
            return m_canceled;
        }

        bool cancel() {
            std::unique_lock l {m_mtx};
            bool already_canceled = m_canceled;
            m_canceled = true;

            if (!already_canceled) {
                // NOTE: risk of deadlock here; the callbacks must never,
                // directly or indirectly, call back into the token such that
                // this mutex gets locked again.
                invoke_observers();
            }

            return !already_canceled;
        }

        void invoke_observers() {
            for (auto &[_, f] : m_observers) {
                f();
            }
        }

        std::optional<std::uint32_t>
        add_observer(std::function<void()> observer) {
            std::unique_lock l {m_mtx};
            if (m_canceled) {
                return std::nullopt;
            }

            auto observer_id = ++m_last_id;
            m_observers[observer_id] = std::move(observer);
            return observer_id;
        }

        void remove_observer(std::size_t id) {
            std::unique_lock l {m_mtx};
            m_observers.erase(id);
        }

    private:
        bool m_canceled = false;
        std::map<std::size_t, std::function<void()>> m_observers;
        std::uint32_t m_last_id = 0;
        mutable std::mutex m_mtx;
    };

    // The cancellation_token class is cheap to copy since it merely involves
    // a std::shared_ptr copy. However, it is thread-unsafe since neither
    // of these member fields are thread-safe. I.e. importantly, std::shared_ptr
    // itself is **not** thread-safe.
    std::shared_ptr<token> m_token;
    std::optional<std::uint32_t> m_observer_id = 0;
};

// A cancellation_token_source produces a cancellation token;
// the token can only be signaled by the cancellation source
// that created it. This class is thread safe and may be used
// from multiple threads.
class cancellation_token_source final {
public:
    cancellation_token_source(const cancellation_token_source &) = delete;
    cancellation_token_source(cancellation_token_source &&) = delete;
    cancellation_token_source &operator=(cancellation_token_source &&) = delete;
    cancellation_token_source &operator=(cancellation_token_source &) = delete;

    explicit cancellation_token_source() {
        m_token = std::make_shared<cancellation_token>();
    }

    // Return the current token. Only one token is associated with the
    // cancellation source at a given time.
    cancellation_token token() {
        // protect m_token during copy.
        std::unique_lock l {m_mtx};
        return *m_token;
    }

    // Signal the cancellation token.
    void cancel() {
        decltype(m_token) token;
        {
            std::unique_lock l {m_mtx};
            token = m_token;
        }

        token->cancel(cancellation_token::cancellation_key {});
    }

    // Discard the current cancellation token, if any, and create a new one.
    void reset() {
        std::unique_lock l {m_mtx};
        m_token.reset();
        m_token = std::make_shared<cancellation_token>();
    }

private:
    mutable std::mutex m_mtx;
    std::shared_ptr<cancellation_token> m_token;
};

}  // namespace tarp
