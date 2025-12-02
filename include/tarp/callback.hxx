#pragma once

#include <functional>

namespace tarp {

template<typename signature>
class Callback;

// A lightweight fast callback container.
// std::function-like type erasure is _only_ used
// for capturing lambdas or functors.
// capture-less lambdas (which decay to C-function pointers),
// member and static functions are instead stored as simple
// pointers bound to a context (which is either some arbitrary
// user provided pointer, the object associated with the member
// function etc).
//
// This is mainly meant when the overhead os a 'signals-and-slots'
// system (e.g. sigc::system, boost_signals2) etc is undesirable
// or unnecessary (single observer, single-threaded, etc).
template<typename R, typename... Args>
class Callback<R(Args...)> {
    using FuncPtr = R (*)(void *, Args...);

    void *m_context = nullptr;
    FuncPtr m_function = nullptr;

    // used for heavier type-erased storage
    // (capturing lambdas, functors etc)
    struct base_box {
        virtual R operator()(Args...) const = 0;
        virtual ~base_box() = default;

        static R invoke_erased(void *ptr, Args... args) {
            return (*static_cast<base_box *>(ptr))(std::forward<Args>(args)...);
        }

        virtual base_box *clone() const = 0;
    };

    template<typename Callable>
    struct box : public base_box {
        box(Callable cb) : m_cb(std::move(cb)) {};

        virtual R operator()(Args... args) const override {
            return m_cb(args...);
        }

        base_box *clone() const override { return new box(m_cb); }

        Callable m_cb;

        ~box() {}
    };

public:
    // For capturing lambdas or std::function-like objects
    // (slower, type-erased).
    template<typename Callable>
    void bind(Callable &&callable) {
        reset();
        base_box *ptr = new box<Callable>(std::forward<Callable>(callable));
        m_context = ptr;
        // This allocates - only use for setup/init, not for hot path
        m_function = base_box::invoke_erased;
    }

    // Fast binding for static class functions or stateless lambdas.
    // Void * is passed back to the bound handler, which must know
    // what do with it (how to cast it etc).
    void bind(void *obj, R (*func)(void *, Args...)) {
        reset();
        m_context = static_cast<void *>(obj);
        m_function = func;
    }

    // Fast binding for member functions
    template<auto Method, typename T>
    void bind(T *obj) {
        reset();
        m_context = obj;
        m_function = [](void *ctx, Args... args) -> R {
            return std::invoke(Method, static_cast<T *>(ctx), args...);
        };
    }

    // Invoke the bound callable.
    // NOTE: this must only be called if a handler has been bound,
    // otherwise it'll cause a null-pointer dereference.
    R operator()(Args... args) const {
        // if (m_function) {
        return m_function(m_context, args...);
        //}
    }

    // True if a callable has been bound else false.
    explicit operator bool() const { return m_function != nullptr; }

    // Discard any state and unbind any handler.
    void reset() {
        if (m_function == base_box::invoke_erased) {
            // we allocated, so we must deallocate
            delete (static_cast<base_box *>(m_context));
        }

        m_context = nullptr;
        m_function = nullptr;
    }

    // Optional: support for cleanup (if using from_callable)
    ~Callback() { reset(); }

    Callback() noexcept = default;

    Callback(const Callback &other) noexcept { *this = other; }

    Callback &operator=(const Callback &other) noexcept {
        if (this != &other) {
            reset();
            m_function = other.m_function;
            if (other.m_function == base_box::invoke_erased) {
                m_context =
                  static_cast<const base_box *>(other.m_context)->clone();
            } else {
                m_context = other.m_context;
            }
        }
        return *this;
    }

    Callback(Callback &&other) noexcept
        : m_context(std::move(other.m_context))
        , m_function(std::move(other.m_function)) {
        other.m_context = nullptr;
        other.m_function = nullptr;
    }

    Callback &operator=(Callback &&other) noexcept {
        if (this != &other) {
            reset();
            m_context = other.m_context;
            m_function = other.m_function;
            other.m_context = nullptr;
            other.m_function = nullptr;
        }
        return *this;
    }
};


}  // namespace tarp
