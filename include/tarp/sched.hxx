#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <future>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <type_traits>

#include <tarp/cancellation_token.hxx>
#include <tarp/common.h>
#include <tarp/cxxcommon.hxx>
#include <tarp/filters.hxx>
#include <tarp/signal.hxx>
#include <tarp/type_traits.hxx>

//
namespace tarp::sched {

enum class queueingDiscipline : std::uint8_t {
    FIFO,
    LIFO,
    PRIO, /* NOTE: max-heap semantics */
};

using qdisc = queueingDiscipline;

//

/*
 * Abstract Interface for a Scheduler aka Queueing Discpline.
 * A scheduler is associated with one or more internal queues
 * and is defined by an algorithm as to the order that enqueued
 * items are dequeued. The most rudimentary example is a FIFO
 * scheduer where items are simply dequeued in the order
 * that they were enqueued in: first in, first out. Arbitrarily
 * complex queueing algorithms can be used however.
 */
template<typename queue_item_t>
class Scheduler {
protected:
    virtual void do_enqueue(std::unique_ptr<queue_item_t> item) = 0;
    virtual std::unique_ptr<queue_item_t> do_dequeue() = 0;

public:
    DISALLOW_COPY_AND_MOVE(Scheduler);

    /*
     * The ID is used to identify and refer to a scheduler instance inside a
     * hierarchy and must be unique within that hierarchy.  */
    explicit Scheduler(uint32_t id) : m_id(id) {}

    virtual ~Scheduler() = default;

    void enqueue(std::unique_ptr<queue_item_t> item) {
        if (!item) {
            throw std::invalid_argument(
              "Illegal attempt to enqueue unacceptable NULL value");
        }
        do_enqueue(std::move(item));
    }

    virtual std::unique_ptr<queue_item_t> dequeue() { return do_dequeue(); }

    /* Return the number of enqueued items. Not all enqueued items are
     * necessarily dequeueable at some specific time. For example, some
     * Schedulers may buffer, delay, duplicate, or drop items in the queue. */
    virtual std::size_t get_queue_length() const = 0;

    ///* Number of items in the queue that may actually be dequed */
    // virtual std::size_t num_dequeable() const = 0;

    /* Discard all enqueued items. */
    virtual void clear() = 0;

    bool empty() const { return get_queue_length() == 0; }

    uint32_t get_id() const { return m_id; }

    bool attach_parent(uint32_t parent_id, Scheduler<queue_item_t> &parent);
    bool detach_parent(uint32_t parent_id);

    using filter_type = sched::filters::Filter<queue_item_t>;
    void attach_filter(uint32_t filter_id,
                       std::shared_ptr<filter_type> filter,
                       uint32_t destination_child_id);

    void detach_filter(uint32_t filter_id);

private:
    uint32_t m_id;

    void attach_child(uint32_t child_id, Scheduler<queue_item_t> &child);
    void detach_child(uint32_t child_id);

    std::shared_ptr<Scheduler> m_parent;
    std::map<std::uint32_t, Scheduler<queue_item_t> *> m_children;

    // filter_id, <filter, destination_child_id
    std::map<uint32_t, std::pair<std::shared_ptr<filter_type>, uint32_t>>
      m_filters;
};

//

#if __cplusplus >= 202002L
// use concepts

template<typename T>
concept fifo_qitif = true;


template<typename T>
concept deadline_qitif = requires(const T &t) {
    { t.expired() } -> std::same_as<bool>;

    {
        t.get_expiration_time()
    } -> std::same_as<std::chrono::system_clock::time_point>;
};

#else
// in the absence of concepts, use hacks to enforce interface constaints.

/* Queue Item interfaces required by various schedulers.
 * NOTE: qitif = 'queue item template interface', for brevity. */
struct fifo_qitif {
    template<typename C>
    struct constraints;
};

struct deadline_qitif {
    // clang-format off
    template <typename C,
        VALIDATE(&C::expired, bool (C::*)() const),
        VALIDATE(&C::get_expiration_time, std::chrono::system_clock::time_point (C::*)() const)
             >
    struct constraints;
    // clang-format on
};
#endif

//

#if __cplusplus >= 202002L
template<fifo_qitif queue_item_t>
class SchedulerFifo final : public Scheduler<queue_item_t> {
#else
template<typename queue_item_t>
class SchedulerFifo final : public Scheduler<queue_item_t> {
    REQUIRE(queue_item_t, fifo_qitif);
#endif
public:
    explicit SchedulerFifo(uint32_t id = 0) : Scheduler<queue_item_t>(id) {}

