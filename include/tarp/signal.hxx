#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include <tarp/cxxcommon.hxx>
#include <tarp/functools.hxx>
#include <tarp/type_traits.hxx>

namespace tarp {

/*
 * Interface that allows disconnection of a signal consumer from
 * a signal provider. The signal consumer *must* call disconnect()
 * in its destructor -- otherwise an exception is thrown.
 */
class signal_connection {
public:
    virtual void disconnect(void) = 0;
    virtual ~signal_connection() noexcept(false) = default;
};

namespace impl {
// non-specialized signal class
template<typename callback_signature,
         typename signal_output =
           tarp::type_traits::signature_decomp_return_t<callback_signature>,
         template<typename output, typename input> typename reducer =
           tarp::reduce::last,
         typename ts_policy = tarp::type_traits::thread_safe>
class signal;

// non-specialized monosignal class
template<typename callback_signature,
         typename signal_output =
           tarp::type_traits::signature_decomp_return_t<callback_signature>,
         typename ts_policy = tarp::type_traits::thread_safe>
class monosignal;

/*
 * Means to safely synchronize disconnection of a signal consumer
 * with a signal provider, even when the two run in different threads.
 */
template<typename ts_policy>
struct signal_token {
    bool valid {true};

    using mutex_t = typename tarp::type_traits::ts_types<ts_policy>::mutex_t;
    mutex_t mtx;
};

/*
 * The following class implements a signal -- and more generally, the
 * observer design pattern aka signals-and-slots.
 *
 * NOTE: this implementation is largely inspired by the libsigc++ signals
 * library. For anything more robust or complex, refer to that. This
 * implementation is primarily in order to minimize external dependencies
 * in libtarp and other projects that use it.
 *
 * tarp::signal and tarp::hook
 * ----------------------------
 * A signal is a notification of an event; a signal structure is provided that
 * signal (event) handlers can be connected to (registered with) and they will
 * be invoked (in order of registration) when the respective signal is emitted.
 * This typically implies signal handlers do not return any values since an
 * event/signal conceptually is a unidirectional data flow. Data is passed
 * from the signal provider to the signal consumer by way of notification
 * of an event.
 *
 * However, this need not be the case. Callbacks connected to a signal can
 * (must) return a value, according to the signature defined for the respective
 * signal. In this case data in fact flows both ways and especially backward:
 * the purpose of the signal is not to communicate information but to source
 * information from one or more callbacks. Since this is conceptuallly a
 * different use case, a tarp::hook alias is provided so that the user can
 * be explicit about the semantics:
 * - tarp::signal is for passing event information to non-returning callbacks
 * - tarp::hook is for getting data/information from one or more callbacks.
 *
 * The user, of course, is not obliged to observe this convention.
 *
 * disconnecting from a signal
 * -----------------------------
 * Disconnection from is more tricky than connection to a signal. This is
 * a well-known aspect of the observer pattern.
 * There are two cases:
 * - the signal consumer outlives the signal provider and no disconnection is
 *   needed. Use the connect_detached() method if you can guarantee this and
 *   do not need to ever disconnect from the signal for as long as that signal
 *   exists.
 *
 * - the signal provider may outlive the signal consumer. If this is the case,
 * or if the consumer wishes to be able to disconnect from the signal, then
 * the user must use the connect() method and hold on to the return value.
 * The return value *must* be kept for as long as the signal consumer object
 * exists. That is, the signal connection object returned by connect() must
 * not outlive the signal consumer.
 *
 * callbacks returning values
 * ----------------------------
 * - (1) if no callbacks are connected to a signal but that signal has a
 * signature that specifies a return value, then when the signal is emitted the
 * value returned to the signal provider is obtained by value-initializing a
 * variable of the return type.
 *
 * - (2) if a single callback is connected to the signal, then its return value
 * is returned.
 *
 * - (3) if multiple callbacks are connected to the signal, then by default the
 *   value returned by the last callback (i.e. the callback that was connected
 *   last) is returned.
 *
 * - (4) otherwise if multiple callbacks are connected to the signal and the
 *   signal was instantiated with template parameters to use a different
 *   reducer, then the return value of the signal is the value returned by the
 *   specified reducer.
 *
 * EXAMPLE of dealing with return values:
 *   -- default, get last or default value
 *   tarp::signal<int(void)> sig;
 *
 *   -- get value of *first* callback
 *   tarp::signal<int(void), int, tarp::reduce::first>();
 *
 *   -- get the sum of all values
 *   tarp::signal<int(void), int, tarp::reduce::sum>();
 *
 *   -- get a vector of all values
 *   tarp::signal<int(void), std::vector<int>, tarp::reduce::list>();
 * -------------------------------------
 *
 * The signature of callbacks connectable to the signal is specified as with
 * std::function: Return type(Param types) e.g. void(int, int), void(), int()
 * etc. The signature is obtained from the R and vargs template parameters:
 * R(vargs...).
 */

/*
 * Specification of template specialization parameters for
 * the signal class.
 *
 * Macro rationale: avoid copy-pasting and make it easy to change.
 *
 * ~~~ TEMPLATE PARAMETERS: ~~~
 *-----------------------------
 * --> R
 * The return type that callbacks must have.
 *
 * --> vargs
 * The parameter types that callbacks must take.
 *
 * --> signal_output
 * The output type of the signal returned by emit().
 * This is almost always the same as R.
 *
 * --> reducer
 * The reducer to use for producing a signal_output type.
 * emit() will instantiate a reduce template class and use
 * it to reduce the return vaues of all the callbacks (which
 * return R) to a single value (of type signal_output), which
 * is then returned by emit().
 *
 * NOTE: reducer is instantiated as follows:
 *   reducer<signal_output, R>
 * IOW the reducer must take R (the type returned by a callback)
 * and finally produce a value of signal_output type.
 */
// clang-format off
#define SIGNAL_TEMPLATE_SPEC                                  \
   typename R,                                                \
   typename... vargs,                                         \
   typename signal_output,                                    \
   template<typename output, typename input> class reducer, \
   typename ts_policy

#define SIGNAL_TEMPLATE_INSTANCE    \
   R(vargs...), signal_output, reducer, ts_policy
//clang-format on

/*
 * Partial template specialization to make it possible to use an
 * INVOKE-type signature for the first template parameter;
 * the non-specialized signal class is left undefined.
 */
template<SIGNAL_TEMPLATE_SPEC>
class signal<SIGNAL_TEMPLATE_INSTANCE> {
    // since there can be multiple observers, the parameters must
    // be copy-assignable and they must not be r-value references!
    // Otherwise e.g. given 3 observers and a unique_ptr param,
    // only the first observer will get the unique ptr and the
    // subsequent ones a moved-from unique_ptr i.e. nullptr.
    static_assert((!std::is_rvalue_reference_v<vargs> && ...));

