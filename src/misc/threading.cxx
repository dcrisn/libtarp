#include "tarp/log.h"
#include "tarp/signal.hxx"
#include <chrono>
#include <memory>
#include <shared_mutex>
#include <stdexcept>
#include <tarp/threading.hxx>

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
    // the state of a stopped thread cannot be changed.
    if (m_state == threadState::STOPPED) {
        return;
    }

    // a thread is only set to INITIALIZED in the constructor, once.
    if (state == threadState::INITIALIZED) {
        throw std::logic_error(
          "BUG: the state of the thread may not be set to INITIALIZED");
    }

    m_state = state;
}

// NOTE: Private function. No lock here.
enum ThreadEntity::threadState ThreadEntity::get_state() const {
    return m_state;
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

        /* If the thread entity has been created but not .run() yet,
         * then keep it that way. Do not signal. */
        if (get_state() == threadState::INITIALIZED) {
            return;
        }
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

ActiveObject::ActiveObject(
  std::unique_ptr<tarp::sched::Scheduler<interfaces::task>> sched) {
    m_scheduler = std::move(sched);
}

bool ActiveObject::has_pending_tasks() const {
    std::unique_lock l {m_scheduler_mtx};
    return m_scheduler->get_queue_length() > 0;
}

std::unique_ptr<interfaces::task> ActiveObject::get_next_task() {
    std::unique_lock l {m_scheduler_mtx};

    auto task = m_scheduler->dequeue();

    if (!task) {
        throw std::logic_error(
          "BUG: cannot get task from QueueItem in ActiveObject");
    }
    return task;
}

WorkerThread::WorkerThread(std::uint32_t worker_id) : m_worker_id(worker_id) {
}

void WorkerThread::set_task(std::unique_ptr<interfaces::task> task) {
    std::unique_lock l {m_mtx};
    m_next_task = std::move(task);
}

void WorkerThread::initialize(void) {
}

void WorkerThread::cleanup(void) {
}

void WorkerThread::prepare_resume(void) {
}

/* Start in on the next task if available */
void WorkerThread::do_work(void) {
    {
        std::unique_lock l {m_mtx};

        if (!m_next_task) {
            set_state(threadState::PAUSED);
            return;
        }

        m_current_task = std::move(m_next_task);
        m_next_task.reset();
    }

    m_current_task->execute();

    /* ready for new work, become a follower again. */
    m_sig_work_done.emit(m_worker_id);
}

// NOTE: this id is a read-only field, hence no mutex-protection is needed.
std::size_t WorkerThread::get_worker_id() const {
    return m_worker_id;
}

ThreadPool::ThreadPool(
  uint16_t num_workers,
  std::unique_ptr<tarp::sched::Scheduler<interfaces::task>> scheduler)
    : m_num_workers(num_workers), m_taskq(std::move(scheduler)) {
}

std::size_t ThreadPool::get_queue_length() const {
    std::shared_lock l {m_mtx};
    return m_taskq->get_queue_length();
}

void ThreadPool::enqueue_task(std::unique_ptr<interfaces::task> task) {
    if (!task) {
        throw std::invalid_argument("Illegal attempt to enqueue null task");
    }

    {
        std::unique_lock l {m_mtx};
        m_taskq->enqueue(std::move(task));
    }

    // if thread pool idling, wake it up.
    signal();
}

std::size_t ThreadPool::get_num_threads() const {
    std::shared_lock l {m_mtx};
    return m_num_workers;
}

/* NOTE: the actual resizing is done in loop() when the flow of control
 * gets around to it. */
void ThreadPool::set_num_threads(std::size_t n) {
    {
        std::unique_lock l {m_mtx};
        m_num_workers = n;
    }

    signal(); /* wake thread pool if idling */
}

// a more meaningful alias for run().
void ThreadPool::start() {
    run();
}

/* Stop and destroy all the workers.
 * NOTE: This is a permanent action: the thread pool cannot be restarted
 * once paused.
 * NOTE: This function will block until all workers have been stopped. A
 * worker may only be stopped when idle i.e. once it has completed whatever
 * current task it is in the middle of.
 * */
void ThreadPool::cleanup(void) {
    /* copy or move what we need here so we can unlock the mutex,
     * otherwise if calling into a worker with locked mutex we are almost
     * guaranteed to deadlock.
     * Example:
     * -- call signal_connection->disconnect() for worker x
     * -- Assume worker x is emitting the task-completion signal, which
     *  calls back into the ThreadPool.
     * -- the ThreadPool signal handler is add_follower, which attempts to lock
     *  m_mtx, but we already hold it here ==> DEADLOCK.
     *
     * To prevent deadlocks, ensure we *never* make a call to another object
     * from inside a mutex-protected region. */
    decltype(m_threads) worker_threads;
    decltype(m_worker_signals) worker_signal_connections;

    {
        std::unique_lock l {m_mtx};
        worker_threads = m_threads;
        std::swap(m_worker_signals, worker_signal_connections);
    }

    for (auto &[worker_id, signal_connection] : worker_signal_connections) {
        signal_connection->disconnect();
    }

    for (auto &[worker_id, worker] : worker_threads) {
        worker->stop();
    }
}

void ThreadPool::do_work(void) {
    resize_pool_if_needed();

    std::shared_ptr<tarp::threading::WorkerThread> worker;
    std::unique_ptr<interfaces::task> task;

    {
        std::unique_lock l {m_mtx};

        // nothing to do. Wait for some arbitrary time until further notice.
        if (m_idle_threads.empty() || m_taskq->empty()) {
            set_state(threadState::PAUSED);
            return;
        }

        // use LIFO semantics for the idle threads; i.e. the thread that has
        // most recently become idle is the one that gets picked first for
        // new work. See POSA, vol2 p464.
        // => Leaders get dequeued from the front, followers get enqueued to
        // the front as well.
        worker = m_idle_threads.front();
        m_idle_threads.pop_front();

        task = m_taskq->dequeue();
        if (!task) {
            throw std::logic_error("BUG: failed conversion from QueueItem "
                                   "to tarp::threading::task");
        }
    }

    if (!worker || !task) {
        throw std::logic_error("BUG, null worker/task");
    }

    worker->set_task(std::move(task));
    worker->run();
}

// @locks
void ThreadPool::resize_pool_if_needed() {
    /* objects that we need to call into; we save them here so we can do it
     * without locking any mutexes on the ThreadPool side, otherwise if we get a
     * circular call we will end up with a deadlock. */
    std::vector<std::shared_ptr<tarp::threading::WorkerThread>> to_initialize;
    std::vector<std::shared_ptr<tarp::threading::WorkerThread>> to_stop;
    std::unique_ptr<tarp::signal_connection> signal_connection;

    {
        std::unique_lock l {m_mtx};
        size_t current_pool_size = m_threads.size();
        size_t desired_pool_size = m_num_workers;

        // already the right size, nothing to do.
        if (current_pool_size == desired_pool_size) {
            return;
        }

        // expand the pool size.
        if (current_pool_size < desired_pool_size) {
            auto n = desired_pool_size - current_pool_size;
            for (size_t i = 0; i < n; ++i) {
                auto id = m_next_worker_id++;
                auto worker = std::make_shared<WorkerThread>(id);

                // probably a duplicated entry causing the failed insertion.
                // This should never happen.
                if (auto [_, ok] = m_threads.insert({id, worker}); !ok) {
                    throw std::logic_error(
                      "Likely BUG: failed to register new worker thread");
                }

                m_idle_threads.push_back(worker);
                hook_up_task_completion_signal(*worker);
                to_initialize.push_back(worker);
            }
        }

        // shrink the pool size: we can only prune threads that are not
        // currently engaged in a task, i.e. only currently idle threads
        // ('followers').
        if (current_pool_size > desired_pool_size) {
            size_t num_excess_threads = current_pool_size - desired_pool_size;
            auto n = std::min(num_excess_threads, m_idle_threads.size());

            for (size_t i = 0; i < n; ++i) {
                auto worker = m_idle_threads.back();
                auto worker_id = worker->get_worker_id();
                m_idle_threads.pop_back();
                m_threads.erase(worker_id);
                to_stop.push_back(worker);

                // disconnect task-completion signal connection
                auto signal_connection_it = m_worker_signals.find(worker_id);
                if (signal_connection_it != m_worker_signals.end()) {
                    signal_connection = std::move(signal_connection_it->second);
                    m_worker_signals.erase(signal_connection_it);
                }
            }
        }
    }

    // =============== run UNLOCKED ===========
    //
    for (auto &worker : to_initialize) {
        worker->run();   /* initialize */
        worker->pause(); /* idle until further notice */
    }

    for (auto &worker : to_stop) {
        worker->stop();
    }

    if (signal_connection) {
        signal_connection->disconnect();
    }
}

void ThreadPool::add_follower(uint32_t worker_id) {
    std::shared_ptr<tarp::threading::WorkerThread> worker;

    {
        std::unique_lock l {m_mtx};

        // make sure the thread is actually 'known'.
        auto found = m_threads.find(worker_id);
        if (found == m_threads.end()) {
            throw std::logic_error(
              "BUG: apparently unknown thread cannot be made a follower");
        }

        // make sure the thread does not exist in m_idle_threads, otherwise
        // there is a bug
        auto idle_found =
          std::find_if(m_idle_threads.begin(),
                       m_idle_threads.end(),
                       [worker_id](auto thread) {
                           return thread->get_worker_id() == worker_id;
                       });

        if (idle_found != m_idle_threads.end()) {
            throw std::logic_error("BUG: duplicate follower in thread pool");
        }

        worker = found->second;
        m_idle_threads.push_front(worker);
    }

    if (!worker) {
        return;
    }

    worker->pause();
    m_num_tasks_handled++;
    signal(); /* wake up thread pool if idling */
}

void ThreadPool::hook_up_task_completion_signal(
  tarp::threading::WorkerThread &worker) {
    auto worker_id = worker.get_worker_id();

    // an entry already exists, i.e. signal already connected, then we have
    // a bug. This function should only be called once for a given worker so
    // there should not be duplicates.
    if (m_worker_signals.find(worker_id) != m_worker_signals.end()) {
        throw std::logic_error(
          "BUG: duplicate task-completion signal connection with worker "s +
          std::to_string(worker_id));
    }

    auto signal_connection = worker.get_idle_signal().connect(
      std::bind(&ThreadPool::add_follower, this, std::placeholders::_1));

    m_worker_signals.emplace(
      std::make_pair(worker_id, std::move(signal_connection)));
}

void ThreadPool::initialize(void) {
}

void ThreadPool::prepare_resume(void) {
}

size_t ThreadPool::get_num_tasks_handled() const {
    std::shared_lock l {m_mtx};
    return m_num_tasks_handled;
}


}  // namespace tarp
