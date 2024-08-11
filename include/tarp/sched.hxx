#pragma once

#include <cstdint>
#include <deque>
#include <future>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <type_traits>

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

    virtual ~Scheduler() {}

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

//

template<typename queue_item_t>
class SchedulerFifo : public Scheduler<queue_item_t> {
public:
    REQUIRE(queue_item_t, fifo_qitif);

    explicit SchedulerFifo(uint32_t id = 0) : Scheduler<queue_item_t>(id) {}

    virtual std::size_t get_queue_length() const override;
    virtual void clear() override;

private:
    virtual void do_enqueue(std::unique_ptr<queue_item_t> item) override;
    virtual std::unique_ptr<queue_item_t> do_dequeue() override;

    std::deque<std::unique_ptr<queue_item_t>> m_q;
};

template<typename queue_item_t>
void SchedulerFifo<queue_item_t>::do_enqueue(
  std::unique_ptr<queue_item_t> item) {
    m_q.push_back(std::move(item));
}

template<typename queue_item_t>
std::unique_ptr<queue_item_t> SchedulerFifo<queue_item_t>::do_dequeue() {
    if (m_q.empty()) {
        return nullptr;
    }

    auto head = std::move(m_q.front());
    m_q.pop_front();
    return head;
}

template<typename queue_item_t>
std::size_t SchedulerFifo<queue_item_t>::get_queue_length() const {
    return m_q.size();
}

template<typename queue_item_t>
void SchedulerFifo<queue_item_t>::clear() {
    m_q.clear();
}

/*
 * A deadline scheduler. It sorts its queued items in order of expiration time
 * and imposes a FIFO ordering on them: that is, the item expiring first gets
 * dequed first. The sort is stable: items with an equal expiration time
 * maintain their relative order.
 *
 * NOTE: an item may only be dequed when its expiration time arrives; up until
 * that point it is buffered in the queue.
 */
template<typename queue_item_t>
class SchedulerDeadline : public Scheduler<queue_item_t> {
public:
    REQUIRE(queue_item_t, deadline_qitif);

    SchedulerDeadline(uint32_t id = 0) : Scheduler<queue_item_t>(id) {};
    virtual std::size_t get_queue_length() const override;
    virtual void clear() override;

private:
    virtual void do_enqueue(std::unique_ptr<queue_item_t> item) override;
    virtual std::unique_ptr<queue_item_t> do_dequeue() override;

    inline auto time_now() const { return std::chrono::system_clock::now(); }

    std::list<std::unique_ptr<queue_item_t>> m_q {};
};

template<typename queue_item_t>
void SchedulerDeadline<queue_item_t>::do_enqueue(
  std::unique_ptr<queue_item_t> item) {
    m_q.emplace_back(std::move(item));

    using param_type = const std::unique_ptr<queue_item_t> &;
    m_q.sort([](param_type a, param_type b) {
        return a->get_expiration_time() < b->get_expiration_time();
    });
}