    virtual std::size_t get_queue_length() const override { return m_q.size(); }

    virtual void clear() override { m_q.clear(); }

private:
    virtual void do_enqueue(std::unique_ptr<queue_item_t> item) override {
        m_q.push_back(std::move(item));
    }

    virtual std::unique_ptr<queue_item_t> do_dequeue() override {
        if (m_q.empty()) {
            return nullptr;
        }

        auto head = std::move(m_q.front());
        m_q.pop_front();
        return head;
    }

    std::deque<std::unique_ptr<queue_item_t>> m_q;
};

/*
 * A deadline scheduler. It sorts its queued items in order of expiration time
 * and imposes a FIFO ordering on them: that is, the item expiring first gets
 * dequed first. The sort is stable: items with an equal expiration time
 * maintain their relative order.
 *
 * NOTE: an item may only be dequed when its expiration time arrives; up until
 * that point it is buffered in the queue.
 */
#if __cplusplus >= 202002L
template<deadline_qitif queue_item_t>
class SchedulerDeadline final : public Scheduler<queue_item_t> {
#else
template<typename queue_item_t>
class SchedulerDeadline final : public Scheduler<queue_item_t> {
    REQUIRE(queue_item_t, deadline_qitif);
#endif
public:
    SchedulerDeadline(uint32_t id = 0) : Scheduler<queue_item_t>(id) {};

    virtual std::size_t get_queue_length() const override { return m_q.size(); }

    virtual void clear() override { m_q.clear(); }

    std::optional<std::chrono::system_clock::time_point>
    get_first_deadline() const {
        if (m_q.empty()) {
            return std::nullopt;
        }
        return m_q.front()->get_expiration_time();
    }

private:
    virtual void do_enqueue(std::unique_ptr<queue_item_t> item) override {
        m_q.emplace_back(std::move(item));

        using param_type = const std::unique_ptr<queue_item_t> &;
        m_q.sort([](param_type a, param_type b) {
            return a->get_expiration_time() < b->get_expiration_time();
        });
    }

    virtual std::unique_ptr<queue_item_t> do_dequeue() override {
        if (m_q.empty()) {
            return nullptr;
        }

        /* Buffer items until they are actually expired. */
        if (!m_q.front()->expired()) {
            return nullptr;
        }

        auto front = std::move(m_q.front());
        m_q.pop_front();
        return std::move(front);
    }

    inline auto time_now() const { return std::chrono::system_clock::now(); }

    std::list<std::unique_ptr<queue_item_t>> m_q {};
};

//

namespace interfaces {
/*
 * Abstract interface for a task. A task is a packaged piece of executable code.
 *
 * NOTE: it is important that this be a non-template
 * so that we do not need to templatize task containers and everything else
 * that only interact with tasks at an interface level. */
class task {
public:
    virtual ~task() = default;
    virtual void execute(void) = 0;
    virtual std::string get_name() const = 0;
};

/*
 * A task with the additional semantics of being undo-able
 * and redo-able. */
class command {
public:
    virtual ~command() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual void redo() = 0;
    virtual std::string get_name() const = 0;
};

/* A task that:
 *  - has a deadline (expiration time).
 *  - can be executed once (one-shot) or at an interval.
 *
 * NOTE: an already-expired deadline (a deadline of NOW or in the past)
 * is effectively the same as having *no* deadline. Therefore this task
 * can be used for simple tasks that must be enqueued once only.
 */
class deadline_task {
public:
    virtual ~deadline_task() = default;

    /* Execute the function bound to this task */
    virtual void execute() = 0;

    /* True if we are at or past the deadline, else False */
    virtual bool expired() const = 0;

    virtual std::chrono::system_clock::time_point
    get_expiration_time() const = 0;

