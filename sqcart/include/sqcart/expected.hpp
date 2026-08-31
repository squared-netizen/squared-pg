// SPDX-License-Identifier: MIT
//
// sqcart/expected.hpp — Result channel storage.
//
// std::expected is C++23. This project targets C++20, so the type may be
// unavailable. When the toolchain provides it, sqcart::expected is a plain
// alias and no code is generated here. Otherwise a minimal substitute is
// used, covering only the subset the public API needs.
//
// This is the one place sqcart passes over a standard-library facility. The
// justification is availability, not preference: the substitute is designed
// to be deleted, and switching to std::expected must be source-compatible.
// See docs/developer/sqcart/expected.md.
//
// Substitute limitations, deliberate:
//   - no monadic operations (and_then, transform, or_else)
//   - no std::unexpected CTAD games
//   - no constexpr guarantees beyond the trivial cases
//   - error type must be nothrow-move-constructible
// If code needs any of those, it is relying on C++23 and should say so.

#ifndef SQCART_EXPECTED_HPP
#define SQCART_EXPECTED_HPP

#if defined(__has_include)
#  if __has_include(<version>)
#    include <version>
#  endif
#endif

#if !defined(SQCART_HAVE_STD_EXPECTED)
#  if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
#    define SQCART_HAVE_STD_EXPECTED 1
#  else
#    define SQCART_HAVE_STD_EXPECTED 0
#  endif
#endif

#if SQCART_HAVE_STD_EXPECTED

#include <expected>

namespace sqcart {
template <class T, class E>
using expected = std::expected<T, E>;

template <class E>
using unexpected = std::unexpected<E>;
}  // namespace sqcart

#else  // ---------------------------------------------------------------

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace sqcart {

template <class E>
class unexpected {
public:
    constexpr explicit unexpected(E error) noexcept(
        std::is_nothrow_move_constructible_v<E>)
        : error_(std::move(error)) {}

    [[nodiscard]] constexpr const E& error() const& noexcept { return error_; }
    [[nodiscard]] constexpr E&&      error() &&     noexcept { return std::move(error_); }

private:
    E error_;
};

template <class E>
unexpected(E) -> unexpected<E>;

namespace detail {

template <class T, class E>
class expected_storage {
protected:
    union {
        T value_;
        E error_;
    };
    bool has_value_;

    expected_storage() : value_(), has_value_(true) {}

    explicit expected_storage(T v) : value_(std::move(v)), has_value_(true) {}

    explicit expected_storage(unexpected<E> u)
        : error_(std::move(u).error()), has_value_(false) {}

    expected_storage(const expected_storage& other) : has_value_(other.has_value_) {
        if (has_value_) { ::new (std::addressof(value_)) T(other.value_); }
        else            { ::new (std::addressof(error_)) E(other.error_); }
    }

    expected_storage(expected_storage&& other) noexcept(
        std::is_nothrow_move_constructible_v<T> &&
        std::is_nothrow_move_constructible_v<E>)
        : has_value_(other.has_value_) {
        if (has_value_) { ::new (std::addressof(value_)) T(std::move(other.value_)); }
        else            { ::new (std::addressof(error_)) E(std::move(other.error_)); }
    }

    ~expected_storage() { destroy(); }

    void destroy() noexcept {
        if (has_value_) { value_.~T(); }
        else            { error_.~E(); }
    }

    expected_storage& operator=(expected_storage other) noexcept(
        std::is_nothrow_move_constructible_v<T> &&
        std::is_nothrow_move_constructible_v<E>) {
        destroy();
        has_value_ = other.has_value_;
        if (has_value_) { ::new (std::addressof(value_)) T(std::move(other.value_)); }
        else            { ::new (std::addressof(error_)) E(std::move(other.error_)); }
        return *this;
    }
};

}  // namespace detail

/// Minimal std::expected substitute. See file header for the omitted subset.
template <class T, class E>
class expected : private detail::expected_storage<T, E> {
    using base = detail::expected_storage<T, E>;

public:
    using value_type      = T;
    using error_type      = E;
    using unexpected_type = unexpected<E>;

    expected() : base() {}
    expected(T value) : base(std::move(value)) {}
    expected(unexpected<E> error) : base(std::move(error)) {}

    expected(const expected&)     = default;
    expected(expected&&) noexcept = default;

    // Not defaulted: the base stores its alternatives in a union and assigns
    // through a by-value operator, which cannot be the implicit signature.
    expected& operator=(const expected& other) {
        base::operator=(static_cast<const base&>(other));
        return *this;
    }

    expected& operator=(expected&& other) noexcept(
        std::is_nothrow_move_constructible_v<T> &&
        std::is_nothrow_move_constructible_v<E>) {
        base::operator=(static_cast<base&&>(other));
        return *this;
    }

    [[nodiscard]] constexpr bool has_value() const noexcept { return this->has_value_; }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return has_value(); }

    [[nodiscard]] constexpr T&        value() &       noexcept { return this->value_; }
    [[nodiscard]] constexpr const T&  value() const&  noexcept { return this->value_; }
    [[nodiscard]] constexpr T&&       value() &&      noexcept { return std::move(this->value_); }

    [[nodiscard]] constexpr T&        operator*() &      noexcept { return this->value_; }
    [[nodiscard]] constexpr const T&  operator*() const& noexcept { return this->value_; }
    [[nodiscard]] constexpr T&&       operator*() &&     noexcept { return std::move(this->value_); }

    [[nodiscard]] constexpr T*        operator->()       noexcept { return std::addressof(this->value_); }
    [[nodiscard]] constexpr const T*  operator->() const noexcept { return std::addressof(this->value_); }

    [[nodiscard]] constexpr E&        error() &       noexcept { return this->error_; }
    [[nodiscard]] constexpr const E&  error() const&  noexcept { return this->error_; }
    [[nodiscard]] constexpr E&&       error() &&      noexcept { return std::move(this->error_); }
};

/// void specialisation, needed by operations returning Result<void>.
template <class E>
class expected<void, E> {
public:
    using value_type      = void;
    using error_type      = E;
    using unexpected_type = unexpected<E>;

    expected() noexcept : has_value_(true) {}
    expected(unexpected<E> error) : error_(std::move(error).error()), has_value_(false) {}

    [[nodiscard]] constexpr bool has_value() const noexcept { return has_value_; }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return has_value_; }

    constexpr void value() const noexcept {}

    [[nodiscard]] constexpr const E& error() const& noexcept { return error_; }
    [[nodiscard]] constexpr E&&      error() &&     noexcept { return std::move(error_); }

private:
    E    error_{};
    bool has_value_;
};

}  // namespace sqcart

#endif  // SQCART_HAVE_STD_EXPECTED

#endif  // SQCART_EXPECTED_HPP
