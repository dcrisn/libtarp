#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <tarp/cxxcommon.hxx>
#include <tarp/signal.hxx>
#include <tarp/timeguard.hxx>

namespace tarp {

class ThreadEntity {
public:
    ThreadEntity(void);
    virtual ~ThreadEntity(void);

    bool run();
    void pause(void);
    void stop(void);

    bool is_paused(void) const;
    bool is_stopped(void) const;
    bool is_running(void) const;

protected:
    enum threadState { INITIALIZED, RUNNING, PAUSED, STOPPED };

    DISALLOW_COPY_AND_MOVE(ThreadEntity);
    void set_state(enum threadState state);

    /*
     * Hooks derived classes can override -- NOPs by default;
     * NOTE: all these hooks run in a mutex-protected context
     * inside the main loop, so they are thread-safe (for the
     * purposes of changing the state of this class).
     *
     * (1) called *once* after the thread is spawned,
     * before the main loop starts.
     *
     * (2) called when run() is invoked on a paused thread entity.
     * This can be used for reinitialization when waking from a pause.
     *
     * (3) called for every pass around the main loop. This is the
     * main thing the thread entity does. There is no sleep to speak
     * of in the main loop so the user should sleep in this function
     * as approprirate if any delays are needed.
     *
     * (4)
     * Called on stopping the thread, before the main loop returns.
     * This is when stop() is called expicitly or in the destructor
     * of the thread entity.
     */
    virtual void initialize(void);     /* (1) */
    virtual void prepare_resume(void); /* (2) */
    virtual void do_task(void) = 0;    /* (3) */
    virtual void cleanup(void);        /* (4) */

    /*
     * Derived classes that need to wait for certain periods in do_task
     * should use one of these functions -- as opposed to sleeping.
     * This is because if stop() is called, the thread should be
     * responsive and unblock immediately. wait_until and wait_for
     * handle this case. OTOH a ThreadEntity engaged in a blocking sleep
     * inside do_task will hang until do_task returns, hence a stop()
     * request will block for a possibly unacceptably long period.
     */
    void wait_until(std::chrono::steady_clock::time_point tp);
    void wait_for(std::chrono::microseconds duration);

private:
    void spawn(void);
    void loop(void);
    void wake(std::unique_lock<std::mutex> &&guard);

    std::thread m_thread;
    mutable std::mutex m_mtx;
    enum threadState m_state = STOPPED;
    std::condition_variable m_wait_cond;
    bool m_keep_waiting;
};

template<typename T>
class Timer : public ThreadEntity {
public:
    Timer(std::unique_ptr<tarp::TimeGuard<T>> interval);

    auto &get_timeout_signal(void) { return m_timeout_signal; }

private:
    virtual void do_task(void) override;

    std::unique_ptr<tarp::TimeGuard<T>> m_guard;
    tarp::signal<void(void)> m_timeout_signal;
};

template<typename T>
Timer<T>::Timer(std::unique_ptr<tarp::TimeGuard<T>> interval)
    : m_guard(std::move(interval)) {
}

template<typename T>
void Timer<T>::do_task(void) {
    if (m_guard->disabled()) {
        set_state(STOPPED);
        return;
    }

    if (m_guard->down()) {
        m_timeout_signal.emit();
        m_guard->shift(1);
        return;
    }

    if (m_guard->up()) {
        wait_until(m_guard->get_next_timepoint());
    }
}

}  // namespace tarp
