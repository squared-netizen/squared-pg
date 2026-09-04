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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <yyjson.h>

#include "impl.hpp"

namespace sqcart {
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
bool get_str_arr(yyjson_val* obj, const char* key, std::vector<std::string>& out)
{
    yyjson_val* v = yyjson_obj_get(obj, key);
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
        if (!is_str(e)) {
            return false;
        }
        out.emplace_back(yyjson_get_str(e));
    }
    return true;
}

// Required object member.
yyjson_val* get_req_obj(yyjson_val* obj, const char* key)
{
    yyjson_val* v = yyjson_obj_get(obj, key);
    if (is_obj(v)) {
        return v;
    }
    return nullptr;
}

bool parse_engine(yyjson_val* obj, std::optional<CompatRef>& out)
{
    yyjson_val* e = get_req_obj(obj, "engine");
    if (e == nullptr) {
        return true; // optional; absent is fine
    }
    CompatRef ref;
    std::string id, version;
    if (!get_req_str(e, "id", id)) {
        return false;
    }
    if (!get_req_str(e, "version", version)) {
        return false;
    }
    ref.id = std::move(id);
    ref.version = std::move(version);
    out = std::move(ref);
    return true;
}

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
bool parse_requires(yyjson_val* body, std::optional<std::string>& framework,
                    std::vector<std::string>& packages, std::vector<std::string>& kits,
                    std::vector<std::string>* engine_capabilities = nullptr)
{
    yyjson_val* r = yyjson_obj_get(body, "requires");
    if (r == nullptr) {
        return true;   // Absent means "requires nothing", not malformed.
    }
    if (!is_obj(r)) {
        return false;
    }
    if (!get_opt_str(r, "framework", framework)) {
        return false;
    }
    if (!get_str_arr(r, "packages", packages)) {
        return false;
    }
    if (!get_str_arr(r, "kits", kits)) {
        return false;
    }
    if (engine_capabilities != nullptr &&
        !get_str_arr(r, "engine_capabilities", *engine_capabilities)) {
        return false;
    }
    return true;
}

bool parse_ownership(yyjson_val* obj, OwnershipRules& out)
{
    yyjson_val* o = yyjson_obj_get(obj, "ownership");
    if (o == nullptr) {
        return true;
    }
    if (!is_obj(o)) {
        return false;
    }
    return get_str_arr(o, "generated", out.generated) && get_str_arr(o, "user", out.user)
           && get_str_arr(o, "shared", out.shared);
}

// ---- kind bodies ---------------------------------------------------------

bool parse_cartridge_body(yyjson_val* body, CartridgeBody& out)
{
    yyjson_val* entry = get_req_obj(body, "entry");
    if (entry == nullptr) {
        return false;
    }
    if (!get_req_str(entry, "module", out.entry.module)) {
        return false;
    }
    if (!get_req_str(entry, "type", out.entry.type)) {
        return false;
    }
    if (!get_req_str(entry, "lua_abi", out.entry.lua_abi)) {
        return false;
    }
    get_opt_str(entry, "target", out.entry.target);

    if (!parse_engine(body, out.framework)) {
        return false;
    }
    if (!get_str_arr(body, "packages", out.packages)) {
        return false;
    }
    get_opt_str(body, "orientation", out.orientation);
    if (!get_str_arr(body, "permissions", out.permissions)) {
        return false;
    }
    return true;
}

