#pragma once

#include <optional>
#include <stdexcept>

// Rust-y return type that encodes error and success in one object.
//
// I've been using std::pair<bool, std::string> all over to enable
// flows like this:
//   auto [ok,e] = f();
//   if (!ok){
//   ...
//   }
//
// Another common pattern:
//   auto [res, e] = f();
//   if (!res.has_value()){  /* std::optional */
//      // handle error
//   }
//
// This class generalizes that approach by mimicking the Expected type of Rust.
// The idea is similar to the pair but the solution is more general and arguably
// even simpler and friendlier:
//  auto res = f();
//  if (!res){
//      auto e = res.e();
//      // handle error.
//  }
//
// The std::pair<bool, std::string> is then simply expected<bool,std::string>;
template<typename T, typename ERR>
class expected {
    explicit expected(std::optional<T> ok, std::optional<ERR> err)
        : m_ok_value(std::move(ok)), m_error_value(std::move(err)) {
        if (m_ok_value.has_value() and m_error_value.has_value()) {
            throw std::logic_error("Illegal construction of expected<T,ERR> "
                                   "with both alternatives set");
        }
    }

public:
    // for tag dispatching to disambiguate case when std::is_same<T,ERR>.
    struct OKtag {};

    struct Etag {};

    static inline constexpr OKtag oktag = OKtag();
    static inline constexpr Etag etag = Etag();

    // Constructors for success and error cases
    expected(const T &value, OKtag = {})
        : expected(std::make_optional(value), std::nullopt) {}

    expected(T &&value, OKtag = {})
        : expected(std::make_optional(std::move(value)), std::nullopt) {}

    expected(const ERR &error, Etag = {})
        : expected(std::nullopt, std::make_optional(error)) {}

    expected(ERR &&error, Etag = {})
        : expected(std::nullopt, std::make_optional(std::move(error))) {}

    static auto OK(const T &value) { return expected(value, OKtag {}); }

    static auto E(const ERR &error) {
        return expected(std::move(error), Etag {});
    }

    bool has_value() const noexcept { return m_ok_value.has_value(); }

    operator bool() const noexcept { return has_value(); }

    bool ok() const { return has_value(); }

    // Accessors
    const T &value() const { return m_ok_value.value(); }

    const T &operator*() const { return value(); }

    const ERR &e() const { return m_error_value.value(); }

private:
    std::optional<T> m_ok_value;
    std::optional<ERR> m_error_value;
};

// ============================
// Specialization for T=void.
// ============================
template<typename ERR>
class expected<void, ERR> {
public:
    // Success constructor
    expected() : m_ok(true) {}

    // Error constructors
    expected(const ERR &error) : m_error_value(error) {}

    expected(ERR &&error) : m_error_value(std::move(error)) {}

    static auto OK() { return expected(); }

    static auto E(const ERR &error) { return expected(error); }

    static auto E(ERR &&error) { return expected(std::move(error)); }

    bool has_value() const noexcept { return m_ok.has_value(); }

    operator bool() const noexcept { return has_value(); }

    bool ok() const noexcept { return has_value(); }

    const ERR &e() const { return m_error_value.value(); }

private:
    std::optional<bool> m_ok;
    std::optional<ERR> m_error_value;
};
