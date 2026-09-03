// SPDX-License-Identifier: MIT
//
// squared/pg/version.hpp — SemVer 2.0.0 and the range comparator grammar.
//
// Specification: §2.8.2 (version and constraint grammar). The concrete
// grammar implemented here is the cartridge format's §5.3 restricted
// comparator form, which the repository already ships manifests in:
//
//     range   := clause ( " " clause )*
//     clause  := op version
//     op      := ">=" | ">" | "<=" | "<" | "=" | "~" | "^"
//
// Clauses are conjunctive. §2.8.2 additionally admits a bare version meaning
// exact match and "*" meaning any; both are accepted here because manifests
// in the wild use them and rejecting them would fail resolution for a reason
// the author cannot see.

#ifndef SQUARED_PG_VERSION_HPP
#define SQUARED_PG_VERSION_HPP

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace squared::pg {

/// A SemVer 2.0.0 version. Build metadata is retained for round-tripping but
/// ignored for ordering, per SemVer §10.
class Version {
public:
    Version() = default;

    [[nodiscard]] static std::optional<Version> parse(std::string_view text);

    [[nodiscard]] std::uint32_t major() const noexcept { return major_; }
    [[nodiscard]] std::uint32_t minor() const noexcept { return minor_; }
    [[nodiscard]] std::uint32_t patch() const noexcept { return patch_; }
    [[nodiscard]] const std::string& prerelease() const noexcept { return prerelease_; }
    [[nodiscard]] const std::string& build() const noexcept { return build_; }
    [[nodiscard]] bool is_prerelease() const noexcept { return !prerelease_.empty(); }

    [[nodiscard]] std::string to_string() const;

    /// SemVer precedence. Pre-release identifiers compare per SemVer §11.4:
    /// numeric identifiers numerically, alphanumeric ones lexically, numeric
    /// below alphanumeric, and a larger set of identifiers above a smaller
    /// one where all preceding identifiers are equal.
    [[nodiscard]] std::strong_ordering operator<=>(const Version& other) const noexcept;
    [[nodiscard]] bool operator==(const Version& other) const noexcept;

private:
    std::uint32_t major_{0};
    std::uint32_t minor_{0};
    std::uint32_t patch_{0};
    std::string   prerelease_;
    std::string   build_;
};

/// A conjunction of comparator clauses.
class VersionRange {
public:
    enum class Op : std::uint8_t { eq, lt, lte, gt, gte, caret, tilde };

    struct Clause {
        Op      op{Op::eq};
        Version version;
    };

    VersionRange() = default;

    /// Parse a range expression. An empty string, "*" and "any" all produce
    /// the unconstrained range.
    [[nodiscard]] static std::optional<VersionRange> parse(std::string_view text);

    /// Whether `version` satisfies every clause.
    ///
    /// A pre-release version is only admitted when some clause names a
    /// pre-release of the same (major, minor, patch) — §2.8.2 forbids
    /// selecting a pre-release that was not explicitly asked for.
    [[nodiscard]] bool satisfied_by(const Version& version) const noexcept;

    [[nodiscard]] bool unconstrained() const noexcept { return clauses_.empty(); }
    [[nodiscard]] const std::vector<Clause>& clauses() const noexcept { return clauses_; }
    [[nodiscard]] const std::string& text() const noexcept { return text_; }

private:
    std::vector<Clause> clauses_;
    std::string         text_;
};

}  // namespace squared::pg

#endif  // SQUARED_PG_VERSION_HPP
