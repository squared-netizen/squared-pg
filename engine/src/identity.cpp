// SPDX-License-Identifier: MIT

#include "squared/pg/identity.hpp"

#include <algorithm>

namespace squared::pg {
namespace {

constexpr std::size_t kMaxIdentifierBytes = 128;  // §2.8.1
constexpr std::size_t kMinSegments        = 2;    // cartridge format §5.2
constexpr std::size_t kMaxSegments        = 8;

[[nodiscard]] bool valid_segment(std::string_view segment) {
    if (segment.empty()) return false;
    const char first = segment.front();
    if (first < 'a' || first > 'z') return false;
    // A separator may not end a segment, and may not double.
    char previous = first;
    for (std::size_t i = 1; i < segment.size(); ++i) {
        const char c = segment[i];
        const bool separator = (c == '_' || c == '-');
        const bool alnum     = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (!separator && !alnum) return false;
        if (separator && (previous == '_' || previous == '-')) return false;
        previous = c;
    }
    return previous != '_' && previous != '-';
}

}  // namespace

std::string_view to_string(ResourceKind kind) noexcept {
    switch (kind) {
        case ResourceKind::project_template: return "template";
        case ResourceKind::kit: return "kit";
        case ResourceKind::package: return "package";
        case ResourceKind::asset: return "asset";
        case ResourceKind::plugin: return "plugin";
        case ResourceKind::cartridge: return "cartridge";
    }
    return "template";
}

std::optional<ResourceKind> resource_kind_from_string(std::string_view text) noexcept {
    if (text == "template") return ResourceKind::project_template;
    if (text == "kit") return ResourceKind::kit;
    if (text == "package") return ResourceKind::package;
    // The cartridge format names the asset kind "asset_bundle"; the generator
    // architecture calls it "asset". Both spellings map to one kind so a
    // manifest written against either document indexes.
    if (text == "asset" || text == "asset_bundle" || text == "assets") return ResourceKind::asset;
    if (text == "plugin") return ResourceKind::plugin;
    if (text == "cartridge") return ResourceKind::cartridge;
    return std::nullopt;
}

std::string_view identity_prefix(ResourceKind kind) noexcept {
    // A cartridge id is a reverse-DNS name under a domain the author controls
    // (format §5.2), so it carries no kind prefix. Returning an empty prefix
    // makes "does the prefix match" vacuously true for that kind.
    if (kind == ResourceKind::cartridge) return {};
    return to_string(kind);
}

std::optional<ResourceId> ResourceId::parse(std::string_view text) {
    if (text.empty() || text.size() > kMaxIdentifierBytes) return std::nullopt;

    std::size_t count = 0;
    std::size_t start = 0;
    while (true) {
        const std::size_t dot = text.find('.', start);
        const std::string_view segment =
            text.substr(start, dot == std::string_view::npos ? dot : dot - start);
        if (!valid_segment(segment)) return std::nullopt;
        ++count;
        if (dot == std::string_view::npos) break;
        start = dot + 1;
    }
    if (count < kMinSegments || count > kMaxSegments) return std::nullopt;

    return ResourceId{std::string{text}};
}

std::optional<ResourceId> ResourceId::parse(std::string_view text, ResourceKind kind) {
    auto id = parse(text);
    if (!id) return std::nullopt;
    const std::string_view prefix = identity_prefix(kind);
    if (prefix.empty()) return id;
    if (!id->view().starts_with(prefix) || id->view().size() <= prefix.size() ||
        id->view()[prefix.size()] != '.') {
        return std::nullopt;
    }
    return id;
}

std::optional<ResourceKind> ResourceId::kind() const noexcept {
    const std::size_t dot = text_.find('.');
    if (dot == std::string::npos) return std::nullopt;
    return resource_kind_from_string(std::string_view{text_}.substr(0, dot));
}

std::vector<std::string_view> ResourceId::segments() const {
    std::vector<std::string_view> parts;
    std::string_view text{text_};
    std::size_t start = 0;
    while (true) {
        const std::size_t dot = text.find('.', start);
        parts.push_back(text.substr(start, dot == std::string_view::npos ? dot : dot - start));
        if (dot == std::string_view::npos) break;
        start = dot + 1;
    }
    return parts;
}

std::optional<ResourceRef> ResourceRef::parse(std::string_view text) {
    std::string_view identity = text;
    std::string      range;
    if (const std::size_t at = text.find('@'); at != std::string_view::npos) {
        identity = text.substr(0, at);
        range    = std::string{text.substr(at + 1)};
    }
    auto id = ResourceId::parse(identity);
    if (!id) return std::nullopt;
    return ResourceRef{*id, std::move(range)};
}

std::string ResourceRef::to_string() const {
    if (range.empty()) return id.str();
    return id.str() + '@' + range;
}

bool is_reserved_namespace(const ResourceId& id) noexcept {
    const auto parts = id.segments();
    // `*.squared_pg.*` is reserved for generator-internal resources (§2.8.1).
    // The check is on the second segment because the first is the kind token.
    return parts.size() >= 2 && (parts[1] == "squared_pg" || parts[1] == "squared-pg");
}

}  // namespace squared::pg
