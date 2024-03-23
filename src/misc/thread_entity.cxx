#include <iostream>

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
    std::this_thread::sleep_for(1s);
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

void ThreadEntity::stop(void) {
    std::unique_lock<std::mutex> l(m_mtx);
    auto current_state = m_state;
    set_state(STOPPED);

    if (current_state == STOPPED) {
        return;
    }

    /* need to get the thread unstuck from waiting at the
     * condition variable first */
    if (current_state == PAUSED) {
        l.unlock();
        m_flag.notify_all();
    }

    /* no lock here or we would deadlock with the loop()
     * method; also NOTE .join() should not be called from
     * multiple threads. It is expected that only one thread
     * will call stop() for a thread entity. */
    if (m_thread.joinable()) {
        if (l.owns_lock()) l.unlock();
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
        m_flag.notify_all();
        return true;
    }

    return false;
}

void ThreadEntity::loop(void) {
    std::unique_lock<std::mutex> l(m_mtx, std::defer_lock);
    
    auto wait_checker = [this]{
        return m_state != PAUSED;
    };

    initialize();

    /* NOTE:
     * since we lock for a whole pass around the loop, client code
     * can only change m_state at the end of pass since that's when
     * the lock gets unlocked.
     */
    while (true) {
        l.lock();

        switch (m_state) {
        case RUNNING: do_task(); break;
        case PAUSED:
            m_flag.wait(l, wait_checker);
            prepare_resume();
            break;
        case STOPPED: cleanup(); return;
        default: break;
        }

        l.unlock();

        /* NOTE: this is to give other threads a chance to get
         * the lock e.g. in order to pause, wake, or stop the
         * thread entity. Without this the infinite loop means
         * no other threads will get a chance to lock the mutex
         * -- even though we unlock it at the end of the while loop,
         * as that is too fast.
         * A minimal sleep will fix this problem. The sleep duration
         * is inconsequential -- what matters is that it gets put
         * to sleep and suspended by the kernel. */
        std::this_thread::sleep_for(100ns);
    }
}
