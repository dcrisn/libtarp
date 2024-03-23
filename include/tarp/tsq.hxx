#pragma once

#include <deque>
#include <mutex>

#include <tarp/common.h>

namespace tarp {

/*
 * Thread-safe queue implementation.
 *
 * This is a simple wrapper around an STL queue type that wraps
 * its methods so that they are protected by mutex guards to ensure
 * thread safety.
 */
template<typename T>
class tsq final {
public:
    DISALLOW_COPY_AND_MOVE(tsq);
    tsq(void) = default;

    std::size_t size(void) const {
        LOCK(m_mtx);
        return m_queue.size();
    }

    bool empty(void) const {
        LOCK(m_mtx);
        return m_queue.empty();
    };

    void push_back(const T &i) {
        LOCK(m_mtx);
        m_queue.push_back(i);
    }

    void push_back(T &&i) {
        LOCK(m_mtx);
        m_queue.emplace_back(i);
    }

    template<template<typename> class SEQ>
    void push_back_many(const SEQ<T> &seq) {
        LOCK(m_mtx);
        for (const auto &i : seq) {
            m_queue.push_back(i);
        }
    }

    /*
     * NOTE: non-const back()/front() methods are not provided
     * as that is dangerous -- particularly so in a multi-threaded
     * context where it would defeat the purpose of serialization since
     * the data could be changed through the references and therefore
     * be subject to potential race conditions.
     */
    const T &back(void) const {
        LOCK(m_mtx);
        return m_queue.back();
    }

    T pop_back(void) {
        LOCK(m_mtx);
        T t = m_queue.back();
        m_queue.pop_back();
        return t;
    }

    /*
     * Pop n elements (all if n == 1) from the back of the queue
     * as if with pop_back and insert them one by one into seq
     * using push_back.
     */
    template<template<typename> class SEQ>
    void pop_back_many(SEQ<T> &seq, int n = -1) {
        LOCK(m_mtx);
        std::size_t num = (n == -1 || n > m_queue.size()) ? m_queue.size() : n;
        for (size_t i = 0; i < num; ++i) {
            seq.emplace_back(m_queue.back());
            m_queue.pop_back();
        }
    }

    void push_front(const T &i) {
        LOCK(m_mtx);
        m_queue.push_front(i);
    }

    void push_front(T &&i) {
        LOCK(m_mtx);
        m_queue.emplace_front(i);
    }

    template<template<typename> class SEQ>
    void push_front_many(const SEQ<T> &seq) {
        LOCK(m_mtx);
        for (const auto &i : seq) {
            m_queue.push_front(i);
        }
    }

    const T &front(void) const {
        LOCK(m_mtx);
        return m_queue.front();
    }

    T pop_front(void) {
        LOCK(m_mtx);
        T t = m_queue.front();
        m_queue.pop_front();
        return t;
    }

    /*
     * Pop n elements (all if n == 1) from the front of the queue
     * as if with pop_front and insert them one by one into seq
     * using push_back.
     */
    template<template<typename> class SEQ>
    void pop_front_many(SEQ<T> &seq, int n = -1) {
        LOCK(m_mtx);
        std::size_t num = (n == -1 || n > m_queue.size()) ? m_queue.size() : n;
        for (size_t i = 0; i < num; ++i) {
            seq.emplace_back(m_queue.front());
            m_queue.pop_front();
        }
    }

private:
    std::deque<T>      m_queue;
    mutable std::mutex m_mtx;
};

}  // namespace tarp