template<typename queue_item_t>
std::unique_ptr<queue_item_t> SchedulerDeadline<queue_item_t>::do_dequeue() {
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

template<typename queue_item_t>
std::size_t SchedulerDeadline<queue_item_t>::get_queue_length() const {
    return m_q.size();
}

template<typename queue_item_t>
void SchedulerDeadline<queue_item_t>::clear() {
    m_q.clear();
}

//

namespace interfaces {
/*
 * Abstract interface for a task. A task is a packaged piece of executable code.
 *
 * NOTE: it is important that this be a non-template
 * so that we do not need to templatize task containers and everything else
 * that interacts with tasks merely at an interface level. */
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
 *  - has a deadline
 *  - can be executed once (one-shot) or at an interval.
 *  - returns a 'future' that can be waited on to get the actual result of
 *  executing a callback.
 *
 * NOTE: an already-expired deadline (a deadline of NOW or in the past) is
 * effectively the same as having *no* deadline. Therefore this task can be used
 * for simple tasks that must be enqueued once only.
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
    task(callable_type &&func, const std::string &name = "");
    ~task() override {};

    void execute() override;
    std::future<result_type> get_future();

    std::string get_name() const;

private:
    std::string m_name;
    std::promise<result_type> m_result;
    callable_type m_f;
};

/* Create and store a task in a unique_ptr to a task or one of its parent
 * classes. */
template<typename abc, typename callable_type>
std::pair<std::unique_ptr<abc>,
          std::future<std::invoke_result_t<callable_type>>>
make_task_as(callable_type &&f) {
    using return_type = std::invoke_result_t<callable_type>;
    using task_type = sched::task<return_type, callable_type>;

    static_assert(std::is_base_of_v<abc, task_type>);

    auto task = new task_type(std::forward<decltype(f)>(f));
    auto future = task->get_future();

    return std::make_pair(std::unique_ptr<abc>(task), std::move(future));
}

template<typename result_type, typename callable_type>
task<result_type, callable_type>::task(callable_type &&func,
                                       const std::string &name)
    : m_f(func), m_name(name) {
}

template<typename result_type, typename callable_type>
std::string task<result_type, callable_type>::get_name() const {
    return m_name;
}

template<typename return_type, typename callable_type>
void task<return_type, callable_type>::execute(void) {
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

// Implements much of the interval_task interface that does not involve
// templates. Not meant to be instantiated directly, only derived from by
// concrete subclasses.
class interval_task_mixin : public interfaces::deadline_task {
public:
    /* If starts_expired=true, then the deadline is in the present. That is, the
     * task starts off with expired=true. Otherwise if starts_expired=false, the
     * initial deadline is INTERVAL from NOW.
     *
     * NOTE: f is copied by value so it can be saved internally and used when
     * renewing.
     */
    explicit interval_task_mixin(std::chrono::milliseconds interval,
                                 std::optional<std::size_t> max_num_expirations,
                                 bool starts_expired);

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

private:
    std::chrono::system_clock::time_point m_next_deadline;
    std::chrono::milliseconds m_interval;
    std::optional<std::size_t> m_max_num_renewals;
    std::size_t m_num_renewals {0};
    bool m_start_expired;
};

// Logically, an interval_task is an extension of task. However, this risks
// resulting in a complex inheritance tree and requiring virtual inheritance.
// That is best avoided (both because of unnecessary overhead and more complex
// behavior), so keep these as two separate inheritance hierarchies.
template<typename result_type, typename callable_type>
class interval_task : public interval_task_mixin {
public:
    explicit interval_task(std::chrono::milliseconds interval,
                           std::optional<std::size_t> max_num_expirations,
                           bool starts_expired,
                           callable_type f)
        : interval_task_mixin(interval, max_num_expirations, starts_expired)
        , m_f(std::move(f)) {}

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
    using signal_signature =
      type_traits::signature_comp_t<void, result_type>;
    tarp::signal<signal_signature> m_result;

    callable_type m_f;
};

/* Return a unique_ptr to an abc (abstract base class) that stores a command. */
template<typename abc, typename callable_type>
std::unique_ptr<abc>
make_interval_task_as(std::chrono::milliseconds interval,
                      std::optional<std::size_t> max_num_expirations,
                      bool starts_expired,
                      callable_type &&f) {
    using return_type = std::invoke_result_t<callable_type>;
    using interval_command_type =
      sched::interval_task<return_type, callable_type>;

    static_assert(std::is_base_of_v<abc, interval_command_type>);

    auto *ptr = (new interval_command_type(interval,
                                           max_num_expirations,
                                           starts_expired,
                                           std::forward<decltype(f)>(f)));

    return std::unique_ptr<abc>(ptr);
}

template<typename return_type, typename callable_type>
void interval_task<return_type, callable_type>::execute(void) {
    /* If the callable returns a result, we must commmunicate it to
     * the future. If it does not return a result (i.e. it returns void),
     * we must still call .set_value() on the future without arguments
     * in order to unblock the future since the promise is now fulfilled.
     */
    if constexpr (!std::is_void_v<std::invoke_result_t<decltype(m_f)>>) {
        m_result.emit(m_f());
    } else {
        m_f();
        m_result.emit();
    }
}

//

template<typename result_type, typename callable_type>
class deadline_task : public interval_task_mixin {
public:
    explicit deadline_task(std::chrono::milliseconds expires_from_now,
                           callable_type f)
        : interval_task_mixin(expires_from_now, 0, false), m_f(std::move(f)) {}

    void execute() override;
    std::future<result_type> get_future();

    // use std::future here; this is semantically a one-shot so that will work
    // fine.
private:
    callable_type m_f;
    std::promise<result_type> m_result;
};

/* Return a unique_ptr to an abc (abstract base class) that stores a deadline
 * task. */
template<typename abc, typename callable_type>
std::future<std::invoke_result_t<callable_type>>
make_deadline_task_as(std::chrono::milliseconds expires_from_now,
                      callable_type &&f) {
    using return_type = std::invoke_result_t<callable_type>;
    using task_t = sched::deadline_task<return_type, callable_type>;

    static_assert(std::is_base_of_v<abc, task_t>);

    auto *ptr = new task_t(expires_from_now, std::forward<decltype(f)>(f));
    auto future = ptr->get_future();

    return std::make_pair(std::unique_ptr<abc>(ptr), std::move(future));
}

template<typename return_type, typename callable_type>
void deadline_task<return_type, callable_type>::execute(void) {
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
