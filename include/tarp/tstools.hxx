#pragma once

#include <tarp/cxxcommon.hxx>
#include <tarp/tsq.hxx>

namespace tarp {

/*
 * Wrap an object into a struct protected by a mutex.
 * Provide a RAII interface that guards and proxies all
 * accesses to members of the wrapped object.
 */
template<typename T>
class lock_guarded_struct {
    static_assert(std::is_default_constructible<T>::value,
                  "lock_guarded expects a default-constructible object");

private:
    std::mutex m_mtx;
    T m_object;

public:
    class guarded_proxy {
    public:
        guarded_proxy(lock_guarded_struct &lgs)
            : m_lockwrap(lgs), m_lock(lgs.m_mtx) {}

        T &operator*() { return m_lockwrap.m_object; }

        T *operator->() { return &m_lockwrap.m_object; }

    private:
        lock_guarded_struct &m_lockwrap;
        std::unique_lock<std::mutex> m_lock;
    };

    /* Obviously, only one 'guarded proxy' can exist at one time.
     * If you try to create a second one in the same thread where the first
     * one has not gone out of scope, you will deadlock.
     *
     * NOTE: since guard can be used to access the actual object, a reference
     * to some internal member of the object can easily be done. DO NOT DO THIS.
     * That would be subverting the whole mechanism and throw safety out the
     * window. Use the object the way it is intended i.e. only set or get by
     * value, through the guard.
     */
    guarded_proxy guard() { return guarded_proxy(*this); }
};

}  // namespace tarp
