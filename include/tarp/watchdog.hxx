#ifndef TARP_WATCHDOG_HXX_
#define TARP_WATCHDOG_HXX_

#include <chrono>
#include <functional>
#include <mutex>
#include <stdexcept>

#include <tarp/threading.hxx>

namespace tarp {

using wd_guard_cb_t = std::function<bool(void)>;
using wd_action_cb_t = std::function<void(void)>;

/*
 * Simple watchdog class running in its own thread.
 *
 * The watchdog can run in two modes (see constructors below):
 *
 * 1. guarded mode
 * The user must provide a guard callback. At the end of every
 * lapsed interval, the watchdog will invoke the guard. If
 * it returns true, the watchdog resets itself. If it returns
 * false, the watchdog will invoke the action callback and then
 * pause itself. The user need not explicitly reset the watchdog
 * in this case.
 *
 * 2. unguarded mode
 * At the end of every lapsed interval, the watchdog is triggered
 * -- 'bites' -- and will invoke the action callback, and then
 * pause itself. The user must explicitly call reset() to prevent
 * the watchdog from triggering.
 *
 * NOTE:
 * A paused watchdog can be resumed via a call to kick().
 * When this happens, the watchdog implicitly resets itself as well
 * so that it is not instantlly triggered again.
 */
template<typename T = std::chrono::seconds>
class Watchdog : public tarp::threading::ThreadEntity {
public:
    /* guarded-mode watchdog constructor */
    Watchdog(T interval, wd_action_cb_t action, wd_guard_cb_t guard)
        : m_guard_mode(true)
        , m_interval(interval)
        , m_action_cb(action)
        , m_guard(guard) {}

    /* unguarded-mode watchdog constructor */
    Watchdog(T interval, wd_action_cb_t action)
        : m_guard_mode(false)
        , m_interval(interval)
        , m_action_cb(action)
        , m_guard() {}

    /* Reset the watchdog so that it waits another full interval
     * before it 'bites'. I.e. keep the watchdog at bay.
     * NOTE: a watchdog that has already 'bitten' must be 'kicked'; .reset()
     * is only for a running watchdog.
     * NOTE: a watchdog that has been stopped can never used again. Reset
     * has no effect on a stopped watchdog. */
    void reset();

    /* Called either manually or automatically on failure to
     * reset the watchdog.
     * This in turn calls the user-supplied action callback.
     * The watchdog will be paused after this and can be resumed
     * (assuming the process is not exited) with kick().
     * When that is done, the watchdog is implicitly reset as well
     * so that it doesn't instantly trigger again. */
    void bite(void);

    /*
     * Re-arm the watchdog if paused. NOP if the watchdog is not paused.
     * NOTE: paused is *not* the same as stopped. A stopped watchdog cannot
     * restarted. */
    void kick();

private:
    void initialize(void) override;
    void do_work(void) override;

    bool m_guard_mode;
    T m_interval;
    std::chrono::steady_clock::time_point m_deadline;
    wd_action_cb_t m_action_cb;
    wd_guard_cb_t m_guard;
    mutable std::mutex m_mtx;
};

template<typename T>
void Watchdog<T>::reset() {
    // printf("****** WATCHDOG RESET\n");

    {
        LOCK(m_mtx);
        if (!is_running()) {
            return;
        }
        initialize();
    }

    signal();
}

template<typename T>
void Watchdog<T>::initialize() {
    m_deadline = std::chrono::steady_clock::now() + m_interval;
}

template<typename T>
void Watchdog<T>::bite() {
    LOCK(m_mtx);

    // printf("****** WATCHDOG TRIGGERED\n");
    if (!m_action_cb) {
        throw std::logic_error("Illegal null action callback");
    }
    m_action_cb();
    set_state(threadState::PAUSED);
}

template<typename T>
void Watchdog<T>::do_work(void) {
    decltype(m_deadline) deadline;
    {
        LOCK(m_mtx);
        deadline = m_deadline;
    }

    if (std::chrono::steady_clock::now() < deadline) {
        wait_for(m_interval);
        return;
    }

    {
        LOCK(m_mtx);
        if (m_guard_mode) {
            if (m_guard()) {
                return;
            }
        }
    }

    bite();
}

template<typename T>
void Watchdog<T>::kick() {
    if (is_running()) {
        return;
    }

    {
        std::unique_lock l {m_mtx};
        initialize();
    }

    run();
}


}  // namespace tarp

#endif
