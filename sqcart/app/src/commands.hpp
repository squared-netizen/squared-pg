// SPDX-License-Identifier: MIT
//
// Internal command table. Every command has the same shape:
//
//     ExitCode cmd_x(const Invocation&, Environment&);
//
// A command receives its arguments and its IO and returns an exit code. It
// does not read globals, does not call exit(), and does not touch std::cout.
// That uniformity is what lets test_cli.cpp drive all of them identically.

#ifndef SQCART_APP_COMMANDS_HPP
#define SQCART_APP_COMMANDS_HPP

#include "sqcart/app/cli.hpp"
#include "sqcart/sqcart.hpp"

#include <string>

namespace sqcart::app {

// Reuse the library's Result channel for CLI-internal fallibility, with a
// plain string error: argument problems are for humans, not for programmatic
// recovery, so they do not deserve an ErrorCode.
template <class T>
using Result = sqcart::expected<T, std::string>;

Result<Invocation> parse(std::span<const std::string> args);

ExitCode cmd_list(const Invocation&, Environment&);
ExitCode cmd_show(const Invocation&, Environment&);
ExitCode cmd_info(const Invocation&, Environment&);
ExitCode cmd_cat(const Invocation&, Environment&);
ExitCode cmd_verify(const Invocation&, Environment&);
ExitCode cmd_digest(const Invocation&, Environment&);
ExitCode cmd_unpack(const Invocation&, Environment&);
ExitCode cmd_pack(const Invocation&, Environment&);
ExitCode cmd_create(const Invocation&, Environment&);

// --- shared helpers ------------------------------------------------------

/// Resolve a possibly-relative path against the Environment's cwd rather than
/// the process cwd, so tests need not chdir.
std::filesystem::path resolve(const Environment& env, std::string_view p);

/// Build OpenOptions from the common flags (--lenient, --verify-integrity,
/// --symlinks).
OpenOptions open_options_from(const Invocation& inv);

/// Parse --symlinks=skip|follow|reject. Defaults to skip; an unrecognised
/// value also yields skip, and the caller warns.
SymlinkPolicy symlink_policy_from(const Invocation& inv);

/// Print a library Error to stderr in a consistent form.
void report(Environment& env, const Error& e);

/// Open the cartridge named by the first positional argument, reporting any
/// failure. Empty on error, with the message already written to env.err.
std::optional<Cartridge> open_positional(const Invocation& inv, Environment& env,
                                         std::string_view usage_hint);

}  // namespace sqcart::app

#endif  // SQCART_APP_COMMANDS_HPP
