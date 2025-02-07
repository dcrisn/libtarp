#pragma once

#include <exception>
#include <string>

#if __cplusplus >= 202002L
#include <format>
#endif

#include <tarp/ioutils.hxx>

namespace tarp {
namespace exception {

class NullPtrDereference : public std::exception {
public:
    explicit NullPtrDereference(const char *message) : m_errstring(message) {}

    explicit NullPtrDereference(const std::string &message)
        : m_errstring(message) {}

    virtual ~NullPtrDereference() noexcept {}

    virtual const char *what() const noexcept { return m_errstring.c_str(); }

protected:
    std::string m_errstring;
};

#ifdef DISABLE_NULL_POINTER_DEREFERENCE_EXCEPTIONS
#define throw_on_nullptr(ptr) \
    do {                      \
    } while (0)
#else
#define throw_on_nullptr(ptr)                                     \
    do {                                                          \
        if (ptr == nullptr) {                                     \
            throw tarp::exception::NullPtrDereference(            \
              "Illegal attempt to dereference null pointer in " + \
              std::string(__PRETTY_FUNCTION__));                  \
        }                                                         \
    } while (0)
#endif

//

// Throw an exception with a formatted message.
// The input args are fed directly to snprintf
// to produce a formatted string.
// Example:
//    do_throwc<std::logic_error>("BUG in myfunc: %s", errstr);
template<typename exception_t, typename... msg>
void do_throwc(msg &&...vargs) {
    throw exception_t(tarp::utils::io::sfmt(std::forward<msg>(vargs)...));
}

// Throw an exception with a formatted message.
// The inputs args are fed directly to std::format
// to produce a formatted string.
// Example:
//    do_throw<std::logic_error>("BUG in myfunc: {}", errstr);
#if __cplusplus >= 202002L

template<typename exception_t, typename... msg>
[[noreturn]] void do_throw(std::format_string<msg...> fmt, msg &&...vargs) {
    throw exception_t(std::vformat(fmt.get(), std::make_format_args(vargs...)));
}

#endif


}  // namespace exception
}  // namespace tarp
