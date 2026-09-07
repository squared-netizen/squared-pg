// SPDX-License-Identifier: MIT
//
// squared/pg/identity.hpp — resource identity.
//
// Specification: §2.8.1 (identifier grammar), §2.6.7 (identity across the
// boundary), §2.4.4 (identity is never a path).
//
// ResourceId is a distinct type rather than a std::string alias because the
// distinction it carries is the whole point: a value of this type has been
// checked against the grammar and its type prefix agrees with its kind. A
// std::string that happens to contain dots has been checked against nothing.
//
// Grammar note. §2.8.1 writes segments as [a-z][a-z0-9]*(-[a-z0-9]+)*; the
// cartridge format §5.2, which the shipped manifests and the sqcart reader
// already enforce, writes them as [a-z][a-z0-9_]* with two to eight segments.
// The repository is ground truth during initial implementation (AGENTS.md),
// so the cartridge grammar is implemented and both separators are accepted:
// underscores because manifests use them, hyphens because the specification
// asks for them. This divergence is recorded as D-030.

#ifndef SQUARED_PG_IDENTITY_HPP
#define SQUARED_PG_IDENTITY_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace squared::pg {

/// The resource kinds the generator composes. Mirrors sqcart::Kind, minus the
/// container-only kinds the generator never resolves as a resource.
enum class ResourceKind : std::uint8_t {
    project_template,  ///< `template` is a keyword; the manifest token is "template"
    kit,
    package,
    asset,
    plugin,
    cartridge,
};

[[nodiscard]] std::string_view to_string(ResourceKind kind) noexcept;
[[nodiscard]] std::optional<ResourceKind> resource_kind_from_string(std::string_view text) noexcept;

/// The token a resource identity of this kind must begin with.
[[nodiscard]] std::string_view identity_prefix(ResourceKind kind) noexcept;

class ResourceId {
public:
    ResourceId() = default;

    /// Validate and construct. Empty when the text violates §2.8.1/§5.2.
    [[nodiscard]] static std::optional<ResourceId> parse(std::string_view text);

    /// Validate and construct, additionally requiring the type prefix to
    /// match `kind`.
    [[nodiscard]] static std::optional<ResourceId> parse(std::string_view text, ResourceKind kind);

    [[nodiscard]] const std::string& str() const noexcept { return text_; }
    [[nodiscard]] std::string_view view() const noexcept { return text_; }
    [[nodiscard]] bool empty() const noexcept { return text_.empty(); }

    /// The kind implied by the leading segment, when it names one.
    [[nodiscard]] std::optional<ResourceKind> kind() const noexcept;

    [[nodiscard]] std::vector<std::string_view> segments() const;

    [[nodiscard]] bool operator==(const ResourceId& other) const noexcept { return text_ == other.text_; }
    [[nodiscard]] auto operator<=>(const ResourceId& other) const noexcept { return text_ <=> other.text_; }

private:
    explicit ResourceId(std::string text) : text_(std::move(text)) {}
    std::string text_;
};

/// A resource reference: an identity plus an optional version constraint,
/// written `identity` or `identity@range` (§2.8.2).
struct ResourceRef {
    ResourceId  id;
    std::string range;  ///< empty means unconstrained

    [[nodiscard]] static std::optional<ResourceRef> parse(std::string_view text);
    [[nodiscard]] std::string to_string() const;
};

/// Reserved namespaces, §2.8.1. `*.squared_pg.*` is generator-internal; a
/// user resource claiming it is an authoring mistake worth reporting.
[[nodiscard]] bool is_reserved_namespace(const ResourceId& id) noexcept;

}  // namespace squared::pg

#endif  // SQUARED_PG_IDENTITY_HPP
