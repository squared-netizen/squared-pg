// SPDX-License-Identifier: MIT
//
// sqcart CLI — the jar-like archive utility.
//
// The single design constraint here is that no command function touches
// std::cout, std::cerr, std::cin, argc/argv, getenv, or exit(). Everything
// flows through Environment. main.cpp is the only translation unit that knows
// a terminal exists, and it is a dozen lines long.
//
// That is not stylistic. It is what makes the CLI testable: a test constructs
// an Environment over string streams, calls run(), and asserts on the exit
// code and the captured output — no subprocess, no temp-file scraping, no
// shell. See app/tests/test_cli.cpp.

#ifndef SQCART_APP_CLI_HPP
#define SQCART_APP_CLI_HPP

#include <filesystem>
#include <iosfwd>
#include <optional>
#include <utility>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sqcart::app {

/// Process exit codes. Distinguished so CI can tell "this cartridge is
/// non-conforming" from "you typed the command wrong" — collapsing both to 1
/// is the mistake that makes a verify step untrustworthy in a pipeline.
enum class ExitCode : int {
    ok            = 0,
    failure       = 1,  ///< The operation failed: bad cartridge, IO error.
    usage         = 2,  ///< Malformed invocation. Nothing was attempted.
    non_conforming = 3, ///< `verify` ran and the cartridge did not conform.
};

[[nodiscard]] constexpr int to_int(ExitCode c) noexcept
{
    return static_cast<int>(c);
}

/// Everything a command may touch outside its own arguments.
///
/// `out` is reserved for machine-readable results — entry listings, digests,
/// manifest JSON. `err` carries diagnostics, progress and error text. A
/// caller piping `sqcart list x.sq | ...` must never receive a warning on
/// stdout, which is why the split is enforced by the type rather than by
/// convention.
struct Environment {
    std::ostream& out;
    std::ostream& err;
    std::istream& in;

    /// Working directory for resolving relative paths. Explicit rather than
    /// implicit so tests need not chdir the whole process.
    std::filesystem::path cwd{};

    /// Whether `out` is an interactive terminal. Commands use this only to
    /// decide presentation (column padding, colour), never behaviour.
    bool out_is_tty{false};
};

/// A parsed invocation, produced by parse() and consumed by dispatch().
/// Separating the two makes argument handling testable on its own.
struct Invocation {
    std::string              command;
    std::vector<std::string> positional;
    std::vector<std::string> flags;      ///< Long flags, without leading "--".
    std::vector<std::pair<std::string, std::string>> options;  ///< --key=value

    [[nodiscard]] bool has_flag(std::string_view name) const noexcept;
    [[nodiscard]] std::optional<std::string> option(std::string_view name) const;
};

/// Run the CLI. `args` excludes argv[0].
///
/// This is the whole entry point. main() constructs an Environment over the
/// real streams and calls this; a test constructs one over stringstreams and
/// calls the same function.
[[nodiscard]] ExitCode run(std::span<const std::string> args, Environment& env);

/// Write the usage text to `stream`. Exposed so both `--help` and usage
/// errors emit the identical text from one source.
void write_usage(std::ostream& stream);

}  // namespace sqcart::app

#endif  // SQCART_APP_CLI_HPP
