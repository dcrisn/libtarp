#pragma once

#include <memory>
#include <tuple>

namespace tarp {

// Destroy the current owning unique_ptr and create a new one that holds
// the owned pointer, static cast to Derived.
template<typename Derived, typename Base, typename Del>
std::unique_ptr<Derived, Del>
static_uniqptr_cast(std::unique_ptr<Base, Del> &&p) {
    auto *d = static_cast<Derived *>(p.release());
    return std::unique_ptr<Derived, Del>(d, std::move(p.get_deleter()));
}

// Return true if the unique_ptr template parameter type has a custom deleter.
template<typename UNIQ_PTR_T>
constexpr bool has_custom_deleter() {
    using deleter = typename UNIQ_PTR_T::deleter_type;

    using T = typename UNIQ_PTR_T::element_type;
    using default_deleter = std::default_delete<T>;

    return !(std::is_same_v<default_deleter, deleter>);
}

// Destroy the current owning unique_ptr and try to create a new one that holds
// the owned pointer, dynamic cast to Derived. If the Dynamic cast succeeds,
// the first element returned is a boolean=true. The second element is the new
// unique pointer, successfully cast. The third element is null.
// Otherwise on failure the first element is a boolean=false, the second element
// is null, and the third element is the _original_ unique pointer, returned
// back to the called unchanged. NOTE: does not work with custom deleters.
template<typename Derived, typename Base>
std::tuple<bool, std::unique_ptr<Derived>, std::unique_ptr<Base>>
dynamic_uniqptr_cast(std::unique_ptr<Base> &&p) {
    static_assert(!has_custom_deleter<std::remove_reference_t<decltype(p)>>());

    Derived *result = dynamic_cast<Derived *>(p.get());
    if (result) {
        p.release();
        return {true, std::unique_ptr<Derived>(result), nullptr};
    }

    // If the dynamic cast fails, return back the original pointer in
    // pair.second -> otherwise the unique_ptr would be lost.
    return {false, nullptr, std::move(p)};
}


};  // namespace tarp
