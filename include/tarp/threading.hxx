#pragma once

// C++ stdlib
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <list>
#include <map>
#include <shared_mutex>
#include <thread>
#include <type_traits>

#include <tarp/cxxcommon.hxx>
#include <tarp/sched.hxx>
#include <tarp/signal.hxx>
#include <tarp/timeguard.hxx>

namespace tarp {
namespace threading {

using namespace std::chrono_literals;

/* Abstract task interface; it is important that this be a non-template
 * so that we do not need to templatize task containers and everything else
 * that interacts with tasks merely at an interface level. */
class task : public tarp::sched::QueueItem {
public:
    virtual ~task();

    virtual void execute(void) = 0;
};

/*
 * Concrete task that packs a callable for scheduling and deferred
 * execution.
 *
 * --> result_type
 * This should be the return type of the callable, communicated back to the
 * caller via a future.
 *
 * --> callable_type
 * A callable that returns a value of type result_type and that must be
 * invokable without arguments. It should contain all the context it
 * requires for execution. Therefore this is typically a lambda or
 * a functor or some such.
 */
template<typename result_type, typename callable_type>
class command : public task {
public:
    command(callable_type &&func);
    ~command() override {};

    void execute() override;
    std::future<result_type> get_future();

    std::string get_pretty_name() const override { return ""; }

private:
    std::promise<result_type> m_result;
    callable_type m_f;
};

template<typename callable_type>
std::unique_ptr<task> make_task(callable_type &&f) {
    std::unique_ptr<tarp::threading::task> ret;

    using return_type = std::invoke_result_t<callable_type>;

    ret.reset(new tarp::threading::command<return_type, callable_type>(
      std::forward<decltype(f)>(f)));
    return ret;
}

template<typename result_type, typename callable_type>
command<result_type, callable_type>::command(callable_type &&func) : m_f(func) {
}

template<typename return_type, typename callable_type>
void command<return_type, callable_type>::execute(void) {
    /* If the callable returns a result, we must commmunicate it to
     * the future. If it does not return a result (i.e. it returns void),
     * we must still call .set_value() on the future without arguments
     * in order to unblock the future since the promise is now fulfilled.
     */
    if constexpr (!std::is_void_v<std::invoke_result_t<decltype(m_f)>>) {
        m_result.set_value(m_f());
    } else {
        m_f();
        m_result.set_value();
    }
}

template<typename result_type, typename callable_type>
std::future<result_type> command<result_type, callable_type>::get_future(void) {
    return m_result.get_future();
}

class ThreadEntity {
public:
    ThreadEntity(void);
    virtual ~ThreadEntity(void);
    DISALLOW_COPY_AND_MOVE(ThreadEntity);

    /* IF:
     * 1) the thread has been created but has never run, then initialize
     * and start running.
     * 2) the thread is paused, then resume it.
     * 3) the thread is stopped, then do nothing. A stopped thread entity can
     *    not be resumed.
     * 4) the thread is already running, then do nothing.
     *
     * Return false if the thread entity is stopped, else true.
     */
    bool run();

    /* More meaningful name in producer-consumer scenarios. */
    bool signal() { return run(); }

    /* Put the thread entity on pause; this is a state where the thread
     * entity is no longer running but can be resumed (woken up) */
    void pause(void);

    /* Stop the thread entity. Unlike pause, this is a permanent state.
     * The std::thread associated with the thread entity will be implicitly
     * exited and joined. */
    void stop(void);

    bool is_paused(void) const;
    bool is_stopped(void) const;
    bool is_running(void) const;

protected:
    enum class threadState : std::uint8_t {
        INITIALIZED,
        RUNNING,
        PAUSED,
        STOPPED
    };

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
     * as approprirate if any delays are needed, in order to avoid
     * busy-looping. However, care should be taken when calling wait_for
     * or wait_until. These will block either for a period of time or
     * until the thread entity is signalled (via run() or signal()),
     * whichever comes first. However, if signal()/run() is called while
     * do_work() is running but not during a call to wait_until/wait_for,
     * then the signal will be missed and wait_for/wait_until will block
     * for the whole specified time. That is, signals are edge-triggered,
     * not level-trigered, *unlike* with semaphores.
     *
     * (4)
     * Called on stopping the thread, before the main loop returns.
     * This is when stop() is called explicitly or in the dtor
     * of the thread entity.
     */
    virtual void initialize(void);     /* (1) */
    virtual void prepare_resume(void); /* (2) */
    virtual void do_work(void) = 0;    /* (3) */
    virtual void cleanup(void);        /* (4) */

