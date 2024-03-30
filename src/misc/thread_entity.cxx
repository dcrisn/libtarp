#include <tarp/thread_entity.hxx>

using namespace std;
using namespace tarp;

ThreadEntity::ThreadEntity() : m_state(INITIALIZED) {
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

void ThreadEntity::do_task(void) {
    using namespace std::chrono_literals;
    wait_for(1s);
}

void ThreadEntity::wait_until(std::chrono::steady_clock::time_point tp) {
    LOCK(m_mtx);

    m_keep_waiting = true;

    m_wait_cond.wait_until(lock, tp, [this] {
        return !m_keep_waiting;
    });
}

void ThreadEntity::wait_for(std::chrono::microseconds duration) {
    LOCK(m_mtx);

    m_keep_waiting = true;

    m_wait_cond.wait_for(lock, duration, [this] {
        return !m_keep_waiting;
    });
}

bool ThreadEntity::is_paused(void) const {
    LOCK(m_mtx);
    return m_state == PAUSED;
}

bool ThreadEntity::is_stopped(void) const {
    LOCK(m_mtx);
    return m_state == STOPPED;
}

bool ThreadEntity::is_running(void) const {
    LOCK(m_mtx);
    return m_state == RUNNING;
}

void ThreadEntity::set_state(enum threadState state) {
    m_state = state;
}

void ThreadEntity::spawn(void) {
    set_state(RUNNING);
    std::thread t {[this] {
        loop();
    }};

    std::swap(t, m_thread);
}

/*
 */
void ThreadEntity::stop(void) {
    std::unique_lock<std::mutex> l(m_mtx);
    set_state(STOPPED);
    m_keep_waiting = false;

    /* if thread is either PAUSED or waiting in do_task, then get it
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
    std::unique_lock<std::mutex> l(m_mtx);
    if (m_state != RUNNING) {
        return;
    }

    set_state(PAUSED);
}

bool ThreadEntity::run() {
    std::unique_lock<std::mutex> l(m_mtx);

    /*
     * If already running, NOP. If stopped, this is
     * permanent, so return. If INITIALIZED, then
     * we are running for the first time, so spawn
     * the thread. */
    switch (m_state) {
    case INITIALIZED: spawn(); return true;
    case RUNNING: return true;
    case STOPPED: return false;
    case PAUSED:
        set_state(RUNNING);
        l.unlock();
        m_wait_cond.notify_all();
        return true;
    }

    return false;
}

/*
 * Loop forever (or until stopped) and run do_task or pause, depending on the
 * state.
 *
 * (1)
 * We need to lock when checking the current state (i.e. for the switch
 * statement) to avoid race conditions. For example, consider two separate
 * threads requesting different states e.g. PAUSED and STOPPED. These must
 * be serialized.
 *
 * (2)
 * do_task runs with the mutex unlocked since it contains client-specific logic
 * that should not change the state of this base class. There are some other
 * reasons the mutex should not be locked.
 * - if do_task tried to lock the mutex (e.g. by calling pause(), stop() etc)
 *   there would be a deadlock.
 * - if do_task performs some long-running action then different threads would
 *   have to wait until do_task returns before they can get the mutex.
 *   Specifically, any calls to stop() would hang until do_task returned,
 *   which might be unacceptable (e.g. imagine do_task sleeps for 10 minutes).
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
        return m_state != PAUSED;
    };

    initialize();

    while (true) {
        l.lock(); /* (1) */

        switch (m_state) {
        case RUNNING:
            l.unlock();
            do_task(); /* (2) */
            break;
        case PAUSED: /* (3) */
            m_wait_cond.wait(l, pause_checker);
            l.unlock();
            prepare_resume();
            break;
        case STOPPED:
            l.unlock();
            cleanup();
            return;
        default: break;
        }
    }
}
