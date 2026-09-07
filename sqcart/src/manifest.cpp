// SPDX-License-Identifier: MIT
//
// Manifest envelope + kind-body parsing, format spec §5. Wraps the yyjson
// backend. No exception crosses the public API; every failure is an Error
// value with a precise ErrorCode and message.
//
// yyjson is an implementation detail: the yyjson_doc lives entirely inside
// this translation unit and vanishes before Manifest is returned, so no
// backend type escapes into a public header (FR-CON-3).

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <yyjson.h>

#include "impl.hpp"

namespace sqcart {
namespace detail_manifest {

/// Shape check for a capability token: `name` or `name@range`.
///
/// `name` is dotted lowercase, like an identifier segment chain. The range, if
/// present, is passed over -- §5.3's grammar is the consumer's to resolve, and
/// validating it here would mean sqcart owning a version resolver it has no
/// use for.
[[nodiscard]] bool valid_capability_token(std::string_view token)
{
    if (token.empty() || token.size() > 128) return false;

    const std::size_t at = token.find('@');
    const std::string_view name = token.substr(0, at);
    if (at != std::string_view::npos && at + 1 >= token.size()) return false;  // trailing '@'
    if (name.empty()) return false;

    bool segment_start = true;
    for (char c : name) {
        if (c == '.') {
            if (segment_start) return false;  // empty segment
            segment_start = true;
            continue;
        }
        const bool lower = c >= 'a' && c <= 'z';
        const bool digit = c >= '0' && c <= '9';
        if (segment_start && !lower) return false;
        if (!lower && !digit && c != '_') return false;
        segment_start = false;
    }
    return !segment_start;  // must not end on a '.'
}

/// A single identity segment: `[a-z][a-z0-9_]*`.
///
/// The shared grammar behind `kind` and consumer identities. Factored out
/// because two call sites spelling the same rule twice is two chances for
/// them to drift.
[[nodiscard]] bool valid_identity_segment(std::string_view s)
{
    if (s.empty()) return false;
    if (!(s.front() >= 'a' && s.front() <= 'z')) return false;
    for (char c : s) {
        const bool lower = c >= 'a' && c <= 'z';
        const bool digit = c >= '0' && c <= '9';
        if (!lower && !digit && c != '_') return false;
    }
    return true;
}

/// Shape check for a consumer identity (§5.6).
///
/// Dotted segments, because a consumer is usually named by one: `squared_pg`,
/// `squared_framework`. Length-capped like every other token here, since an
/// identity is a map key and a map key is attacker-controlled.
[[nodiscard]] bool valid_consumer_id(std::string_view id)
{
    if (id.empty() || id.size() > 128) return false;
    std::size_t start = 0;
    while (true) {
        const std::size_t dot = id.find('.', start);
        if (!valid_identity_segment(id.substr(start, dot - start))) return false;
        if (dot == std::string_view::npos) return true;
        start = dot + 1;
    }
}

/// Shape check for the envelope's `tree` (§5.1).
///
/// Exactly two legal shapes, and no normalisation: either "." or a relative
/// directory path ending in '/'. A bare "tree" without the separator is
/// refused rather than corrected, because accepting both spellings would put
/// two ways of writing one path into every manifest in the ecosystem, and the
/// reader would then have to canonicalise on the way out to keep them from
/// diverging in error messages and indexes.
///
/// Refuses `..` and `.` components, absolute paths, backslashes, colons and
/// control characters, matching the entry-path rules of §3.3 -- a payload root
/// that could escape the cartridge would defeat every path check downstream
/// of it. `SQ-INF/` is refused outright: the reserved directory is never
/// payload (§4), so a root pointing into it can only be a mistake.
[[nodiscard]] bool valid_tree_root(std::string_view tree)
{
    if (tree == ".") return true;
    if (tree.empty() || tree.size() > 1024) return false;
    if (tree.front() == '/') return false;
    if (tree.back() != '/') return false;

    if (tree.starts_with(kMetaDir)) return false;

    std::size_t start = 0;
    while (start < tree.size()) {
        const std::size_t slash = tree.find('/', start);
        const std::string_view segment = tree.substr(start, slash - start);
        if (segment.empty()) return false;               // "" from "a//b" or a leading '/'
        if (segment == "." || segment == "..") return false;
        for (char c : segment) {
            const auto u = static_cast<unsigned char>(c);
            if (u < 0x20 || c == '\\' || c == ':') return false;
        }
        start = slash + 1;
    }
    return true;
}

}  // namespace detail_manifest

using detail_manifest::valid_capability_token;
using detail_manifest::valid_consumer_id;
using detail_manifest::valid_tree_root;

bool valid_kind_token(std::string_view token) noexcept
{
    // One segment, not dotted. A kind is a role name; a dotted role name
    // would imply a hierarchy, and a hierarchy is something a reader would
    // then be asked to interpret.
    return token.size() <= 64 && detail_manifest::valid_identity_segment(token);
}

namespace {

// RAII guard so the yyjson document is freed on every return path.
class DocGuard {
public:
    explicit DocGuard(yyjson_doc* d) : doc_(d) {}