    /*
     * Derived classes that need to wait for certain periods in do_work
     * should use one of these functions -- as opposed to sleeping.
     * This is because if stop() is called, the thread should be
     * responsive and unblock immediately. wait_until and wait_for
     * handle this case. OTOH a ThreadEntity engaged in a blocking sleep
     * inside do_work will hang until do_work returns, hence a stop()
     * request will block for a possibly unacceptably long period.
     *
     * NOTE: the sleep has a level-triggered interrupt sensitivity. That is to
     * say, if the thread entity is signaled *before* the sleep commences, then
     * wait_for/wait_until will clear the flag and then return immediately
     * without blocking. */
    void wait_until(std::chrono::steady_clock::time_point tp);
    void wait_for(std::chrono::microseconds duration);

private:
    void spawn(void);
    void loop(void);
    void wake(std::unique_lock<std::mutex> &&guard);

    std::thread m_thread;
    mutable std::mutex m_mtx;
    enum threadState m_state = threadState::STOPPED;
    std::condition_variable m_wait_cond;
    bool m_signaled;
};

/* A timer expiring at intervals dictated by the given TimeGuard.
 * The time guard can specify a fix period or something else
 * such as an exponentially increasing interval. In any case,
 * when the timer times out is completely dependent on the
 * timeguard */
template<typename T>
class Timer : public ThreadEntity {
public:
    Timer(std::unique_ptr<tarp::TimeGuard<T>> interval);

    auto &get_timeout_signal(void) { return m_timeout_signal; }

private:
    virtual void do_work(void) override final;
    virtual void initialize(void) override final;
    virtual void cleanup(void) override final;
    virtual void prepare_resume(void) override final;

    std::unique_ptr<tarp::TimeGuard<T>> m_guard;
    tarp::signal<void(void)> m_timeout_signal;
};

template<typename T>
Timer<T>::Timer(std::unique_ptr<tarp::TimeGuard<T>> interval)
    : m_guard(std::move(interval)) {
}

template<typename T>
void Timer<T>::do_work(void) {
    if (m_guard->disabled()) {
        set_state(threadState::STOPPED);
        return;
    }

    if (m_guard->down()) {
        /* NOTE: Any signal handlers should run quickly, in order
         * not to block the timer! */
        m_timeout_signal.emit();
        m_guard->shift(1);
        return;
    }

    if (m_guard->up()) {
        wait_until(m_guard->get_next_timepoint());
    }
}

template<typename T>
void Timer<T>::initialize(void) {
}

template<typename T>
void Timer<T>::cleanup(void) {
}

template<typename T>
void Timer<T>::prepare_resume(void) {
}

/* A timer with an adjustable period.
 *
 * On expiration of each period, the timer emits a 'tick'
 * signal. The period can be dynamically adjusted at any
 * given time. The tick frequency (i.e. the oscillation
 * frequency) could be varied in proportion to some quantity,
 * therefore implementing the software equivalent of something
 * like a VCO.
 *
 * To execute an action on every tick, either hook on to the
 * tick signal or override the on_tick method. NOTE: The signal
 * is emitted *after* on_tick returns.
 */
class Oscillator : public ThreadEntity {
public:
    auto &get_tick_signal(void) { return m_tick_signal; }

    /* Update the period of the timer. The first tick after the update happens
     * when an entire period has elapsed from the previous tick. Upon updating
     * the period, if the new period is much smaller than the old one, then it
     * is possible one or more (new) periods will have already elapsed since the
     * previous tick time point. In that case, one (and only one) tick signal
     * will be emitted instantly, after which the timer is advanced and brought
     * up to date.
     *
     * NOTE: in order to try to provide time-accurate ticks, the oscillator
     * will take into account how long the tick signal handler (or on_tick)
     * took when determining the amount of time left until the next tick.
     * NOTE: if the period of the oscillator is very small and the tick signal
     * handler (or on_tick) are slow to the extent that the ticks do not occur
     * at the expected intervals, then it is up to the user to either ensure
     * the handlers run faster or select a more appropriate, coarser, period.
     */
    void set_period(const std::chrono::microseconds &period);
    std::chrono::microseconds get_period() const;

protected:
    virtual void on_tick(void) {}

private:
    std::chrono::steady_clock::time_point time_now() const;

