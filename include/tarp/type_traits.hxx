#pragma once

#include <type_traits>

namespace tarp {
namespace type_traits {

//

// --------------------------------------------------------------------
// Signature composition.
// Template that lets you compose a function signature (R(PARAMS))
// from discrete elements. This is useful for example when the types of the
// parameters and/or return value are generated dynamically.
// Another special use case is when specifying the signature of a function
// that takes void as an argument. 'void' is not an actual type that is
// valid as a function argument ( e.g. int(void) ) and will produce a
// compilation error. The template is specialized so that <R, void>
// produces R() instead of R(void) to placate the compiler.
// --------------------------------------------------------------------

// Signature of a function that return R and takes an arbitrary number of
// arguments of arbitrary types.
template<typename R, typename... args>
struct signature_comp {
    using signature = R(args...);
};

// Signature of a function that takes a single argument T and returns R.
template<typename R, typename T>
struct signature_comp<R, T> {
    using return_type = R;
    using signature = R(T);
};

// Signature of a function that takes void (i.e. no arguments) amd returns R.
template<typename R>
struct signature_comp<R, void> {
    using signature = R();
};

template<typename R, typename... args>
using signature_comp_t = typename signature_comp<R, args...>::signature;

//

// --------------------------------------------------------------------
// Signature decomposition.
// Template that lets you decompose a function signature (R(PARAMS)) into
// components.
// --------------------------------------------------------------------

/*
 * Helper template to extract return value type from a function
 * signature by means of partial template specialization.
 * The type of the return value is stored in the type member
 * 'return_type'.
 * The function signature is stored in the type member 'signature'. For member
 * functions, it omits the type of the instance and converts the signature
 * to a normal non-member function signature. That is:
 *     int(T::*(std::string)) => int(std::string).
 */
template<typename signature>
struct signature_decomp;

template<typename R, typename... args>
struct signature_decomp<R(args...)> {
    using return_type = R;
    using signature = R(args...);
};

// member function
template<typename obj, typename R, typename... args>
struct signature_decomp<R (obj::*)(args...)> {
    using return_type = R;
    using signature = R(args...);
    using constness = std::false_type;
    using class_t = obj;
};

// Const member function
template<typename obj, typename R, typename... args>
struct signature_decomp<R (obj::*)(args...) const> {
    using return_type = R;
    using signature = R(args...);
    using constness = std::true_type;
    using class_t = obj;
};

template<typename signature>
using signature_decomp_return_t =
  typename signature_decomp<signature>::return_type;

template<typename signature>
using signature_decomp_signature_t =
  typename signature_decomp<signature>::signature;

/* Extract the class type from a member function signature e.g.
 *   int (T::*)(const std::string &s) const => T.
 */
template<typename signature>
using signature_decomp_memfn_class_t =
  typename signature_decomp<signature>::class_t;

//

}  // namespace type_traits
}  // namespace tarp
