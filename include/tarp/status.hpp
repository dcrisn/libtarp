#pragma once

#include <optional>
#include <string>

namespace tarp {

// Return value that stores true on success or an error string on error.
// NOTE: this encodes the common expected<bool, error_string> case
// (see excpected.hpp)
class status {
public:
    status() = default;

    // Construct error
    explicit status(std::string error) : m_error(std::move(error)) {}

    // Static helpers
    static status OK() { return status(); }

    static status E(std::string err) { return status(std::move(err)); }

    // Query state
    bool ok() const noexcept { return !m_error.has_value(); }

    explicit operator bool() const noexcept { return ok(); }

    // Access error
    const std::string &e() const { return *m_error; }

private:
    std::optional<std::string> m_error;
};

template<typename T>
status get_status(T t, const std::string &e) {
    if constexpr (std::is_same_v<bool, T>) {
        if (!t) {
            return status::E(e);
        }
    } else if constexpr (std::is_same_v<status, T>) {
        if (!t) {
            return status::E(e + ": " + t.e());
        }
    } else {
        static_assert("bad status argument");
    }
    return status::OK();
}

#define OK_OR_RETURN(status_instance, errstr)       \
    if (!(status_instance)) {                       \
        return get_status(status_instance, errstr); \
    }

}  // namespace tarp