    /* Postpone the expiration time by the specified delay value */
    virtual void delay(std::chrono::microseconds delay) = 0;

    /* True if the task can be 'renewed' -- that is re-armed and re-scheduled
     * for another execution. */
    virtual bool renewable() const = 0;

    /* Move the expiration time into the future so the task is considered not to
     * have expired. This is usually called before re-scheduling a renewable
     * task. */
    virtual void renew() = 0;
};

}  // namespace interfaces

//

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
class task : public sched::interfaces::task {
public:
    task(callable_type func,
         const std::string &name = "",
         std::optional<cancellation_token> = {});

    ~task() override = default;

    void execute() override;
    std::future<result_type> get_future();

    std::string get_name() const override;

private:
    const std::string m_name;
    std::optional<cancellation_token> m_cancellation_token;
    std::promise<result_type> m_result;
    std::remove_reference_t<callable_type> m_f;
};

/* Create and store a task in a unique_ptr to a task or one of its parent
 * classes. */
template<typename abc, typename callable_type>
std::pair<std::unique_ptr<abc>,
          std::future<std::invoke_result_t<callable_type>>>
make_task_as(callable_type f, std::optional<cancellation_token> token = {}) {
    using return_type = std::invoke_result_t<callable_type>;
    using task_type = sched::task<return_type, callable_type>;

    static_assert(std::is_base_of_v<abc, task_type>);

    auto task = new task_type(std::move(f), "", token);
    auto future = task->get_future();

    return std::make_pair(std::unique_ptr<abc>(task), std::move(future));
}

template<typename result_type, typename callable_type>
task<result_type, callable_type>::task(callable_type func,
                                       const std::string &name,
                                       std::optional<cancellation_token> token)
    : m_f(std::move(func))
    , m_name(name)
    , m_cancellation_token(std::move(token)) {
}

template<typename result_type, typename callable_type>
std::string task<result_type, callable_type>::get_name() const {
    return m_name;
}

template<typename return_type, typename callable_type>
void task<return_type, callable_type>::execute(void) {
    if (m_cancellation_token->canceled()) {
        return;
    }

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
std::future<result_type> task<result_type, callable_type>::get_future(void) {
    return m_result.get_future();
}

//

// Task that is executed every period for (optionally) a set number of
// periods.
// This class implements much of the periodic_task interface that does
// not involve templates. Not meant to be instantiated directly, only
// derived from by concrete subclasses.
class periodic_task_mixin : public interfaces::deadline_task {
public:
    // If starts_expired=true, then the deadline is in the present.
    // That is, the task starts off with expired=true. Otherwise if
    // starts_expired=false, the initial deadline is INTERVAL from NOW.
    explicit periodic_task_mixin(std::chrono::milliseconds interval,
                                 std::optional<std::size_t> max_num_renewals,
                                 bool starts_expired,
                                 std::optional<cancellation_token> token = {});

    bool expired() const override;
    std::chrono::system_clock::time_point get_expiration_time() const override;
    void delay(std::chrono::microseconds delay) override;
    bool renewable() const override;
    void renew() override;

protected:
    void set_expiration(std::chrono::system_clock::time_point deadline);
    std::chrono::system_clock::time_point time_now() const;
    std::chrono::milliseconds get_interval() const;
    bool starts_expired() const;

    std::optional<std::size_t> get_max_num_renewals() const;

    bool canceled() const {
        if (!m_cancellation_token.has_value()) {
            return false;
        }
        return m_cancellation_token->canceled();
    }

private:
    std::optional<cancellation_token> m_cancellation_token;
    std::chrono::system_clock::time_point m_next_deadline;
    std::chrono::milliseconds m_interval;
    std::optional<std::size_t> m_max_num_renewals;
    std::size_t m_num_renewals {0};
    bool m_start_expired;
};

// Logically, a periodic_task is an extension of task. However, this risks
// resulting in a complex inheritance tree and requiring virtual inheritance.
// That is best avoided (both because of unnecessary overhead and more complex
// behavior), so keep these as two separate inheritance hierarchies.
template<typename result_type, typename callable_type>
class periodic_task final : public periodic_task_mixin {
public:
    explicit periodic_task(std::chrono::milliseconds interval,
                           std::optional<std::size_t> max_num_renewals,
                           bool starts_expired,
                           callable_type f,
                           std::optional<cancellation_token> token = {})
        : periodic_task_mixin(interval,
                              max_num_renewals,
                              starts_expired,
                              std::move(token))
        , m_f(std::move(f)) {}

    ~periodic_task() = default;

    void execute() override;

    /* NOTE: Ideally we would like to use a std::future here as a way to signal
     * the completion of a task, without risk of blocking. However, the
     * issue is that a std::promise may only be set once. A new one would need
     * to be created for every execution, which means the user would need to
     * keep getting the newly associated future, which is very inconvenient. The
     * signal mechanism here is much more flexible, but it is blocking: one
     * single slow handler will block the task indefinitely. Worse, **in a
     * multithreaded application signals can easily cause deadlocks**. Signal
     * handlers therefore must run very fast and not block for any period of
     * time and not do anything that needs locks or runs the risk of deadlocks.
     * It is the responsibility of the user to ensure this is the case. */
    auto &signal_result() { return m_result; }

private:
    using signal_signature = type_traits::signature_comp_t<void, result_type>;
    tarp::ts::signal<signal_signature> m_result;

    std::remove_reference_t<callable_type> m_f;
};

/* Return a unique_ptr to an abc (abstract base class) that stores a periodic
 * task. */
template<typename abc, typename callable_type>
std::unique_ptr<abc>
make_periodic_task_as(std::chrono::milliseconds interval,
                      std::optional<std::size_t> max_num_renewals,
                      bool starts_expired,
                      callable_type f,
                      std::optional<cancellation_token> token = {}) {
    using return_type = std::invoke_result_t<callable_type>;
    using interval_task_type = sched::periodic_task<return_type, callable_type>;

    static_assert(std::is_base_of_v<abc, interval_task_type>);

    auto *ptr = (new interval_task_type(interval,
                                        max_num_renewals,
                                        starts_expired,
                                        std::move(f),
                                        std::move(token)));

    return std::unique_ptr<abc>(ptr);
}

template<typename return_type, typename callable_type>
void periodic_task<return_type, callable_type>::execute(void) {
    if (canceled()) {
        return;
    }

    if constexpr (!std::is_void_v<std::invoke_result_t<decltype(m_f)>>) {
        m_result.emit(m_f());
    } else {
        m_f();
        m_result.emit();
    }
}

//

// A specialized case of a periodic_task with one single expiration.
// I.e. a non-renewable periodic_task.
template<typename result_type, typename callable_type>
class deadline_task final : public periodic_task_mixin {
public:
    explicit deadline_task(std::chrono::milliseconds expires_from_now,
                           callable_type f,
                           std::optional<cancellation_token> token = {})
        : periodic_task_mixin(expires_from_now, 0, false, std::move(token))
        , m_f(std::move(f)) {}

    void execute() override;
    std::future<result_type> get_future();

private:
    std::remove_reference_t<callable_type> m_f;
    std::promise<result_type> m_result;
};

/* Return a unique_ptr to an abc (abstract base class) that stores a deadline
 * task. */
template<typename abc, typename callable_type>
std::pair<std::unique_ptr<abc>,
          std::future<std::invoke_result_t<callable_type>>>
make_deadline_task_as(std::chrono::milliseconds expires_from_now,
                      callable_type f,
                      std::optional<cancellation_token> token = {}) {
    using return_type = std::invoke_result_t<callable_type>;
    using task_t = sched::deadline_task<return_type, callable_type>;

    static_assert(std::is_base_of_v<abc, task_t>);

    auto *ptr = new task_t(expires_from_now, std::move(f), std::move(token));
    auto future = ptr->get_future();

    return std::make_pair(std::unique_ptr<abc>(ptr), std::move(future));
}

template<typename return_type, typename callable_type>
void deadline_task<return_type, callable_type>::execute(void) {
    if (canceled()) {
        return;
    }

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
std::future<result_type>
deadline_task<result_type, callable_type>::get_future(void) {
    return m_result.get_future();
}


}  // namespace tarp::sched