bool parse_template_body(yyjson_val* body, TemplateBody& out)
{
    if (!get_str_arr(body, "project_types", out.project_types)) {
        return false;
    }
    if (!get_str_arr(body, "platforms", out.platforms)) {
        return false;
    }
    if (!get_req_str(body, "tree", out.tree)) {
        return false;
    }
    yyjson_val* params = yyjson_obj_get(body, "parameters");
    if (params != nullptr) {
        if (!is_arr(params)) {
            return false;
        }
        size_t idx, max;
        yyjson_val* e;
        yyjson_arr_foreach(params, idx, max, e)
        {
            if (!is_obj(e)) {
                return false;
            }
            TemplateBody::Parameter p;
            if (!get_req_str(e, "name", p.name)) {
                return false;
            }
            if (!get_req_str(e, "type", p.type)) {
                return false;
            }
            yyjson_val* req = yyjson_obj_get(e, "required");
            if (req != nullptr && yyjson_get_type(req) == YYJSON_TYPE_BOOL) {
                p.required = yyjson_get_bool(req);
            }
            get_opt_str(e, "pattern", p.pattern);
            get_opt_str(e, "description", p.description);
            out.parameters.push_back(std::move(p));
        }
    }
    // Template requires block. The schema gives templates a richer `kits`
    // shape than other kinds -- {required, optional} rather than a flat list
    // -- so this cannot reuse parse_requires wholesale.
    if (yyjson_val* r = yyjson_obj_get(body, "requires"); r != nullptr) {
        if (!is_obj(r)) {
            return false;
        }
        if (!get_opt_str(r, "framework", out.framework_range)) {
            return false;
        }
        if (!get_str_arr(r, "packages", out.required_packages)) {
            return false;
        }
        if (yyjson_val* k = yyjson_obj_get(r, "kits"); k != nullptr) {
            if (!is_obj(k)) {
                return false;
            }
            if (!get_str_arr(k, "required", out.required_kits)) {
                return false;
            }
            if (!get_str_arr(k, "optional", out.optional_kits)) {
                return false;
            }
        }
    }
    if (!parse_ownership(body, out.ownership)) {
        return false;
    }
    return true;
}

bool parse_kit_body(yyjson_val* body, KitBody& out)
{
    yyjson_val* ext = get_req_obj(body, "external");
    if (ext == nullptr) {
        return false;
    }
    std::string id, version;
    if (!get_req_str(ext, "id", id) || !get_req_str(ext, "version", version)) {
        return false;
    }
    out.external.id = std::move(id);
    out.external.version = std::move(version);

    if (!get_str_arr(body, "provides", out.provides)) {
        return false;
    }
    if (!get_str_arr(body, "platforms", out.platforms)) {
        return false;
    }
    if (!get_str_arr(body, "compatible_templates", out.compatible_templates)) {
        return false;
    }
    if (!parse_requires(body, out.framework_range, out.required_packages, out.required_kits)) {
        return false;
    }
    if (!get_str_arr(body, "integration_areas", out.integration_areas)) {
        return false;
    }
    // FR-KIND-2: integration_areas is required non-empty. It is the basis of
    // the engine's pre-mutation conflict check (§2.7.4); a kit that declares
    // nothing cannot be checked against another kit at all.
    if (out.integration_areas.empty()) {
        return false;
    }
    if (!parse_ownership(body, out.ownership)) {
        return false;
    }
    return true;
}

bool parse_package_body(yyjson_val* body, PackageBody& out)
{
    if (!get_str_arr(body, "provides", out.provides)) {
        return false;
    }
    if (!get_str_arr(body, "platforms", out.platforms)) {
        return false;
    }
    {
        std::vector<std::string> ignored_kits;
        if (!parse_requires(body, out.framework_range, out.required_packages, ignored_kits)) {
            return false;
        }
    }
    if (!get_req_str(body, "build_system", out.build_system)) {
        return false;
    }
    if (!get_str_arr(body, "build_targets", out.build_targets)) {
        return false;
    }
    yyjson_val* feats = yyjson_obj_get(body, "features");
    if (feats != nullptr) {
        if (!is_obj(feats)) {
            return false;
        }
        size_t idx, max;
        yyjson_val *k, *v;
        yyjson_obj_foreach(feats, idx, max, k, v)
        {
            if (yyjson_get_type(v) != YYJSON_TYPE_BOOL) {
                return false;
            }
            out.features.emplace(yyjson_get_str(k), yyjson_get_bool(v));
        }
    }
    return true;
}