    virtual void do_work(void) override final;
    virtual void initialize(void) override final;
    virtual void cleanup(void) override final;
    virtual void prepare_resume(void) override final;

    static inline constexpr std::chrono::microseconds c_default_period {1s};
    mutable std::shared_mutex m_mtx;
    std::chrono::microseconds m_period {c_default_period};
    std::chrono::steady_clock::time_point m_prev_tick_tp;
    tarp::signal<void(void)> m_tick_signal;
};

/*
 * A subset of the Active Object design pattern, including internal
 * command queues. This achieves the goal of decoupling method invocation,
 * which happens in the thread of the caller, from method execution, which
 * happens in the active object thread.
 *
 * The command queue is hidden inside a Scheduler, which implements a queuing
 * discpline for scheduling tasks. See tarp/sched.hxx fmi.
 *
 * A task is an instance of `command` (see below) and embeds a callable
 * that expects no arguments and optionally returns a result. The callable is
 * executed in the active object thread and the result is returned
 * asynchronously via the promise/future mechanism. Derived classes may do one
 * of three things with the promise:
 * - block on it until a result is available. Then return the result to calling
 *   code.
 * - discard it and return nothing to the caller. I.e. do not wait for the task
 *   to be completed.
 * - return the promise to the caller thereby delegating to it the decision and
 *   responsibility of waiting for a result.
 *
 * Derived Classes
 * ---------------
 * Derived classes should override the do_work method (and any other methods as
 * appropriate) of the ThreadEntity as usual. Inside do_work, they can check
 * whether there are pending tasks using has_pending_tasks(). A task can be
 * dequeued using get_next_task() and then executed by invoking its execute()
 * method. The next task dequeued depends on the Scheduler (that is, on the
 * queue discipline used by it).
 */
class ActiveObject : public ThreadEntity {
public:
    /* The scheduler is specified via dependency injection in order to avoid
     * unnecessary templates. */
    explicit ActiveObject(std::unique_ptr<tarp::sched::Scheduler> sched =
                            std::make_unique<tarp::sched::SchedulerFifo>());

protected:
    bool has_pending_tasks() const;
    std::unique_ptr<task> get_next_task();

    /* Create a task based on the future-promise mechanism.
     * This will be scheduled for execution according to
     * the scheduler's queue discipline.
     *
     * NOTE: previously, some additional helpers were implemented so as to
     * further simplify the creation of a task e.g.
     * enqueue_task_and_wait_until_done. However, building the 'execution
     * context' gets really hairy and error prone. For example, some of the
     * arguments should be copied, others captured by reference, others moved.
     * Client code is in the best position to do this, therefore the
     * responsibility for creating the callable logic is offloaded to derived
     * classes.
     * Typically, this is going to to be a small lambda that captures everything
     * it needs using the appropriate semantics (move/copy/reference).
     */
    template<typename callable_type>
    auto schedule_task(callable_type &&func)
      -> std::future<std::invoke_result_t<callable_type>> {
        auto task_item = std::make_unique<
          command<std::invoke_result_t<callable_type>, callable_type>>(
          std::forward<decltype(func)>(func));

        auto future = task_item->get_future();

        {
            std::unique_lock l(m_scheduler_mtx);
            m_scheduler->enqueue(std::move(task_item));
        }

        /* wake up the active object if idling */
        signal();

        return future;
    }

private:
    std::unique_ptr<tarp::sched::Scheduler> m_scheduler;
    mutable std::mutex m_scheduler_mtx;
};

/*
 * An extension of a ThreadEntity to make it better suited as a worker thread in
 * a thread pool. The WorkerThread follows moves through a basic sequence of
 * states:
 * - if no next task, pause.
 * - if given a task and signaled, run that task.
 */
class WorkerThread : public ThreadEntity {
public:
    /* Create a new worker thread. ID is a unique identifier for a given
     * thread. Convenient for looking up the threads in hash tables and such.
     * NOTE: the reason that std::thread_id is not used instead is that it might
     * be useful to be in control of allocating the thread identifiers. For
     * example, we could associate certain ID ranges with higher priorities
     * or associate an ID with certain events only.
     * Therefore the id is more of a *worker* id than a *thread* id.
     */
    WorkerThread(std::uint32_t worker_id);

    std::size_t get_worker_id() const;

