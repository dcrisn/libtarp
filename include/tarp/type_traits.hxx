#pragma once

#include <tuple>
#include <type_traits>

#include <tarp/cxxcommon.hxx>

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

#define CLASS_OF(member_function) \
    tarp::type_traits::signature_decomp_memfn_class_t<decltype(member_function)>

//

/*------------------------------------------------------------
 * Detect if a class C implements the specified interface.
 *
 * NOTE: this is a crutch meant to be useful pre-C++20 (when
 * concepts are introduced).
 *
 * The 'interface' argument should be a non-template class
 * that contains a template class member named 'constraints',
 * instantiable with a single template argument, which is the
 * name of the class being tested for interface compliance.
 *
 * The 'interface' class (specifically, its 'constraints')
 * member ultimatey determines whether this class will evaluate
 * to true (=C implements the expected interface) or false (=C
 * does not implement the expected interface).
 *------------------------------------------------------------*/

namespace impl {

// General fallback case. This template gets selected if the more
// specialized version below is not well-formed.
template<typename C, typename interface, typename = void>
struct implements_interface : std::false_type {};

// This specialization will be selected over the default if well-formed.
// If this gets selected, then C is deemed to comply with the expected
// interface.
template<typename C, typename interface>
struct implements_interface<
  C,
  interface,
  std::void_t<typename interface::template constraints<C>>> : std::true_type {};
}  // namespace impl

/*
 * Public API; used to hide the third unnamed template parameter to the
 * impl::implements_interface template. */
template<typename C, typename interface>
using implements_interface = impl::implements_interface<C, interface>;

template<typename C, typename interface>
inline constexpr bool implements_interface_v =
  impl::implements_interface<C, interface>::value;

/* Convenient way to check a class inherits from an interface; will do in the
 * absence of C++20's concepts. */
#define REPORT_INTERFACE_VIOLATION(TYPE, INTERFACE)         \
    "Interface Requirement Violation (at " __FILE__         \
    ":" tkn2str(__LINE__) "): type specified for " tkn2str( \
      TYPE) " does not implement required interface (" tkn2str(INTERFACE) ")"

#define REQUIRE(type_argument, required_interface)                     \
    static_assert(                                                     \
      (tarp::type_traits::implements_interface_v<type_argument,        \
                                                 required_interface>), \
      REPORT_INTERFACE_VIOLATION(type_argument, required_interface))

// Helper to be used for member function signature compliance checks inside
// interface validators.
#define VALIDATE(memfn_address, signature) \
    decltype(static_cast<signature>(memfn_address)) = memfn_address

//

// Given a variadic list of types, convert this to a tuple and produce
// a member type named T. Make T the same as the first (and only) type
// in types... if the size of the tuple is 1.
// Otherwise make it a std::tuple<types...> type.
//
// Additionally, if the size of the tuple is gt 1 (meaning the member T
// will in fact be a tuple), then the member 'is_tuple' will be a
// std::true_type, otherwise it will be a std::false_type.
// This allows you to check whether the member T is a tuple or not.
template<typename... types>
struct type_or_tuple {
    using tup = std::tuple<types...>;
    using T = std::conditional_t<(std::tuple_size_v<tup> == 1),
                                 std::tuple_element_t<0, tup>,
                                 tup>;

    using is_tuple = std::conditional_t<(std::tuple_size_v<tup> > 1),
                                        std::true_type,
                                        std::false_type>;
};

template<typename... types>
using type_or_tuple_t = typename type_or_tuple<types...>::T;

template<typename... types>
inline constexpr bool is_tuple_v = type_or_tuple<types...>::is_tuple::value;
}  // namespace type_traits



}  // namespace tarp
