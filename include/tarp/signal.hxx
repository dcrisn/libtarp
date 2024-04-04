#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>

#include <tarp/cxxcommon.hxx>
#include <tarp/functools.hxx>

namespace tarp {

template<typename callback_signature,
         typename signal_output = tarp::func_return_type<callback_signature>,
         template<typename output, typename input> typename reducer =
           tarp::reduce::last>
class signal;

template<typename callback_signature,
         typename signal_output = tarp::func_return_type<callback_signature>,
         template<typename output, typename input> typename reducer =
           tarp::reduce::last>
using hook = signal<callback_signature, signal_output, reducer>;

/*
 * Means to safely synchronize disconnection of a signal consumer
 * with a signal provider, even when the two run in different threads.
 */
struct signal_token {
    bool valid {true};
    std::mutex mtx;
};

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

/*
 * The following class implements a signal -- and more generally, the
 * publish-subscribe aka observer design pattern.
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
 * Disconnection from is more tricky than connection to a signal.
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
   template<typename output, typename input> class reducer

#define SIGNAL_TEMPLATE_INSTANCE    \
   R(vargs...), signal_output, reducer
//clang-format on

/*
 * Partial template specialization to make it possible to use an
 * INVOKE-type signature for the first template parameter;
 * the non-specialized signal class is left undefined.
 */
template<SIGNAL_TEMPLATE_SPEC>
class signal<SIGNAL_TEMPLATE_INSTANCE> {
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
    std::unique_ptr<tarp::signal_connection>
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

    struct connection : public signal_connection {
        connection(std::shared_ptr<tarp::signal_token> &tkn);
        virtual void disconnect(void) override;
        ~connection(void);

        std::shared_ptr<tarp::signal_token> m_token;
    };

    struct observer {
        observer(bool detached,
                 std::shared_ptr<tarp::signal_token> &tkn,
                 std::function<R(vargs...)> f);
        std::shared_ptr<tarp::signal_token> check(void) const;

        std::function<R(vargs...)> notify;
        bool m_detached;
        std::weak_ptr<tarp::signal_token> m_link;
    };

    std::unique_ptr<tarp::signal_connection>
    register_observer(std::function<R(vargs...)> callback, bool detached);

    std::vector<observer> m_observers;
    std::mutex m_mtx;
};

template<SIGNAL_TEMPLATE_SPEC>
signal_output tarp::signal<SIGNAL_TEMPLATE_INSTANCE>::emit(vargs... params) {
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

    LOCK(m_mtx);

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
        std::unique_lock l(token->mtx);
        if (!token->valid) continue;

        if constexpr (std::is_void_v<R>) std::invoke(it->notify, params...);
        else r.process(std::invoke(it->notify, params...));

        ++it;
    }

    return r.get();
}

template<SIGNAL_TEMPLATE_SPEC>
std::size_t tarp::signal<SIGNAL_TEMPLATE_INSTANCE>::count() {
    LOCK(m_mtx);
    return m_observers.size();
}

template<SIGNAL_TEMPLATE_SPEC>
bool tarp::signal<SIGNAL_TEMPLATE_INSTANCE>::empty() {
    return count() == 0;
}

template<SIGNAL_TEMPLATE_SPEC>
std::unique_ptr<tarp::signal_connection>
tarp::signal<SIGNAL_TEMPLATE_INSTANCE>::connect(
  std::function<R(vargs...)> callback) {
    return register_observer(callback, false);
}

template<SIGNAL_TEMPLATE_SPEC>
void tarp::signal<SIGNAL_TEMPLATE_INSTANCE>::connect_detached(
  std::function<R(vargs...)> callback) {
    auto conn = register_observer(callback, true);
    conn->disconnect();
}

template<SIGNAL_TEMPLATE_SPEC>
std::unique_ptr<tarp::signal_connection>
tarp::signal<SIGNAL_TEMPLATE_INSTANCE>::register_observer(observer_callback_t f,
                                                          bool detached) {
    auto token = std::make_shared<tarp::signal_token>();

    LOCK(m_mtx);

    m_observers.emplace_back(observer({detached, token, f}));
    return std::make_unique<tarp::signal<SIGNAL_TEMPLATE_INSTANCE>::connection>(
      token);
}

template<SIGNAL_TEMPLATE_SPEC>
tarp::signal<SIGNAL_TEMPLATE_INSTANCE>::connection::connection(
  std::shared_ptr<tarp::signal_token> &tkn)
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
void tarp::signal<SIGNAL_TEMPLATE_INSTANCE>::connection::disconnect(void) {
    {
        LOCK(m_token->mtx);
        m_token->valid = false;
    }
    m_token.reset();
}

template<SIGNAL_TEMPLATE_SPEC>
tarp::signal<SIGNAL_TEMPLATE_INSTANCE>::connection::~connection(void){
    if (!m_token) return;

    LOCK(m_token->mtx);
    if (m_token->valid) {
        throw std::logic_error(
          "signal_connection destructed without being disconnected");
    }
}

template<SIGNAL_TEMPLATE_SPEC>
tarp::signal<SIGNAL_TEMPLATE_INSTANCE>::observer::observer(
  bool detached,
  std::shared_ptr<tarp::signal_token> &ref,
  std::function<R(vargs...)> f)
    : notify(f), m_detached(detached), m_link(ref) {
}

template<SIGNAL_TEMPLATE_SPEC>
std::shared_ptr<tarp::signal_token>
tarp::signal<SIGNAL_TEMPLATE_INSTANCE>::observer::check(void) const {
    // a detached observer is deemed perpetually alive. User
    // must ensure that is the case (i.e. that the signal consumer
    // outlives the signal provider)
    if (m_detached) return std::make_shared<tarp::signal_token>();

    return m_link.lock();
}

#undef SIGNAL_TEMPLATE_SPEC
#undef SIGNAL_TEMPLATE_INSTANCE

}  // namespace tarp
