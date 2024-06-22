#include <chrono>
#include <shared_mutex>
#include <tarp/thread_entity.hxx>

namespace tarp {
using namespace tarp::threading;
using namespace tarp::sched;

using namespace std;

ThreadEntity::ThreadEntity()
    : m_state(threadState::INITIALIZED), m_signaled(false) {
}

ThreadEntity::~ThreadEntity() {
    stop();
}

void ThreadEntity::initialize() {
}

void ThreadEntity::prepare_resume() {
}

void ThreadEntity::cleanup() {
}

void ThreadEntity::do_work(void) {
    using namespace std::chrono_literals;
    wait_for(1s);
}

void ThreadEntity::wait_until(std::chrono::steady_clock::time_point tp) {
    LOCK(m_mtx);

    /* If the signal arrives before we even begin the sleep, then
     * return immediately. I.e. use level-triggered-interrupt semantics:
     * the 'signaled' state is asserted and we detect that and return.
     */
    if (m_signaled) {
        m_signaled = false;
        return;
    }

    m_wait_cond.wait_until(lock, tp, [this] {
        return m_signaled;
    });

    // NOTE: here the mutex is locked again.
    m_signaled = false;
}

void ThreadEntity::wait_for(std::chrono::microseconds duration) {
    auto now = std::chrono::steady_clock::now();
    return wait_until(now + duration);
}

bool ThreadEntity::is_paused(void) const {
    LOCK(m_mtx);
    return m_state == threadState::PAUSED;
}

bool ThreadEntity::is_stopped(void) const {
    LOCK(m_mtx);
    return m_state == threadState::STOPPED;
}

bool ThreadEntity::is_running(void) const {
    LOCK(m_mtx);
    return m_state == threadState::RUNNING;
}

// NOTE: Private function. No lock here.
void ThreadEntity::set_state(enum threadState state) {
    m_state = state;
}

void ThreadEntity::spawn(void) {
    set_state(threadState::RUNNING);
    std::thread t {[this] {
        loop();
    }};

    std::swap(t, m_thread);
}

/*
 */
void ThreadEntity::stop(void) {
    std::unique_lock l(m_mtx);
    set_state(threadState::STOPPED);
    m_signaled = true;

    /* if thread is either PAUSED or waiting in do_work, then get it
     * unstuck immediately */
    l.unlock();
    m_wait_cond.notify_all();

    /* no lock here or we would deadlock with the loop()
     * method; also NOTE .join() should not be called from
     * multiple threads. It is expected that only one thread
     * will call stop() for a thread entity. */
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void ThreadEntity::pause(void) {
    std::unique_lock l(m_mtx);
    if (m_state != threadState::RUNNING) {
        return;
    }

    set_state(threadState::PAUSED);
}

bool ThreadEntity::run() {
    using ts = threadState;

    std::unique_lock<std::mutex> l(m_mtx);

    // when signaled, interrupt calls to wait_for()/wait_until().
    m_signaled = true;

    /*
     * If stopped, this is permanent, so return. If INITIALIZED,
     * then we are running for the first time, so spawn
     * the thread. */
    switch (m_state) {
    case ts::INITIALIZED: spawn(); return true;
    case ts::STOPPED: return false;
    case ts::RUNNING:
    case ts::PAUSED:
        set_state(ts::RUNNING);
        l.unlock();
        m_wait_cond.notify_all();
        return true;
    }

    return false;
}

/*
 * Loop forever (or until stopped) and run do_work or pause, depending on the
 * state.
 *
 * (1)
 * We need to lock when checking the current state (i.e. for the switch
 * statement) to avoid race conditions. For example, consider two separate
 * threads requesting different states e.g. PAUSED and STOPPED. These must
 * be serialized.
 *
 * (2)
 * do_work runs with the mutex unlocked since it contains client-specific logic
 * that should not change the state of this base class. There are some other
 * reasons the mutex should not be locked.
 * - if do_work tried to lock the mutex (e.g. by calling pause(), stop() etc)
 *   there would be a deadlock.
 * - if do_work performs some long-running action then different threads would
 *   have to wait until do_work returned before they could get the mutex.
 *   Specifically, any calls to stop(), pause etc would hang until do_work
 * returned, which might be unacceptable (e.g. imagine do_work sleeps for 10
 * minutes).
 *
 * (3)
 * A similar rationale applies to prepare_resume and cleanup. These should be
 * very fast to run -- sleeping in there should be out of the question --
 * but unlocking before running these ensures there can be no deadlock.
 *
 * NOTE: the calls to .unlock() also give a chance to other threads to lock
 * the mutex in the meantime.
 */
void ThreadEntity::loop(void) {
    std::unique_lock<std::mutex> l(m_mtx, std::defer_lock);

    auto pause_checker = [this] {
        return m_state != threadState::PAUSED;
    };

    initialize();

    while (true) {
        l.lock(); /* (1) */

        switch (m_state) {
        case threadState::RUNNING:
            l.unlock();
            do_work(); /* (2) */
            break;
        case threadState::PAUSED: /* (3) */
            m_wait_cond.wait(l, pause_checker);
            l.unlock();
            prepare_resume();
            break;
        case threadState::STOPPED:
            l.unlock();
            cleanup();
            return; /* exit the thread */
        default: break;
        }
    }
}

void Oscillator::initialize(void) {
    // To start with, the next tick will be PERIOD from now.
    m_prev_tick_tp = std::chrono::steady_clock::now();
}

std::chrono::microseconds Oscillator::get_period() const {
    std::shared_lock l {m_mtx};
    return m_period;
}

void Oscillator::set_period(const std::chrono::microseconds &period) {
    {
        std::unique_lock l {m_mtx};
        m_period = period;
    }

    signal();
}

std::chrono::steady_clock::time_point Oscillator::time_now() const {
    return std::chrono::steady_clock::now();
}

void Oscillator::do_work(void) {
    std::chrono::microseconds period;

    {
        std::shared_lock l(m_mtx);
        period = m_period;
    }

    auto next_tick_tp = m_prev_tick_tp + period;
    auto now = time_now();
    if (now < next_tick_tp) {
        wait_until(next_tick_tp);
        return;
    }

    /* else, elapsed */

    m_prev_tick_tp = next_tick_tp;

    // if the timer has fallen behind, bring it up to date withe the current
    // time: ensure the next tick is in the future!
    if ((m_prev_tick_tp + period) <= time_now()) {
        m_prev_tick_tp = now;
    }

    on_tick();
    m_tick_signal.emit();
}

// NOP
void Oscillator::cleanup(void) {
}

// NOP
void Oscillator::prepare_resume(void) {
}

ActiveObject::ActiveObject(std::unique_ptr<tarp::sched::Scheduler> sched) {
    m_scheduler = std::move(sched);
}

}  // namespace tarp