    // each parameter must either be a (possibly const) l-value
    // reference, or otherwise copy-assignable.
    static_assert(((std::is_reference_v<vargs> or std::is_copy_assignable_v<std::remove_cv_t<
       vargs>>) && ...));
public:
    DISALLOW_COPY_AND_MOVE(signal);

    signal(void) = default;

    /* same as .emit(...) */
    signal_output operator()(vargs... params) { return emit(params...); }

    /* Invoke all connected callbacks in the order of their connection. */
    signal_output emit(vargs... params);

    /*
     *  Connect the given callback to the signal and return a connection object.
     * A signal consumer can disconnect from the signal by calling .disconnect()
     * on the connection object. NOTE this is called implicitly when a
     * connection object is destructed (calls to .disconnect() are idempotent).
     * NOTE: one single copy of the connection object must ever exist and the
     * sole copy must reside at all times with the signal consumer such that
     * the connection object does not outlive it. */
    std::unique_ptr<signal_connection>
    connect(std::function<R(vargs...)> callback);

    /*
     * Connect the given callback to the signal.
     * NOTE: this is a convenience function that asks the signal provider
     * to consider the signal consumer to always exist. The signal consumer
     * has no way to disconnect from the signal and no connection object
     * is therefore necessary. NOTE: the user must ensure the signal
     * consumer does in fact outlive the signal provider. */
    void connect_detached(std::function<R(vargs...)> callback);

    /* True if the count of callbacks connected to the signal is 0 */
    bool empty(void);

    /* Return the number of callbacks connected to the signal */
    std::size_t count(void);

private:
    using observer_callback_t = std::function<R(vargs...)>;
    using mutex_t = typename tarp::type_traits::ts_types<ts_policy>::mutex_t;
    using lock_t = typename tarp::type_traits::ts_types<ts_policy>::lock_t;
    using signal_token_t = signal_token<ts_policy>;

