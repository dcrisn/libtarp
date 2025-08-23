#pragma once

#include <exception>
#include <iostream>
#include <string>
#include <type_traits>

namespace tarp {

// A simple RAII wrapper that invoked a function in its destructor.
// Useful for cleanup handlers that would otherwise typically
// rely on goto, as often seen in C code.
template<class F>
class scope_guard final {
    F m_func;
    bool m_enabled = true;

public:
    explicit scope_guard(F f) noexcept(std::is_nothrow_move_constructible_v<F>)
        : m_func(std::move(f)) {}

    ~scope_guard() noexcept {
        if (m_enabled) {
            try {
                m_func();
            } catch (const std::exception &e) {
                using namespace std::string_literals;
                std::cerr << "exception caught in scope_guard function: "s +
                               e.what();
                throw;
            }
        }
    }

    // do not call the function in the destructor.
    void disable() noexcept { m_enabled = false; }

    scope_guard(scope_guard &&o) noexcept(
      std::is_nothrow_move_constructible_v<F>)
        : m_func(std::move(o.f_)), m_enabled(o.m_enabled) {
        o.disable();
    }

    scope_guard(const scope_guard &) = delete;
    scope_guard &operator=(const scope_guard &) = delete;
};

// CTAD guide
template<class F>
scope_guard(F) -> scope_guard<F>;

}  // namespace tarp
