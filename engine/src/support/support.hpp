// SPDX-License-Identifier: MIT
//
// engine/src/support/support.hpp — internal helpers.
//
// Private to the engine. §1.3 requires the public/private boundary to be
// explicit: nothing here is reachable from engine/include/, and none of these
// types appear in a public signature.

#ifndef SQUARED_PG_SUPPORT_HPP
#define SQUARED_PG_SUPPORT_HPP

#include "squared/pg/error.hpp"
#include "squared/pg/value.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace squared::pg::support {

// ---------------------------------------------------------------------------
// Digests
//
// SHA-256 is not in the standard library and cannot be reasonably approximated
// with what is, so an implementation is vendored: Brad Conte's public-domain
// code at third-party/crypto-algorithms. This is the engine's only pass over a
// standard-library facility, and it is availability rather than preference.
// Provenance hashes are the mechanism behind §2.7.10 conflict detection, so
// the engine cannot do without one.
// ---------------------------------------------------------------------------

using Digest = std::array<std::byte, 32>;

[[nodiscard]] Digest sha256(std::span<const std::byte> bytes);
[[nodiscard]] Digest sha256(std::string_view text);
[[nodiscard]] std::string to_hex(const Digest& digest);

// ---------------------------------------------------------------------------
// Glob matching
//
// Ownership rules and template `files` entries are glob patterns (§2.8.6,
// §2.7.10). std::regex would need every pattern translated first, and the
// translation is where the bugs live; a direct matcher is 60 lines and has one
// behaviour to document.
// ---------------------------------------------------------------------------

/// Match `path` against `pattern`.
///
///   `*`  matches any run of characters except `/`
///   `**` matches any run of characters including `/`
///   `?`  matches one character except `/`
///
/// A trailing `/**` also matches the directory itself, so `sq_app/**` covers
/// `sq_app` — otherwise every manifest would need both patterns.
[[nodiscard]] bool glob_match(std::string_view pattern, std::string_view path);

// ---------------------------------------------------------------------------
// JSON
//
// yyjson, vendored at third-party/yyjson-0.12.0 and shared with sqcart through
// its SQCART_USE_EXTERNAL_YYJSON option so one build never links two copies.
// ---------------------------------------------------------------------------

/// Parse JSON text into a Value.
[[nodiscard]] Result<Value> json_parse(std::string_view text);

/// Serialise a Value as JSON.
///
/// Object member order is preserved (Value stores ordered pairs), so writing
/// the same Value twice produces identical bytes. §2.7.13 requires generated
/// output to be reproducible, and workspace metadata is generated output.
[[nodiscard]] std::string json_write(const Value& value, bool pretty = true);

// ---------------------------------------------------------------------------
// Paths and text
// ---------------------------------------------------------------------------

/// Normalise a workspace-relative path: forward slashes, no `.` components,
/// no leading or duplicated separators. Empty when the input escapes the
/// workspace or is absolute — the engine never writes outside its target.
[[nodiscard]] std::string normalize_relative(std::string_view path);

/// Split on `/`, discarding empty components.
[[nodiscard]] std::vector<std::string_view> split_path(std::string_view path);

/// Every ancestor directory of a workspace-relative file path, shallowest
/// first. Used to emit create_directory steps before the write that needs them.
[[nodiscard]] std::vector<std::string> ancestor_directories(std::string_view path);

[[nodiscard]] std::string to_lower(std::string_view text);

}  // namespace squared::pg::support

#endif  // SQUARED_PG_SUPPORT_HPP