    struct connection : public signal_connection {
        connection(std::shared_ptr<signal_token<ts_policy>> tkn);
        virtual void disconnect(void) override;
        ~connection(void);

        std::shared_ptr<signal_token<ts_policy>> m_token;
    };

    struct observer {
        observer(bool detached,
                 std::shared_ptr<signal_token_t> &tkn,
                 std::function<R(vargs...)> f);
        std::shared_ptr<signal_token_t> check(void) const;

        std::function<R(vargs...)> notify;
        bool m_detached;
        std::weak_ptr<signal_token_t> m_link;
    };

    std::unique_ptr<signal_connection>
    register_observer(std::function<R(vargs...)> callback, bool detached);

    std::vector<observer> m_observers;
    mutex_t m_mtx;
};

/*
 * WARNING: connection to/disconnection from a signal is not allowed
 * from within a signal handler. Doing so will produce a deadlock.
 */
template<SIGNAL_TEMPLATE_SPEC>
signal_output signal<SIGNAL_TEMPLATE_INSTANCE>::emit(vargs... params) {
    /* If the return type of the signal is not void, then create the specified
     * reducer, which takes that type as input (R) and produces a final output
     * of the specified type (signal_output). Otherwise there would be a compile
     * error since void is not a valid type and cannot be passed to a function.
     * The tarp::reduce::void_reducer is used in this case. */
    using reducer_input_type = R;
    using reducer_output_type = signal_output;
    using reducer_type =
      typename std::conditional<std::is_void_v<reducer_input_type> or
                                  std::is_void_v<reducer_output_type>,
                                tarp::reduce::void_reducer<signal_output, R>,
                                reducer<signal_output, R>>::type;
    reducer_type r;

    lock_t lock{m_mtx};

    for (auto it = m_observers.begin(); it != m_observers.end();) {
        auto token = it->check();

        /* on every signal emission, weed out broken links
         * to observers that are no longer alive */
        if (!token) {
            it = m_observers.erase(it);
            continue;
        }

        /* NOTE: CRITICAL SECTION.
         * The validity check and callback invocation must be mutex-protected.
         * see signal_connection comments. */
        lock_t l(token->mtx);
        if (!token->valid) continue;

        // we use std::forward here to _cast_ the arguments passed to emit()
        // to the parameter types specified in the signal signature.
        if constexpr((std::is_void_v<R>)){
            std::invoke(it->notify, std::forward<vargs>(params)...);
        }
        else {
            r.process(std::invoke(it->notify, std::forward<vargs>(params)...));
        }
        ++it;
    }

    return r.get();
}

template<SIGNAL_TEMPLATE_SPEC>
std::size_t signal<SIGNAL_TEMPLATE_INSTANCE>::count() {
    lock_t l{m_mtx};
    return m_observers.size();
}

template<SIGNAL_TEMPLATE_SPEC>
bool signal<SIGNAL_TEMPLATE_INSTANCE>::empty() {
    return count() == 0;
}

template<SIGNAL_TEMPLATE_SPEC>
std::unique_ptr<signal_connection>
signal<SIGNAL_TEMPLATE_INSTANCE>::connect(
  std::function<R(vargs...)> callback) {
    return register_observer(callback, false);
}

template<SIGNAL_TEMPLATE_SPEC>
void signal<SIGNAL_TEMPLATE_INSTANCE>::connect_detached(
  std::function<R(vargs...)> callback) {
    auto conn = register_observer(callback, true);

    // when connecting in detached mode, 'disconnect()' does nothing.
    // However, the connection object requires us to invoke this method
    // otherwise it will throw an exception in the dtor. So we're doing
    // it here before the object is destructed.
    conn->disconnect();
}

template<SIGNAL_TEMPLATE_SPEC>
std::unique_ptr<signal_connection>
signal<SIGNAL_TEMPLATE_INSTANCE>::register_observer(observer_callback_t f,
                                                          bool detached) {
    auto token = std::make_shared<signal_token_t>();

    lock_t l{m_mtx};

    m_observers.emplace_back(observer({detached, token, f}));
    return std::make_unique<signal<SIGNAL_TEMPLATE_INSTANCE>::connection>(
      token);
}

template<SIGNAL_TEMPLATE_SPEC>
signal<SIGNAL_TEMPLATE_INSTANCE>::connection::connection(
  std::shared_ptr<signal_token_t> tkn)
    : m_token(tkn) {
}

/*
 * Reset the shared pointer to break the link with the
 * signal provider. The signal provider will lazily
 * clean up its state when it gets around to noticing
 * that its link to the the observer (signal consumer)
 * is broken.
 *
 * NOTE: the mutex is needed to avoid race conditions
 * on disconnection when the signal provider and signal consumer
 * run in parallel in separate threads. Specifically, if the
 * the signal provider somehow manages to get a shared pointer
 * to the token before the token has been destructed, then there
 * is a race condition since the signal provider uses the token
 * as an indication that the signal consumer still exists. However,
 * the signal consumer can be destructed between the point where
 * the signal provider gets the shared pointer to the token and
 * the point where the signal provider invokes a callback on
 * the signal consumer:
 * 1. Inside signal consumer destructor (token not yet destructed)
 * 2. signal provider gets shared pointer to token
 * 3. signal consumer destructed
 * 4. signal provider calls callback (BAD: dangling references etc)
 *
 * Another, mutex-protected, flag is needed to ensure serialization
 * and avoid this race condition:
 * 1. Inside signal consumer destructor (token not yet destructed)
 * 2. signal provider gets shared pointer to token
 * 3. signal consumer locks the mutex and invalidates the token
 * 4. signal consumer destructed
 * 5. signal provider locks the mutex and checks the token
 * 6. signal provider sees token is invalid, does not invoke callback
 *
 * OR:
 * 3. signal provider locks the mutex and checks the token
 * 4. signal provider sees token is valid
 * 5. signal provider invokes callback
 * 6. callback is invoked on valid, not yet destructed, signal consumer
 * 6. signal consumer locks the mutex and invalidates the token
 * 7. signal consumer destructed
 *
 * NOTE: it is fundamental (see point 6. above: 'callback is invoked on valid,
 * not-yet destructed signal consumer') that the signal consumer explicitly
 * call .disconnect() in its destructor. This is because this way it can be
 * guaranteed that if a callback is invoked at this point, the signal consumer
 * object is in a valid state as any member objects will not yet have been
 * destructed. Otherwise, if .disconnect() is not called and the token is simply
 * destructed along with the other member variables, then the callback may be
 * invoked on the signal consumer at a point partway through its destruction. At
 * that point dangling references may be used, leading to a crash, depending on
 * the order in which member variables in the signal consumer get destructed.
 *
 * To avoid this scenario and to ensure the signal consumer properly calls
 * .disconnect() in its destructor, an exception is thrown in the destructor
 * of the signal_connection object if this is found not to have been done.
 */
template<SIGNAL_TEMPLATE_SPEC>
void signal<SIGNAL_TEMPLATE_INSTANCE>::connection::disconnect(void) {
    /* make copy in case m_token is reset by some other thread */
    auto token = m_token;
    if (!token){
        return;
    }

    {
        lock_t l{token->mtx};
        token->valid = false;
    }

    m_token.reset();
}

template<SIGNAL_TEMPLATE_SPEC>
signal<SIGNAL_TEMPLATE_INSTANCE>::connection::~connection(void){
    // make copy in case m_token gets reset by some other thread.
    auto token = m_token;
    if (!token) return;

    lock_t l{token->mtx};
    if (token->valid) {
        throw std::logic_error(
          "signal_connection destructed without being disconnected");
    }
}

template<SIGNAL_TEMPLATE_SPEC>
signal<SIGNAL_TEMPLATE_INSTANCE>::observer::observer(
  bool detached,
  std::shared_ptr<signal_token_t> &ref,
  std::function<R(vargs...)> f)
    : notify(f), m_detached(detached), m_link(ref) {
}

template<SIGNAL_TEMPLATE_SPEC>
std::shared_ptr<typename signal<SIGNAL_TEMPLATE_INSTANCE>::signal_token_t>
signal<SIGNAL_TEMPLATE_INSTANCE>::observer::check(void) const {
    // a detached observer is deemed perpetually alive. User
    // must ensure that is the case (i.e. that the signal consumer
    // outlives the signal provider)
    if (m_detached) return std::make_shared<signal_token_t>();

    return m_link.lock();
}

#undef SIGNAL_TEMPLATE_SPEC
#undef SIGNAL_TEMPLATE_INSTANCE

////////////////////////////////////////////////////////

#define SIGNAL_TEMPLATE_SPEC                                  \
   typename R,                                                \
   typename... vargs,                                         \
   typename signal_output,                                    \
   typename ts_policy

#define SIGNAL_TEMPLATE_INSTANCE    \
   R(vargs...), signal_output, ts_policy
//clang-format on

/*
 * Partial template specialization to make it possible to use an
 * INVOKE-type signature for the first template parameter;
 * the non-specialized signal class is left undefined.
 */
template<SIGNAL_TEMPLATE_SPEC>
class monosignal<SIGNAL_TEMPLATE_INSTANCE> {
public:
    DISALLOW_COPY_AND_MOVE(monosignal);

