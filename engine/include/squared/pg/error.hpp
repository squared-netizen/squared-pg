// SPDX-License-Identifier: MIT
//
// squared/pg/error.hpp — the structured error model.
//
// Specification: §2.15.1–2.15.6 (error structure, categories, codes,
// recoverable vs fatal, propagation), §2.4.8 (errors are part of the result
// system), §2.6.12 (errors cross the boundary as data).
//
// Two identifier fields, per D-017. `category` is a closed enum so a workflow
// can switch on it exhaustively. `code` is an open dotted string so a new
// specific failure can be added without breaking a workflow that branches on
// the coarse category.

#ifndef SQUARED_PG_ERROR_HPP
#define SQUARED_PG_ERROR_HPP

#include "squared/pg/value.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#if defined(__has_include)
#  if __has_include(<version>)
#    include <version>
#  endif
#endif

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
#  define SQUARED_PG_HAVE_STD_EXPECTED 1
#else
#  define SQUARED_PG_HAVE_STD_EXPECTED 0
#endif

#if SQUARED_PG_HAVE_STD_EXPECTED
#  include <expected>
#else
#  include "sqcart/expected.hpp"
#endif

namespace squared::pg {

/// §2.15.2. Closed by design: a workflow must be able to enumerate the
/// branches it handles and be told by the compiler when a new one appears.
enum class ErrorCategory : std::uint8_t {
    configuration,
    validation,
    resource,
    template_,   ///< `template` is a keyword; the wire token is "template"
    kit,
    package,
    asset,
    framework,
    filesystem,
    transaction,
    compatibility,
    capability,
    lifecycle,
    internal,
};

[[nodiscard]] std::string_view to_string(ErrorCategory category) noexcept;

/// §2.15.1. Every field beyond category/code/message is optional context; a
/// workflow branches on the first two and reports the rest.
struct EngineError {
    ErrorCategory category{ErrorCategory::internal};
    std::string   code;         ///< `<category>.<subject>.<condition>`, §2.15.3
    std::string   message;      ///< human-readable; never parsed
    std::string   operation;    ///< operation identity that failed
    std::string   resource;     ///< affected resource identity, when applicable
    std::string   path;         ///< affected workspace-relative path, when applicable
    Value         diagnostics;  ///< structured detail; see §2.15.3 for why this matters
    bool          recoverable{false};

    [[nodiscard]] Value to_value() const;
};

/// Construct an error, deriving nothing: the category and code are stated by
/// the caller because a code invented from a category would not be one of the
/// documented codes workflows branch on.
[[nodiscard]] EngineError make_error(ErrorCategory category, std::string code, std::string message);

enum class Severity : std::uint8_t { info, warning, error };

[[nodiscard]] std::string_view to_string(Severity severity) noexcept;

struct Diagnostic {
    Severity    severity{Severity::info};
    std::string message;
    std::string resource;
    std::string path;

    [[nodiscard]] Value to_value() const;
};

#if SQUARED_PG_HAVE_STD_EXPECTED
template <class T>
using Result = std::expected<T, EngineError>;
using Unexpected = std::unexpected<EngineError>;
#else
/// The toolchain lacks <expected>. Rather than vendor a second substitute,
/// reuse the one sqcart already ships and tests (sqcart/expected.hpp);
/// sqcart is linked from vendored in-tree source regardless, so this costs
/// nothing and keeps one substitute under test instead of two.
template <class T>
using Result = sqcart::expected<T, EngineError>;
using Unexpected = sqcart::unexpected<EngineError>;
#endif

/// Shorthand for the overwhelmingly common failure return.
[[nodiscard]] Unexpected fail(ErrorCategory category, std::string code, std::string message);

}  // namespace squared::pg

#endif  // SQUARED_PG_ERROR_HPP
