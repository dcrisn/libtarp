#include <tarp/sched.hxx>

namespace tarp::sched {
using namespace std;

std::optional<uint8_t> QueueItem::get_priority() const {
    return m_priority;
}

void QueueItem::set_priority(uint8_t prio) {
    m_priority = prio;
}

Scheduler::Scheduler() {
}

Scheduler::~Scheduler() {
}

SchedulerFifo::SchedulerFifo() : Scheduler() {
}

void SchedulerFifo::enqueue(std::unique_ptr<QueueItem> item) {
    m_q.push_back(std::move(item));
}

std::unique_ptr<QueueItem> SchedulerFifo::dequeue() {
    if (m_q.empty()) {
        return nullptr;
    }

    auto head = std::move(m_q.front());
    m_q.pop_front();
    return head;
}

std::size_t SchedulerFifo::get_queue_length() const {
    return m_q.size();
}

void SchedulerFifo::clear() {
    m_q.clear();
}

}  // namespace tarp::sched