    ~DocGuard()
    {
        if (doc_) {
            yyjson_doc_free(doc_);
        }
    }

    DocGuard(const DocGuard&) = delete;
    DocGuard& operator=(const DocGuard&) = delete;

    yyjson_doc* get() const
    {
        return doc_;
    }

private:
    yyjson_doc* doc_;
};

Error malformed(std::string msg)
{
    return Error{ErrorCode::manifest_malformed, std::move(msg), std::nullopt, std::nullopt, false};
}

// ---- small helpers ------------------------------------------------------

bool is_obj(yyjson_val* v)
{
    return v != nullptr && yyjson_get_type(v) == YYJSON_TYPE_OBJ;
}

bool is_arr(yyjson_val* v)
{
    return v != nullptr && yyjson_get_type(v) == YYJSON_TYPE_ARR;
}

bool is_str(yyjson_val* v)
{
    return v != nullptr && yyjson_get_type(v) == YYJSON_TYPE_STR;
}

// Optional string member: returns nullopt when absent, else the value.
// A present-but-wrong-type member is malformed (caller returns false).
bool get_opt_str(yyjson_val* obj, const char* key, std::optional<std::string>& out)
{
    yyjson_val* v = yyjson_obj_get(obj, key);
    if (v == nullptr) {
        return true;
    }
    if (!is_str(v)) {
        return false;
    }
    out = std::string(yyjson_get_str(v));
    return true;
}

bool get_req_str(yyjson_val* obj, const char* key, std::string& out)
{
    yyjson_val* v = yyjson_obj_get(obj, key);
    if (!is_str(v)) {
        return false;
    }
    out = std::string(yyjson_get_str(v));
    return true;
}

// String array member. Returns false on wrong type.

// Required object member.

bool parse_authors(yyjson_val* obj, std::vector<Author>& out)
{
    yyjson_val* v = yyjson_obj_get(obj, "authors");
    if (v == nullptr) {
        return true;
    }
    if (!is_arr(v)) {
        return false;
    }
    size_t idx, max;
    yyjson_val* e;
    yyjson_arr_foreach(v, idx, max, e)
    {
        if (!is_obj(e)) {
            return false;
        }
        Author a;
        if (!get_req_str(e, "name", a.name)) {
            return false;
        }
        if (!get_opt_str(e, "email", a.email)) {
            return false;
        }
        if (!get_opt_str(e, "url", a.url)) {
            return false;
        }
        out.push_back(std::move(a));
    }
    return true;
}

// Parse the schema's nested `requires` block (definition `requiresBlock`).
//
// The first draft read these as flat siblings -- required_packages,
// required_kits, framework_range -- which the schema does not define. A
// schema-conforming manifest therefore had its dependency declarations
// silently ignored, which is the worst possible failure for a resolver
// input. FR-MAN-10 makes the schema the tie-breaker, so the schema shape is
// what is parsed. Nothing has shipped with the flat form, so no alias is
// carried.

// Carrier of parsed manifest data in public types only. parse_root() fills
// one; Manifest::parse() (a member with access to the private Impl) copies
// it in. Keeping this free of Manifest::Impl lets the parser stay a plain
// function.
struct ParsedData {
    CartridgeId id;
    std::string kind;
    std::string version;
    int format_version{kFormatVersion};
    EntryPath tree;

    std::optional<std::string> title;
    std::optional<std::string> description;
    std::optional<std::string> license;
    std::vector<std::string> requires_features;
    std::vector<Author> authors;

    /// Consumer identity -> its section, re-serialised. std::map rather than
    /// unordered_map so consumers() is sorted without a sort, and because the
    /// count is small enough that hashing buys nothing.
    std::map<std::string, std::string, std::less<>> consumers;