bool parse_assets_body(yyjson_val* body, AssetsBody& out)
{
    yyjson_val* entries = yyjson_obj_get(body, "entries");
    if (entries != nullptr) {
        if (!is_arr(entries)) {
            return false;
        }
        size_t idx, max;
        yyjson_val* e;
        yyjson_arr_foreach(entries, idx, max, e)
        {
            if (!is_obj(e)) {
                return false;
            }
            AssetsBody::Entry a;
            if (!get_req_str(e, "id", a.id) || !get_req_str(e, "path", a.path)
                || !get_req_str(e, "type", a.type)) {
                return false;
            }
            get_opt_str(e, "format", a.format);
            if (!get_str_arr(e, "platforms", a.platforms)) {
                return false;
            }
            out.entries.push_back(std::move(a));
        }
    }
    {
        std::optional<std::string> ignored_framework;
        std::vector<std::string>   ignored_kits;
        if (!parse_requires(body, ignored_framework, out.required_packages, ignored_kits)) {
            return false;
        }
    }
    return true;
}

bool parse_plugin_body(yyjson_val* body, PluginBody& out)
{
    if (!get_req_str(body, "extends", out.extends)) {
        return false;
    }
    yyjson_val* av = yyjson_obj_get(body, "api_version");
    if (av != nullptr && yyjson_get_type(av) == YYJSON_TYPE_NUM) {
        out.api_version = static_cast<int>(yyjson_get_uint(av));
    }
    if (!get_req_str(body, "entry", out.entry)) {
        return false;
    }
    if (!get_str_arr(body, "provides_services", out.provides_services)) {
        return false;
    }
    if (!get_str_arr(body, "required_engine_capabilities", out.required_engine_capabilities)) {
        return false;
    }
    return true;
}

// Carrier of parsed manifest data in public types only. parse_root() fills
// one; Manifest::parse() (a member with access to the private Impl) copies
// it in. Keeping this free of Manifest::Impl lets the parser stay a plain
// function.
struct ParsedData {
    CartridgeId id;
    Kind kind{Kind::cartridge};
    std::string version;
    int format_version{1};

    std::optional<std::string> title;
    std::optional<std::string> description;
    std::optional<std::string> license;
    std::optional<CompatRef> engine;
    std::vector<std::string> requires_features;
    std::vector<Author> authors;

    std::unique_ptr<CartridgeBody> cartridge;
    std::unique_ptr<TemplateBody> template_;
    std::unique_ptr<KitBody> kit;
    std::unique_ptr<PackageBody> package;
    std::unique_ptr<AssetsBody> assets;
    std::unique_ptr<PluginBody> plugin;

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

    yyjson_val* kind_val = yyjson_obj_get(root, "kind");
    if (!is_str(kind_val)) {
        return unexpected(malformed("manifest requires a string 'kind'"));
    }
    auto kind = kind_from_string(yyjson_get_str(kind_val));
    if (!kind) {
        return unexpected(
            Error{ErrorCode::kind_invalid, "unknown manifest kind", std::nullopt, i.id, false});
    }
    i.kind = *kind;

    if (!get_req_str(root, "version", i.version)) {
        return unexpected(malformed("manifest requires a string 'version'"));
    }

    yyjson_val* fv = yyjson_obj_get(root, "format_version");
    if (fv != nullptr) {
        if (yyjson_get_type(fv) != YYJSON_TYPE_NUM) {
            return unexpected(malformed("'format_version' must be a number"));
        }
        i.format_version = static_cast<int>(yyjson_get_uint(fv));
    }

    if (!get_opt_str(root, "title", i.title) || !get_opt_str(root, "description", i.description)
        || !get_opt_str(root, "license", i.license)) {
        return unexpected(malformed("manifest metadata field has wrong type"));
    }

    if (!parse_engine(root, i.engine)) {
        return unexpected(malformed("manifest 'engine' is malformed"));
    }
    if (!get_str_arr(root, "requires_features", i.requires_features)) {
        return unexpected(malformed("'requires_features' must be a string array"));
    }
    if (!parse_authors(root, i.authors)) {
        return unexpected(malformed("'authors' is malformed"));
    }

