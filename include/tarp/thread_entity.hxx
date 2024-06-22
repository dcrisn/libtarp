#pragma once

// C++ stdlib
#include <chrono>
#include <condition_variable>
#include <future>
#include <shared_mutex>
#include <stdexcept>
#include <thread>

#include <tarp/cxxcommon.hxx>
#include <tarp/sched.hxx>
#include <tarp/signal.hxx>
#include <tarp/timeguard.hxx>
#include <type_traits>

namespace tarp {
namespace threading {

using namespace std::chrono_literals;

// TODO: extend the thread entity interface to make make it easy to implement
// thread pools using the leader-follower pattern.

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
    /* Abstract task interface; it is important that this be a non-template
     * so that we do not need to templatize the scheduler and everything else
     * that interacts with tasks merely at an interface level! */
    class task : public tarp::sched::QueueItem {
    public:
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

        void execute() override;
        std::future<result_type> get_future();

        std::string get_pretty_name() const override { return ""; }

    private:
        std::promise<result_type> m_result;
        callable_type m_f;
    };

    //

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
    auto schedule_task(callable_type &&func) const
      -> std::future<std::invoke_result_t<callable_type>> {
        auto task_item = std::make_unique<
          command<std::invoke_result_t<callable_type>, callable_type>>(
          std::forward<decltype(func)>(func));

        auto future = task_item->get_future();

        {
            std::unique_lock l(m_scheduler_mtx);
            m_scheduler->enqueue(std::move(task_item));
        }

        return future;
    }

    bool has_pending_tasks() const {
        std::unique_lock l {m_scheduler_mtx};
        return m_scheduler->get_queue_length() > 0;
    }

    std::unique_ptr<task> get_next_task() {
        std::unique_lock l {m_scheduler_mtx};

        auto queue_item = m_scheduler->dequeue().release();
        auto task_to_handle = dynamic_cast<task *>(queue_item);

        // NOTE: if the exception throws there will be a leak
        // because nothing frees the queue_item pointer. This is benign
        // since this exception should never be raised if the code is
        // correct.
        if (!task_to_handle) {
            throw std::logic_error(
              "BUG: cannot get task from QueueItem in ActiveObject");
        }

        // release the unique pointer ownership of the QueueItem
        // and create a new one to own the task pointer instead.
        // Unfortunately, this whole song and dance needs to be done
        // here because dynamic_cast does not work on std::unique_ptr.
        return std::unique_ptr<task>(task_to_handle);
    }

private:
    std::unique_ptr<tarp::sched::Scheduler> m_scheduler;
    mutable std::mutex m_scheduler_mtx;
};

template<typename result_type, typename callable_type>
ActiveObject::command<result_type, callable_type>::command(callable_type &&func)
    : m_f(func) {
}

template<typename return_type, typename callable_type>
void ActiveObject::command<return_type, callable_type>::execute(void) {
    /* If the callable returns a result, we must commmunicate it to
     * the future. If it does not return a result (i.e. it returns void),
     * we must still call .set_value() on the future without arguments
     * in order to unblock the future since the promise is now complete.
     */
    if constexpr (!std::is_void_v<std::invoke_result_t<decltype(m_f)>>) {
        m_result.set_value(m_f());
    } else {
        m_f();
        m_result.set_value();
    }
}

template<typename result_type, typename callable_type>
std::future<result_type>
ActiveObject::command<result_type, callable_type>::get_future(void) {
    return m_result.get_future();
}


}  // namespace threading
}  // namespace tarp