    std::string raw_json;
};

Result<ParsedData> parse_root(yyjson_val* root, std::string raw)
{
    if (!is_obj(root)) {
        return unexpected(malformed("manifest root must be an object"));
    }

    ParsedData i;

    if (!get_req_str(root, "id", i.id)) {
        return unexpected(malformed("manifest requires a string 'id'"));
    }

    // The token is checked for shape and kept verbatim. It is deliberately
    // not compared against a list: this library does not know the ecosystem's
    // roles, and a cartridge of a kind it has never seen is a cartridge it
    // must still be able to open, index and report.
    if (!get_req_str(root, "kind", i.kind)) {
        return unexpected(malformed("manifest requires a string 'kind'"));
    }
    if (!valid_kind_token(i.kind)) {
        return unexpected(Error{ErrorCode::kind_invalid,
                                "'kind' is not a well-formed token: " + i.kind,
                                std::nullopt, i.id, false});
    }

    if (!get_req_str(root, "version", i.version)) {
        return unexpected(malformed("manifest requires a string 'version'"));
    }

    // The format tag and version are checked here rather than in validate(),
    // which is where they used to live. validate() runs on an already-open
    // Cartridge, so a manifest from an unsupported format version was being
    // parsed to completion against this version's rules and only reported as
    // a diagnostic afterwards -- and only if the caller ran validate() at
    // all. Every field read after this point is read on the strength of the
    // manifest claiming to be a format this reader implements, so that claim
    // has to be settled first.
    std::string format_tag;
    if (!get_req_str(root, "format", format_tag)) {
        return unexpected(malformed("manifest requires a string 'format'"));
    }
    if (format_tag != kFormatTag) {
        return unexpected(Error{ErrorCode::format_unsupported,
                                "'format' is not " + std::string{kFormatTag} + ": " + format_tag,
                                std::nullopt, i.id, false});
    }

    yyjson_val* fv = yyjson_obj_get(root, "format_version");
    if (fv == nullptr) {
        return unexpected(malformed("manifest requires 'format_version'"));
    }
    if (yyjson_get_type(fv) != YYJSON_TYPE_NUM) {
        return unexpected(malformed("'format_version' must be a number"));
    }
    i.format_version = static_cast<int>(yyjson_get_uint(fv));
    if (i.format_version != kFormatVersion) {
        return unexpected(Error{ErrorCode::format_unsupported,
                                "unsupported format_version "
                                    + std::to_string(i.format_version) + "; this reader implements "
                                    + std::to_string(kFormatVersion),
                                std::nullopt, i.id, false});
    }

    // Payload root (§5.1). Required, and an envelope field rather than a kind
    // body field, so the payload can be located without knowing the kind.
    if (!get_req_str(root, "tree", i.tree)) {
        return unexpected(malformed("manifest requires a string 'tree'"));
    }
    if (!valid_tree_root(i.tree)) {
        return unexpected(malformed(
            "'tree' must be \".\" or a relative directory path ending in '/', "
            "outside SQ-INF/, with no '.' or '..' components: " + i.tree));
    }

    if (!get_opt_str(root, "title", i.title) || !get_opt_str(root, "description", i.description)
        || !get_opt_str(root, "license", i.license)) {
        return unexpected(malformed("manifest metadata field has wrong type"));
    }

    // No `engine` and no `requires_capabilities` here.
    //
    // Both moved into `consumers` (§5.6). Note what that costs and what it
    // buys: this library no longer shape-checks a capability token, so a
    // malformed one reaches the consumer instead of being caught here. That
    // is correct. The consumer owns the namespace, knows the grammar, and can
    // say which capability is missing and what provides it -- three things
    // this library could never do, having only ever been able to say that a
    // string it did not understand was shaped wrongly.

    if (!parse_authors(root, i.authors)) {
        return unexpected(malformed("manifest 'authors' is malformed"));
    }

    // Consumer sections (§5.6). Absent is legal: a cartridge addressed to
    // nobody in particular is a valid archive, and a pure asset bundle may
    // genuinely have nothing to say to a consumer.
    //
    // Each value is re-serialised and stored as text. Storing the yyjson_val
    // would be cheaper and is not possible: the document is freed before
    // Manifest is returned, which is what keeps the backend out of the public
    // header (FR-CON-3).
    if (yyjson_val* consumers = yyjson_obj_get(root, "consumers"); consumers != nullptr) {
        if (!is_obj(consumers)) {
            return unexpected(malformed("'consumers' must be an object"));
        }

        std::size_t idx = 0, max = 0;
        yyjson_val *key = nullptr, *val = nullptr;
        yyjson_obj_foreach(consumers, idx, max, key, val)
        {
            const std::string name{yyjson_get_str(key), yyjson_get_len(key)};
            if (!valid_consumer_id(name)) {
                return unexpected(malformed(
                    "'consumers' key is not a well-formed consumer identity: " + name));
            }
            // An object, not an array or a scalar. The constraint exists so a
            // section is extensible without a version bump: a consumer adding
            // a field adds a member, and one reading an unknown member
            // ignores it. A bare array would make every addition positional.
            if (!is_obj(val)) {
                return unexpected(malformed("consumer section '" + name + "' must be an object"));
            }
            if (i.consumers.find(name) != i.consumers.end()) {
                return unexpected(malformed("'consumers' repeats the identity " + name));
            }

            std::size_t len = 0;
            char* text = yyjson_val_write(val, 0, &len);
            if (text == nullptr) {
                return unexpected(malformed("consumer section '" + name
                                            + "' could not be serialised"));
            }
            i.consumers.emplace(name, std::string{text, len});
            std::free(text);
        }
    }

    i.raw_json = std::move(raw);
    return i;
}

} // namespace

// ---------------------------------------------------------------------------
// Manifest
// ---------------------------------------------------------------------------

Result<Manifest> Manifest::parse(std::string_view json, const Limits& /*limits*/)
{
    // yyjson may read in-situ; it wants a mutable buffer. Copy, parse, and
    // keep the immutable copy for raw_json().
    std::string buf(json);
    yyjson_read_err err;
    yyjson_doc* doc =
        yyjson_read_opts(buf.data(), buf.size(), YYJSON_READ_ALLOW_TRAILING_COMMAS, nullptr, &err);
    if (doc == nullptr) {
        return unexpected(
            malformed("JSON parse error: " + std::string(err.msg ? err.msg : "unknown")));
    }
    DocGuard guard(doc);
    yyjson_val* root = yyjson_doc_get_root(doc);
    auto data = parse_root(root, std::string(json));
    if (!data) {
        return unexpected(data.error());
    }
    ParsedData& d = *data;

    Manifest m;
    Manifest::Impl& i = *m.impl_;
    i.id = std::move(d.id);
    i.kind = d.kind;
    i.version = std::move(d.version);
    i.format_version = d.format_version;
    i.title = std::move(d.title);
    i.description = std::move(d.description);
    i.license = std::move(d.license);
    i.requires_features = std::move(d.requires_features);
    i.tree = std::move(d.tree);
    i.authors = std::move(d.authors);
    i.consumers = std::move(d.consumers);
    i.raw_json = std::move(d.raw_json);
    return m;
}

Manifest::Manifest() : impl_(std::make_unique<Impl>()) {}

Manifest::Manifest(Manifest&&) noexcept = default;
Manifest& Manifest::operator=(Manifest&&) noexcept = default;
Manifest::~Manifest() = default;

const CartridgeId& Manifest::id() const noexcept
{
    return impl_->id;
}

std::string_view Manifest::kind() const noexcept
{
    return impl_->kind;
}

const std::string& Manifest::version() const noexcept
{
    return impl_->version;
}

int Manifest::format_version() const noexcept
{
    return impl_->format_version;
}

const EntryPath& Manifest::tree() const noexcept
{
    return impl_->tree;
}

std::optional<std::string_view> Manifest::title() const noexcept
{
    if (impl_->title) {
        return std::string_view(*impl_->title);
    }
    return std::nullopt;
}

std::optional<std::string_view> Manifest::description() const noexcept
{
    if (impl_->description) {
        return std::string_view(*impl_->description);
    }
    return std::nullopt;
}

std::optional<std::string_view> Manifest::license() const noexcept
{
    if (impl_->license) {
        return std::string_view(*impl_->license);
    }
    return std::nullopt;
}


std::span<const std::string> Manifest::requires_features() const noexcept
{
    return impl_->requires_features;
}


std::span<const Author> Manifest::authors() const noexcept
{
    return impl_->authors;
}

std::vector<std::string_view> Manifest::consumers() const
{
    std::vector<std::string_view> out;
    out.reserve(impl_->consumers.size());
    for (const auto& [name, _] : impl_->consumers) {
        out.emplace_back(name);
    }
    return out;   // std::map iterates in key order, so this is sorted
}

std::optional<std::string_view> Manifest::consumer(std::string_view id) const noexcept
{
    // std::less<> on the map makes this a heterogeneous lookup: no temporary
    // std::string is constructed to answer a question about a string_view.
    const auto it = impl_->consumers.find(id);
    if (it == impl_->consumers.end()) {
        return std::nullopt;
    }
    return std::string_view{it->second};
}

std::string_view Manifest::raw_json() const noexcept
{
    return impl_->raw_json;
}

} // namespace sqcart