    /* Signal emitted when the worker has completed its current task and is
     * ready for new work. In leader-followers terminology, the signal is
     * emitted when a processing thread is now ready to become a follower
     * again. Therefore this is a notifying signal meant for a thread pool. */
    auto &get_idle_signal() { return m_sig_work_done; }

    /*
     * Set the next task (work item) to be done by the worker thread.
     * NOTE: this function overwrites the previous value. Therefore it should
     * only be called when the worker has finished the previous task.
     */
    void set_task(std::unique_ptr<task> task);

private:
    virtual void do_work(void) override final;
    virtual void initialize(void) override final;
    virtual void cleanup(void) override final;
    virtual void prepare_resume(void) override final;

    /* When doing a task, we take the next task and make it current;
     * this is to prevent the task from being destroyed if the user called
     * set_task() while we are in the middle of doing that task */
    std::unique_ptr<tarp::threading::task> m_next_task;
    std::unique_ptr<tarp::threading::task> m_current_task;

    mutable std::mutex m_mtx;
    const std::uint32_t m_worker_id;
    tarp::signal<void(std::uint32_t id)> m_sig_work_done;
};

/*
 * Implementation of a thread pool.
 * The number of worker threads is specified at construction time but can be
 * dynamically adjusted (up to a system-dependent maximum).
 * The thread pool takes tarp::threading::task's as input and passes the tasks
 * on to workers in the thread pool. A default FIFO queueing discipline is used
 * for scheduling the tasks but another scheduler can be specified in the
 * CTOR.
 *
 * The implementation is roughly based on the Leader-followers design pattern.
 */
class ThreadPool final : public ThreadEntity {
public:
    /* Inherited from ThreadEntity. A thread pool cannot be paused. */
    void pause(void) = delete;

    /* Create a thread pool with an initial size of num_workers and using the
     * specified queue discpline for the tasks in the run queue. */
    explicit ThreadPool(uint16_t num_workers,
                        std::unique_ptr<tarp::sched::Scheduler> scheduler =
                          std::make_unique<tarp::sched::SchedulerFifo>());

    /* Get number of tasks queued waiting for execution */
    std::size_t get_queue_length() const;

    /* total number of tasks handled across all workers combined */
    std::size_t get_num_tasks_handled() const;

    /* Schedule a task for execution */
    void enqueue_task(std::unique_ptr<tarp::threading::task> task);

    /* Get the number of worker threads i.e. the size of the worker pool. */
    std::size_t get_num_threads() const;

    /* Resize the worker pool. NOTE: if n is smaller than the previous value
     * and all workers are busy, the request will not be honored immediately.
     * This is since the workers can only be destroyed once they've completed
     * whatever current tak they are in the middle of. */
    void set_num_threads(std::size_t n);

    /* Spawn all the worker threads and start assigning tasks if any are
     * scheduled. */
    void start();

private:
    virtual void initialize(void) override final;
    virtual void prepare_resume(void) override final;

    /* main loop; dispatches tasks to workers and resizes the thread
     * pool as requested */
    void do_work() override;

    /* Stop and destroy all the workers.
     * NOTE: This is a permanent action: the thread pool cannot be restarted
     * once paused.
     * NOTE: This function will block until all workers have been stopped. A
     * worker may only be stopped when idle i.e. once it has completed whatever
     * current task it is in the middle of. */
    virtual void cleanup(void) override final;

    void resize_pool_if_needed(void);
    void add_follower(uint32_t worker_id);
    void hook_up_task_completion_signal(tarp::threading::WorkerThread &worker);

    mutable std::shared_mutex m_mtx;
    std::size_t m_num_workers {0};    /* number of user-requested workers */
    std::size_t m_next_worker_id {0}; /* monotically incrementing */

    /* all the workers in the current thread pool */
    std::map<std::uint32_t, std::shared_ptr<tarp::threading::WorkerThread>>
      m_threads;

    /* workers not currently engaged in processing a task. i.e. workers that
     * are available to take on new work ('followers'). */
    std::list<std::shared_ptr<tarp::threading::WorkerThread>> m_idle_threads;

    std::map<uint32_t, std::unique_ptr<tarp::signal_connection>>
      m_worker_signals;

    const std::unique_ptr<tarp::sched::Scheduler> m_taskq;
    std::uint64_t m_num_tasks_handled {0};
};

}  // namespace threading
}  // namespace tarp
