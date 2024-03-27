#pragma once

#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

#include <tarp/cxxcommon.hxx>

namespace tarp {

class signal_connection;

class abstract_signal_interface {
public:
    static std::unique_ptr<tarp::signal_connection>
    make_signal_connection(tarp::abstract_signal_interface &signal,
                           unsigned id);

    virtual void disconnect(unsigned id) = 0;
};

class signal_connection {
public:
    friend std::unique_ptr<tarp::signal_connection>
    abstract_signal_interface::make_signal_connection(
      tarp::abstract_signal_interface &signal,
      unsigned id);

    signal_connection(const signal_connection &other) = delete;
    signal_connection &operator=(const signal_connection &rhs) = delete;
    signal_connection(signal_connection &&other) = delete;

    // allow only move assignment operator so the signal class can return
    // a signal_connection to the user
    signal_connection &operator=(signal_connection &&rhs) {
        m_disconnected = rhs.m_disconnected;
        m_signal = rhs.m_signal;
        m_id = rhs.m_id;

        rhs.m_disconnected = true;
        return *this;
    }

    /* the user could do this directly, but this allows to change the
     * implementation more easily */
    void disconnect(void) {
        if (m_disconnected) return;
        m_signal.disconnect(m_id);
        m_disconnected = true;
    }

private:
    signal_connection(tarp::abstract_signal_interface &signal, unsigned id)
        : m_signal(signal), m_id(id), m_disconnected(false) {}

    tarp::abstract_signal_interface &m_signal;
    unsigned m_id;
    bool m_disconnected;
};

/*
 * The following class implements a signal.
 *
 * A signal is notification of an event; signal (event) handlers can be
 * connected to it and they will be invoked (in order of registration) when the
 * respective signal is emitted.
 *
 * A handler can be disconnected either by returning 'false' when called to
 * handle the event (NOTE: handlers always return a boolean) or by remembering
 * the connection object returned by .connect() and using that to request a
 * disconnect.
 *
 * NOTE the .emit function (and the () operator overload) are not function
 * templates and therefore they don't use forwarding references. In fact, the
 * exact types indicated by the signature of the signal will need to be
 * observed by any handler that wants to be registered for that signal.
 * E.g. signal<int,string&&> indicates the second argument is an r-value
 * std::string reference.
 *
 * NOTE
 * This class is meant as a basic convenience helper for use within libtarp
 * and such. For anything more proper and robust, look to libsigc++.
 */
template<typename>
class signal;

template<typename R, typename... vargs>
class signal<R(vargs...)> : public abstract_signal_interface {
public:
    DISALLOW_COPY_AND_MOVE(signal);
    signal(void) = default;

    /* same as .emit(...) */
    R operator()(vargs... params) { return emit(params...); }

    /*
     * NOTE: user much check that the signal has at least one callback
     * connected to it, else the return value will be garbage. */
    R emit(vargs... params) {
        for (auto it = m_callback_slots.begin(); it != m_callback_slots.end();
             ++it) {
            /* save and return the result of the last callback */
            if (it + 1 == m_callback_slots.end())
                return std::invoke(it->f, params...);
            else std::invoke(it->f, params...);
        }
    }

    std::unique_ptr<tarp::signal_connection>
    connect(std::function<R(vargs...)> callback) {
        if (callback == nullptr) {
            throw std::invalid_argument("Invalid callback specified (null)");
        }

        // note this wraps around on 2^16, but you should never use this with
        // that many callbacks!!
        m_callback_slots.emplace_back(wrapper {m_id++, std::move(callback)});
        return abstract_signal_interface::make_signal_connection(*this,
                                                                 m_id - 1);
    }

    virtual void disconnect(unsigned id) override {
        for (auto it = m_callback_slots.begin(); it != m_callback_slots.end();
             it++) {
            if (it->id == id) {
                m_callback_slots.erase(it);
                return;
            }
        }
    }

    bool empty(void) { return m_callback_slots.empty(); }

private:
    unsigned m_id;

    struct wrapper {
        std::size_t id;
        std::function<R(vargs...)> f;
    };
    std::vector<struct wrapper> m_callback_slots;
};

#if 0
class signal : public abstract_signal_interface{
public:
    signal(const signal &other) = delete;
    signal &operator=(const signal &rhs) = delete;
    signal(signal &&other) = delete;
    signal &operator=(signal &&rhs) = delete;
    signal(void) = default;

    /* same as .emit(...) */
    void operator()(vargs... param){
        emit(std::forward<vargs>(param)...);
    }

    //template <typename ...params>
    void emit(vargs ...param){
    //void emit(params&&... args){
        for (auto it = m_callback_slots.begin(); it != m_callback_slots.end();){
            bool persist = std::invoke(it->f, param...);
            if (persist) it++;
            else it = m_callback_slots.erase(it);
        }
    }

    signal_connection connect(std::function<bool(vargs...)> callback){
        assert(callback != nullptr);
        // note this wraps around on 2^16, but you should never use this with
        // that many callbacks!!
        m_callback_slots.emplace_back(wrapper{m_id++, std::move(callback)});
    }

    virtual void disconnect(unsigned id) override{
        for (auto it = m_callback_slots.begin(); it != m_callback_slots.end(); it++){
            if (it->id == id){
                m_callback_slots.erase(it);
                return;
            }
        }
    }

private:
    unsigned m_id;

    struct wrapper{std::size_t id; std::function<bool(vargs...)> f;};
    std::vector<struct wrapper> m_callback_slots;
};
#endif

} /* namespace tarp */