    monosignal(void) = default;

    /* same as .emit(...) */
    signal_output operator()(vargs... params) { return emit(params...); }

    /* Invoke all connected callbacks in the order of their connection. */
    signal_output emit(vargs... params);

    /*
     * Connect the given callback to the signal and return a connection object.
     * A signal consumer can disconnect from the signal by calling .disconnect()
     * on the connection object.
     * NOTE: if the connection object is destructed without having been
     * explicitly disconnected, an exception is thrown.
     * NOTE: one single copy of the connection object must ever exist and the
     * sole copy must reside at all times with the signal consumer such that
     * the connection object does not outlive it.
     *
     * NOTE: if more than one connect() call is made in a row (i.e. without
     * an intervening disconnect() call), an exception is thrown since the
     * monosignal only accepts one signal consumer. */
    std::unique_ptr<signal_connection>
    connect(std::function<R(vargs...)> callback);

    /*
     * Connect the given callback to the signal.
     * NOTE: this is a convenience function that asks the signal provider
     * to consider the signal consumer to always exist. The signal consumer
     * has no way to disconnect from the signal and no connection object
     * is therefore necessary. NOTE: the user must ensure the signal
     * consumer does in fact outlive the signal provider. */
    void connect_detached(std::function<R(vargs...)> callback);

