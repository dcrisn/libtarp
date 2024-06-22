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
    std::optional<std::uint8_t> get_priority() const;
    void set_priority(std::uint8_t);
    virtual std::string get_pretty_name() const = 0;

private:
    std::optional<std::uint8_t> m_priority;
};

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
