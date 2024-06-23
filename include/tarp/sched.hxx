#pragma once

/*
 */

#include <deque>
#include <memory>
#include <optional>

#include <cstdint>

#include <tarp/cxxcommon.hxx>

namespace tarp::sched {

class QueueItem {
public:
    virtual ~QueueItem();

    std::optional<std::uint8_t> get_priority() const;
    void set_priority(std::uint8_t);
    virtual std::string get_pretty_name() const = 0;

private:
    std::optional<std::uint8_t> m_priority;
};

/* Convert a QueueItem unique_ptr to a unique_ptr to type T.
 * Return nullptr on failure.
 *
 * NOTE: if the cast is successful, queue_item will be reset to null and will
 * no longer own its raw pointer. Otherwise if the cast fails, nothing changes:
 * queue_item retains ownership of its pointer and nullptr is returned.
 */
template<typename T>
std::unique_ptr<T>
queue_item_cast(std::unique_ptr<tarp::sched::QueueItem> &queue_item) {
    if (!queue_item) {
        return nullptr;
    }

    auto *raw_queue_item = queue_item.get();
    auto raw_converted = dynamic_cast<T *>(raw_queue_item);

    if (!raw_converted) {
        return nullptr;
    }

    // result is marked nodiscard, so this is to shut up the compiler.
    [[maybe_unused]] auto tmp = queue_item.release();

    // Release the unique pointer ownership of the QueueItem
    // and create a new one to own the T pointer instead.
    // Unfortunately, this whole song and dance here is necessary because
    // C++ does not support casting unique_ptr types (unlike shared_ptr which
    // does have support for casting via *_pointer_cast).
    return std::unique_ptr<T>(raw_converted);
}


enum class queueDiscipline : std::uint8_t {
    FIFO,
    LIFO,
    PRIO, /* NOTE: max-heap semantics */
};

using qdisc = queueDiscipline;

/*
 * Abstract Interface for a Scheduler aka Queue Discpline.
 * A scheduler is associated with one or more internal queues
 * and is defined by an algorithm as to the order that enqueued
 * items are dequeued. The most rudimentary example is a FIFO
 * queueing discpline where items are simply dequeued in the order
 * that they were enqueued in: first in, first out. Arbitrarily
 * complex queueing algorithms can be used however.
 */
class Scheduler {
public:
    DISALLOW_COPY_AND_MOVE(Scheduler);
    explicit Scheduler();
    virtual ~Scheduler();

    virtual void enqueue(std::unique_ptr<QueueItem> item) = 0;
    virtual std::unique_ptr<QueueItem> dequeue() = 0;

    /* Return the number of enqueued items */
    virtual std::size_t get_queue_length() const = 0;

    bool empty() const;

    /* Discard all enqueued items. */
    virtual void clear() = 0;
};

class SchedulerFifo : public Scheduler {
public:
    explicit SchedulerFifo();

    virtual void enqueue(std::unique_ptr<QueueItem> item) override;
    virtual std::unique_ptr<QueueItem> dequeue() override;
    virtual std::size_t get_queue_length() const override;
    virtual void clear() override;

private:
    std::deque<std::unique_ptr<QueueItem>> m_q;
};

}  // namespace tarp::sched