    /* True if the signal currently has a handler connected. */
    bool connected(void);

private:
    using observer_callback_t = std::function<R(vargs...)>;
    using mutex_t = typename tarp::type_traits::ts_types<ts_policy>::mutex_t;
    using lock_t = typename tarp::type_traits::ts_types<ts_policy>::lock_t;
    using signal_token_t = signal_token<ts_policy>;

    struct connection : public signal_connection {
        connection(std::shared_ptr<signal_token<ts_policy>> tkn);
        virtual void disconnect(void) override;
        ~connection(void);

        std::shared_ptr<signal_token<ts_policy>> m_token;
    };

    struct observer {
        observer(bool detached,
                 std::shared_ptr<signal_token_t> &tkn,
                 std::function<R(vargs...)> f);
        std::shared_ptr<signal_token_t> check(void) const;

        std::function<R(vargs...)> notify;
        bool m_detached;
        std::weak_ptr<signal_token_t> m_link;
    };

    static inline constexpr auto size = sizeof(observer);

    std::unique_ptr<signal_connection>
    register_observer(std::function<R(vargs...)> callback, bool detached);

    std::unique_ptr<observer> m_observer;
    mutex_t m_mtx;
};

/*
 * WARNING: connection to/disconnection from a signal is not allowed
 * from within a signal handler. Doing so will produce a deadlock.
 */
template<SIGNAL_TEMPLATE_SPEC>
signal_output monosignal<SIGNAL_TEMPLATE_INSTANCE>::emit(vargs... params) {
    lock_t lock{m_mtx};

    // we use a reducer here to deal with the case when the return result
    // is void vs an actual value.
    using reducer_type =
      typename std::conditional<std::is_void_v<R>,
                                tarp::reduce::void_reducer<signal_output, R>,
                                tarp::reduce::last<signal_output, R>>::type;
    reducer_type r;

    if (m_observer == nullptr){
        return r.get();
    }

    auto token = m_observer->check();

    // remove if broken link (observer no longer alive)
    if (!token) {
        m_observer.reset();
        return r.get();
    }

    /* NOTE: CRITICAL SECTION.
    * The validity check and callback invocation must be mutex-protected.
    * see signal_connection comments. */
    lock_t l(token->mtx);
    if (!token->valid){
        m_observer.reset();
        return r.get();
    }

    // we use std::forward here to _cast_ the arguments passed to emit()
    // to the parameter types specified in the signal signature.
    if constexpr (!std::is_void_v<R>){
       return std::invoke(m_observer->notify, std::forward<vargs>(params)...);
    }

    std::invoke(m_observer->notify, std::forward<vargs>(params)...);
}

template<SIGNAL_TEMPLATE_SPEC>
bool monosignal<SIGNAL_TEMPLATE_INSTANCE>::connected() {
    lock_t l{m_mtx};

    if (!m_observer){
        return false;
    }

    auto tkn = m_observer->check();
    
    // remove if broken link (observer no longer alive)
    if (!tkn) {
        m_observer.reset();
        return false;
    }

    /* NOTE: CRITICAL SECTION.
     * The validity check and callback invocation must be mutex-protected.
     * see signal_connection comments. */
    lock_t lock(tkn->mtx);
    if (!tkn->valid){
        m_observer.reset();
        return false;
    }

    return true;
}

template<SIGNAL_TEMPLATE_SPEC>
std::unique_ptr<signal_connection>
monosignal<SIGNAL_TEMPLATE_INSTANCE>::connect(
  std::function<R(vargs...)> callback) {
    return register_observer(callback, false);
}

template<SIGNAL_TEMPLATE_SPEC>
void monosignal<SIGNAL_TEMPLATE_INSTANCE>::connect_detached(
  std::function<R(vargs...)> callback) {
    auto conn = register_observer(callback, true);

    // when connecting in detached mode, 'disconnect()' does nothing.
    // However, the connection object requires us to invoke this method
    // otherwise it will throw an exception in the dtor. So we're doing
    // it here before the object is destructed.
    conn->disconnect();
}

template<SIGNAL_TEMPLATE_SPEC>
std::unique_ptr<signal_connection>
monosignal<SIGNAL_TEMPLATE_INSTANCE>::register_observer(observer_callback_t f,
                                                          bool detached) {
    auto token = std::make_shared<signal_token_t>();

    lock_t l{m_mtx};

    // if observer, see if it is still alive
    if (m_observer != nullptr){
        auto token_tmp = m_observer->check();

        // remove if broken link (observer no longer alive)
        if (!token_tmp) {
            m_observer.reset();
        } else{
            /* NOTE: CRITICAL SECTION.
            * The validity check and callback invocation must be mutex-protected.
            * see signal_connection comments. */
            lock_t lock(token->mtx);
            if (!token_tmp->valid){
                m_observer.reset();
            }
        }

        // if still alive and kicking, then illegal.
        if  (m_observer != nullptr){
            throw std::logic_error("Illegal attempt at more than 1 connection to monosignal");
        }
    }

    m_observer = std::make_unique<observer>(detached, token, f);
    return std::make_unique<monosignal<SIGNAL_TEMPLATE_INSTANCE>::connection>(
      token);
}

template<SIGNAL_TEMPLATE_SPEC>
monosignal<SIGNAL_TEMPLATE_INSTANCE>::connection::connection(
  std::shared_ptr<signal_token_t> tkn)
    : m_token(tkn) {
}

/*
 * Reset the shared pointer to break the link with the
 * signal provider. The signal provider will lazily
 * clean up its state when it gets around to noticing
 * that its link to the the observer (signal consumer)
 * is broken.
 *
 * NOTE: the mutex is needed to avoid race conditions
 * on disconnection when the signal provider and signal consumer
 * run in parallel in separate threads. Specifically, if the
 * the signal provider somehow manages to get a shared pointer
 * to the token before the token has been destructed, then there
 * is a race condition since the signal provider uses the token
 * as an indication that the signal consumer still exists. However,
 * the signal consumer can be destructed between the point where
 * the signal provider gets the shared pointer to the token and
 * the point where the signal provider invokes a callback on
 * the signal consumer:
 * 1. Inside signal consumer destructor (token not yet destructed)
 * 2. signal provider gets shared pointer to token
 * 3. signal consumer destructed
 * 4. signal provider calls callback (BAD: dangling references etc)
 *
 * Another, mutex-protected, flag is needed to ensure serialization
 * and avoid this race condition:
 * 1. Inside signal consumer destructor (token not yet destructed)
 * 2. signal provider gets shared pointer to token
 * 3. signal consumer locks the mutex and invalidates the token
 * 4. signal consumer destructed
 * 5. signal provider locks the mutex and checks the token
 * 6. signal provider sees token is invalid, does not invoke callback
 *
 * OR:
 * 3. signal provider locks the mutex and checks the token
 * 4. signal provider sees token is valid
 * 5. signal provider invokes callback
 * 6. callback is invoked on valid, not yet destructed, signal consumer
 * 6. signal consumer locks the mutex and invalidates the token
 * 7. signal consumer destructed
 *
 * NOTE: it is fundamental (see point 6. above: 'callback is invoked on valid,
 * not-yet destructed signal consumer') that the signal consumer explicitly
 * call .disconnect() in its destructor. This is because this way it can be
 * guaranteed that if a callback is invoked at this point, the signal consumer
 * object is in a valid state as any member objects will not yet have been
 * destructed. Otherwise, if .disconnect() is not called and the token is simply
 * destructed along with the other member variables, then the callback may be
 * invoked on the signal consumer at a point partway through its destruction. At
 * that point dangling references may be used, leading to a crash, depending on
 * the order in which member variables in the signal consumer get destructed.
 *
 * To avoid this scenario and to ensure the signal consumer properly calls
 * .disconnect() in its destructor, an exception is thrown in the destructor
 * of the signal_connection object if this is found not to have been done.
 */
template<SIGNAL_TEMPLATE_SPEC>
void monosignal<SIGNAL_TEMPLATE_INSTANCE>::connection::disconnect(void) {
    /* make copy in case m_token is reset by some other thread */
    auto token = m_token;
    if (!token){
        return;
    }

    {
        lock_t l{token->mtx};
        token->valid = false;
    }

    m_token.reset();
}

template<SIGNAL_TEMPLATE_SPEC>
monosignal<SIGNAL_TEMPLATE_INSTANCE>::connection::~connection(void){
    // make copy in case m_token gets reset by some other thread.
    auto token = m_token;
    if (!token) return;

    lock_t l{token->mtx};
    if (token->valid) {
        throw std::logic_error(
          "signal_connection destructed without being disconnected");
    }
}

template<SIGNAL_TEMPLATE_SPEC>
monosignal<SIGNAL_TEMPLATE_INSTANCE>::observer::observer(
  bool detached,
  std::shared_ptr<signal_token_t> &ref,
  std::function<R(vargs...)> f)
    : notify(f), m_detached(detached), m_link(ref) {
}

template<SIGNAL_TEMPLATE_SPEC>
std::shared_ptr<typename monosignal<SIGNAL_TEMPLATE_INSTANCE>::signal_token_t>
monosignal<SIGNAL_TEMPLATE_INSTANCE>::observer::check(void) const {
    // a detached observer is deemed perpetually alive. User
    // must ensure that this is the case (i.e. that the signal consumer
    // outlives the signal provider)
    if (m_detached) return std::make_shared<signal_token_t>();

    return m_link.lock();
}

}

namespace ts{
template<typename callback_signature,
         typename signal_output =
           tarp::type_traits::signature_decomp_return_t<callback_signature>,
         template<typename output, typename input> typename reducer =
           tarp::reduce::last>
using signal = impl::signal<callback_signature, signal_output, reducer,
tarp::type_traits::thread_safe>;

template<typename callback_signature,
         typename signal_output =
           tarp::type_traits::signature_decomp_return_t<callback_signature>,
         template<typename output, typename input> typename reducer =
           tarp::reduce::last>
using hook = signal<callback_signature, signal_output, reducer>;

template<typename callback_signature,
         typename signal_output =
           tarp::type_traits::signature_decomp_return_t<callback_signature>>
using monosignal = impl::monosignal<callback_signature, signal_output,
                                     tarp::type_traits::thread_safe>;
}

namespace tu{
template<typename callback_signature,
         typename signal_output =
           tarp::type_traits::signature_decomp_return_t<callback_signature>,
         template<typename output, typename input> typename reducer =
           tarp::reduce::last>
using signal = impl::signal<callback_signature, signal_output, reducer,
tarp::type_traits::thread_unsafe>;

template<typename callback_signature,
         typename signal_output =
           tarp::type_traits::signature_decomp_return_t<callback_signature>,
         template<typename output, typename input> typename reducer =
           tarp::reduce::last>
using hook = signal<callback_signature, signal_output, reducer>;

template<typename callback_signature,
         typename signal_output =
           tarp::type_traits::signature_decomp_return_t<callback_signature>>
using monosignal = impl::monosignal<callback_signature, signal_output,
                                 tarp::type_traits::thread_unsafe>;

};


}  // namespace tarp