    // Kind body is the member matching kind(). Foreign kind bodies present
    // alongside the declared kind are a kind_invalid violation.
    yyjson_val* body = nullptr;
    switch (i.kind) {
        case Kind::cartridge:
            body = yyjson_obj_get(root, "cartridge");
            break;
        case Kind::project_template:
            body = yyjson_obj_get(root, "template");
            break;
        case Kind::kit:
            body = yyjson_obj_get(root, "kit");
            break;
        case Kind::package:
            body = yyjson_obj_get(root, "package");
            break;
        case Kind::asset_bundle:
            body = yyjson_obj_get(root, "assets");
            break;
        case Kind::plugin:
            body = yyjson_obj_get(root, "plugin");
            break;
    }
    if (body == nullptr) {
        return unexpected(Error{ErrorCode::kind_invalid,
                                "manifest declares kind but has no matching body", std::nullopt,
                                i.id, false});
    }

    bool ok = false;
    switch (i.kind) {
        case Kind::cartridge: {
            i.cartridge = std::make_unique<CartridgeBody>();
            ok = parse_cartridge_body(body, *i.cartridge);
            break;
        }
        case Kind::project_template: {
            i.template_ = std::make_unique<TemplateBody>();
            ok = parse_template_body(body, *i.template_);
            break;
        }
        case Kind::kit: {
            i.kit = std::make_unique<KitBody>();
            ok = parse_kit_body(body, *i.kit);
            break;
        }
        case Kind::package: {
            i.package = std::make_unique<PackageBody>();
            ok = parse_package_body(body, *i.package);
            break;
        }
        case Kind::asset_bundle: {
            i.assets = std::make_unique<AssetsBody>();
            ok = parse_assets_body(body, *i.assets);
            break;
        }
        case Kind::plugin: {
            i.plugin = std::make_unique<PluginBody>();
            ok = parse_plugin_body(body, *i.plugin);
            break;
        }
    }
    if (!ok) {
        return unexpected(malformed("kind body is malformed"));
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
    i.engine = std::move(d.engine);
    i.requires_features = std::move(d.requires_features);
    i.authors = std::move(d.authors);
    i.cartridge = std::move(d.cartridge);
    i.template_ = std::move(d.template_);
    i.kit = std::move(d.kit);
    i.package = std::move(d.package);
    i.assets = std::move(d.assets);
    i.plugin = std::move(d.plugin);
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

Kind Manifest::kind() const noexcept
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

const std::optional<CompatRef>& Manifest::engine() const noexcept
{
    return impl_->engine;
}

std::span<const std::string> Manifest::requires_features() const noexcept
{
    return impl_->requires_features;
}

std::span<const Author> Manifest::authors() const noexcept
{
    return impl_->authors;
}

std::optional<std::reference_wrapper<const CartridgeBody>> Manifest::as_cartridge() const noexcept
{
    if (impl_->cartridge) {
        return std::cref(*impl_->cartridge);
    }
    return std::nullopt;
}

std::optional<std::reference_wrapper<const TemplateBody>> Manifest::as_template() const noexcept
{
    if (impl_->template_) {
        return std::cref(*impl_->template_);
    }
    return std::nullopt;
}

std::optional<std::reference_wrapper<const KitBody>> Manifest::as_kit() const noexcept
{
    if (impl_->kit) {
        return std::cref(*impl_->kit);
    }
    return std::nullopt;
}

std::optional<std::reference_wrapper<const PackageBody>> Manifest::as_package() const noexcept
{
    if (impl_->package) {
        return std::cref(*impl_->package);
    }
    return std::nullopt;
}

std::optional<std::reference_wrapper<const AssetsBody>> Manifest::as_assets() const noexcept
{
    if (impl_->assets) {
        return std::cref(*impl_->assets);
    }
    return std::nullopt;
}

std::optional<std::reference_wrapper<const PluginBody>> Manifest::as_plugin() const noexcept
{
    if (impl_->plugin) {
        return std::cref(*impl_->plugin);
    }
    return std::nullopt;
}

std::string_view Manifest::raw_json() const noexcept
{
    return impl_->raw_json;
}

} // namespace sqcart
