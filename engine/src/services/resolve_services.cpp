// SPDX-License-Identifier: MIT
//
// The typed resource services. Each resolves its own kind by identity and
// validates its own manifest body (D-012); the envelope was already checked
// by the Resource Service at index time.
//
// None of them substitutes an alternative when resolution fails (§2.3.5,
// §2.7.4). That is not a limitation — it is the rule that keeps generation
// policy in Lua.

#include "services/services.hpp"

#include <algorithm>

namespace squared::pg {
namespace {

[[nodiscard]] EngineError not_found(ErrorCategory category, std::string code, const ResourceRef& ref,
                                    const ResourceIndex& index, ResourceKind kind) {
    EngineError error = make_error(category, std::move(code),
                                   "no resource matching '" + ref.to_string() + "' is indexed");
    error.resource    = ref.id.str();
    error.recoverable = true;

    std::vector<std::string> available;
    for (const ResourceRecord* record : index.of_kind(kind)) {
        available.push_back(record->id.str() + '@' + record->version.to_string());
    }
    Value detail = Value::object();
    detail.set("requested", ref.to_string());
    detail.set("available", Value::strings(available));
    error.diagnostics = std::move(detail);
    return error;
}

[[nodiscard]] EngineError unsatisfiable(ErrorCategory category, std::string code, const ResourceRef& ref,
                                        const std::vector<const ResourceRecord*>& considered) {
    EngineError error = make_error(category, std::move(code),
                                   "no version of " + ref.id.str() + " satisfies '" + ref.range + "'");
    error.resource    = ref.id.str();
    error.recoverable = true;

    std::vector<std::string> versions;
    for (const ResourceRecord* record : considered) versions.push_back(record->version.to_string());
    Value detail = Value::object();
    detail.set("constraint", ref.range);
    // §2.15.3: "no compatible version found" is not actionable; the list of
    // versions that were actually considered is.
    detail.set("considered", Value::strings(versions));
    error.diagnostics = std::move(detail);
    return error;
}

/// Common resolution front half: select a record, open its payload.
[[nodiscard]] Result<ResolvedResource> select_and_open(ResourceService& resources, const ResourceRef& ref,
                                                       ResourceKind kind, ErrorCategory category,
                                                       std::string_view code_prefix) {
    const ResourceIndex& index = resources.index();

    auto range = VersionRange::parse(ref.range);
    if (!range) {
        EngineError error = make_error(category, std::string{code_prefix} + ".version.invalid",
                                       "version constraint '" + ref.range + "' is not a valid range");
        error.resource    = ref.id.str();
        return Unexpected{std::move(error)};
    }

    const auto candidates = index.versions_of(ref.id);
    if (candidates.empty()) {
        return Unexpected{not_found(category, std::string{code_prefix} + ".not_found", ref, index, kind)};
    }

    const ResourceRecord* record = index.select(ref.id, *range);
    if (record == nullptr) {
        return Unexpected{
            unsatisfiable(category, std::string{code_prefix} + ".version.unsatisfiable", ref, candidates)};
    }
    if (record->kind != kind) {
        EngineError error = make_error(category, std::string{code_prefix} + ".kind.mismatch",
                                       ref.id.str() + " is a " + std::string{to_string(record->kind)} +
                                           ", not a " + std::string{to_string(kind)});
        error.resource    = ref.id.str();
        return Unexpected{std::move(error)};
    }

    auto cartridge = resources.open(*record);
    if (!cartridge) return Unexpected{cartridge.error()};

    ResolvedResource resolved;
    resolved.record    = record;
    resolved.cartridge = *cartridge;
    return resolved;
}

/// Payload root inside a cartridge (D-030).
///
/// Templates declare it as `template.tree`. The cartridge format gives kits,
/// packages and asset bundles no equivalent field, so the convention is
/// `tree/` when the cartridge has entries under it and the cartridge root
/// otherwise. Either way SQ-INF/ is never part of the payload.
[[nodiscard]] std::string payload_prefix(const sqcart::Cartridge& cartridge, std::string_view declared) {
    if (!declared.empty()) {
        std::string prefix{declared};
        if (!prefix.empty() && prefix.back() != '/') prefix.push_back('/');
        return prefix;
    }
    for (const sqcart::EntryInfo& entry : cartridge.entries()) {
        if (entry.path.starts_with("tree/")) return "tree/";
    }
    return {};
}

[[nodiscard]] std::vector<std::string> string_list(const Value& value) {
    std::vector<std::string> out;
    if (const Array* array = value.as_array()) {
        for (const Value& item : *array) {
            if (auto s = item.as_string()) out.emplace_back(*s);
        }
    }
    return out;
}

/// Whether a declared platform list admits `wanted`. "all" is a wildcard the
/// shipped seed manifests already use.
[[nodiscard]] bool platforms_admit(const std::vector<std::string>& declared,
                                   const std::vector<std::string>& wanted) {
    if (declared.empty()) return true;
    if (std::find(declared.begin(), declared.end(), "all") != declared.end()) return true;
    if (wanted.empty()) return true;
    return std::all_of(wanted.begin(), wanted.end(), [&](const std::string& platform) {
        return std::find(declared.begin(), declared.end(), platform) != declared.end();
    });
}

}  // namespace

// ---------------------------------------------------------------------------
// TemplateService
// ---------------------------------------------------------------------------

Result<ResolvedResource> TemplateService::resolve(const ResourceRef& ref, const ResolutionContext& context,
                                                  std::vector<Diagnostic>& diagnostics) {
    auto resolved = select_and_open(resources_, ref, ResourceKind::project_template,
                                    ErrorCategory::template_, "template");
    if (!resolved) return resolved;

    const sqcart::Manifest& manifest = resolved->cartridge->manifest();
    auto body = manifest.as_template();
    if (!body) {
        EngineError error = make_error(ErrorCategory::template_, "template.manifest.invalid",
                                       "manifest declares kind template but carries no template body");
        error.resource    = ref.id.str();
        return Unexpected{std::move(error)};
    }
    const sqcart::TemplateBody& template_body = body->get();

    if (!platforms_admit(template_body.platforms, context.platforms)) {
        EngineError error = make_error(ErrorCategory::compatibility, "template.compatibility.platform",
                                       "template " + ref.id.str() + " does not support every requested platform");
        error.resource    = ref.id.str();
        error.recoverable = true;
        Value detail      = Value::object();
        detail.set("supported", Value::strings(template_body.platforms));
        detail.set("requested", Value::strings(context.platforms));
        error.diagnostics = std::move(detail);
        return Unexpected{std::move(error)};
    }

    if (template_body.framework_range && !context.framework_version.empty()) {
        auto range   = VersionRange::parse(*template_body.framework_range);
        auto version = Version::parse(context.framework_version);
        if (range && version && !range->satisfied_by(*version)) {
            EngineError error = make_error(ErrorCategory::framework, "framework.compatibility.template",
                                           "template " + ref.id.str() + " requires framework " +
                                               *template_body.framework_range + ", requested " +
                                               context.framework_version);
            error.resource    = ref.id.str();
            error.recoverable = true;
            return Unexpected{std::move(error)};
        }
    }

    if (template_body.tree.empty()) {
        diagnostics.push_back(Diagnostic{Severity::warning,
                                         "template declares no payload tree; the workspace will be empty "
                                         "apart from generated files",
                                         ref.id.str(), {}});
    }

    resolved->tree_prefix       = payload_prefix(*resolved->cartridge, template_body.tree);
    resolved->ownership         = template_body.ownership;
    resolved->integration_areas = string_list(detail::manifest_extension(manifest, "template",
                                                                         "integration_areas"));
    return resolved;
}

std::span<const sqcart::TemplateBody::Parameter> TemplateService::parameters(
    const ResolvedResource& resolved) {
    auto body = resolved.cartridge->manifest().as_template();
    if (!body) return {};
    return body->get().parameters;
}

std::string TemplateService::working_directory(const ResolvedResource& resolved) {
    // §2.7.3 requires exactly one working directory and §2.8.6 puts it in the
    // manifest. The cartridge format does not model it, so it is read from
    // the preserved raw JSON, with the reference convention `sq_app` as the
    // fallback rather than an error: a template that omits it still generates
    // something sensible.
    const Value declared =
        detail::manifest_extension(resolved.cartridge->manifest(), "template", "working_directory");
    if (auto text = declared.as_string(); text && !text->empty()) {
        return support::normalize_relative(*text);
    }
    return "sq_app";
}

// ---------------------------------------------------------------------------
// KitService
// ---------------------------------------------------------------------------

Result<ResolvedResource> KitService::resolve(const ResourceRef& ref, const ResolutionContext& context,
                                             std::vector<Diagnostic>& diagnostics) {
    auto resolved = select_and_open(resources_, ref, ResourceKind::kit, ErrorCategory::kit, "kit");
    if (!resolved) return resolved;

    const sqcart::Manifest& manifest = resolved->cartridge->manifest();
    auto body = manifest.as_kit();
    if (!body) {
        EngineError error = make_error(ErrorCategory::kit, "kit.manifest.invalid",
                                       "manifest declares kind kit but carries no kit body");
        error.resource    = ref.id.str();
        return Unexpected{std::move(error)};
    }
    const sqcart::KitBody& kit = body->get();

    // §2.7.5: template compatibility is checked at resolution, before any
    // mutation. An empty list means the kit declares no restriction.
    if (!kit.compatible_templates.empty() && !context.template_id.empty()) {
        const bool compatible = std::find(kit.compatible_templates.begin(), kit.compatible_templates.end(),
                                          context.template_id.str()) != kit.compatible_templates.end();
        if (!compatible) {
            EngineError error = make_error(ErrorCategory::compatibility, "kit.compatibility.template",
                                           "kit " + ref.id.str() + " does not support template " +
                                               context.template_id.str());
            error.resource    = ref.id.str();
            error.recoverable = true;
            Value detail      = Value::object();
            detail.set("supported_templates", Value::strings(kit.compatible_templates));
            detail.set("template", context.template_id.str());
            error.diagnostics = std::move(detail);
            return Unexpected{std::move(error)};
        }
    }

    if (!platforms_admit(kit.platforms, context.platforms)) {
        EngineError error = make_error(ErrorCategory::compatibility, "kit.compatibility.platform",
                                       "kit " + ref.id.str() + " does not support every requested platform");
        error.resource    = ref.id.str();
        error.recoverable = true;
        Value detail      = Value::object();
        detail.set("supported", Value::strings(kit.platforms));
        detail.set("requested", Value::strings(context.platforms));
        error.diagnostics = std::move(detail);
        return Unexpected{std::move(error)};
    }

    if (kit.framework_range && !context.framework_version.empty()) {
        auto range   = VersionRange::parse(*kit.framework_range);
        auto version = Version::parse(context.framework_version);
        if (range && version && !range->satisfied_by(*version)) {
            EngineError error = make_error(ErrorCategory::framework, "framework.compatibility.kit",
                                           "kit " + ref.id.str() + " requires framework " +
                                               *kit.framework_range + ", requested " +
                                               context.framework_version);
            error.resource    = ref.id.str();
            error.recoverable = true;
            return Unexpected{std::move(error)};
        }
    }

    // §2.9.4: a `system` acquisition may require host software at build time
    // but never network access at generation time. The distinction matters
    // enough to the user that it is surfaced rather than assumed.
    const Value acquisition = detail::manifest_extension(manifest, "kit", "external");
    if (const Value* mode = acquisition.find("acquisition")) {
        if (auto text = mode->as_string(); text && *text == "system") {
            diagnostics.push_back(
                Diagnostic{Severity::info,
                           "kit " + ref.id.str() + " expects " + kit.external.id +
                               " to be present on the build host; generation does not require it",
                           ref.id.str(), {}});
        }
    }

    if (kit.integration_areas.empty()) {
        diagnostics.push_back(Diagnostic{Severity::warning,
                                         "kit declares no integration areas; conflicts with other kits "
                                         "cannot be detected by declaration",
                                         ref.id.str(), {}});
    }

    resolved->tree_prefix       = payload_prefix(*resolved->cartridge, {});
    resolved->ownership         = kit.ownership;
    resolved->integration_areas = kit.integration_areas;
    return resolved;
}

// ---------------------------------------------------------------------------
// PackageService
// ---------------------------------------------------------------------------

Result<ResolvedResource> PackageService::resolve(const ResourceRef& ref, const ResolutionContext& context,
                                                 std::vector<Diagnostic>& diagnostics) {
    auto resolved =
        select_and_open(resources_, ref, ResourceKind::package, ErrorCategory::package, "package");
    if (!resolved) return resolved;

    const sqcart::Manifest& manifest = resolved->cartridge->manifest();
    auto body = manifest.as_package();
    if (!body) {
        EngineError error = make_error(ErrorCategory::package, "package.manifest.invalid",
                                       "manifest declares kind package but carries no package body");
        error.resource    = ref.id.str();
        return Unexpected{std::move(error)};
    }
    const sqcart::PackageBody& package = body->get();

    if (!platforms_admit(package.platforms, context.platforms)) {
        EngineError error =
            make_error(ErrorCategory::compatibility, "package.compatibility.platform",
                       "package " + ref.id.str() + " does not support every requested platform");
        error.resource    = ref.id.str();
        error.recoverable = true;
        return Unexpected{std::move(error)};
    }

    if (package.framework_range && !context.framework_version.empty()) {
        auto range   = VersionRange::parse(*package.framework_range);
        auto version = Version::parse(context.framework_version);
        if (range && version && !range->satisfied_by(*version)) {
            EngineError error = make_error(ErrorCategory::framework, "framework.compatibility.package",
                                           "package " + ref.id.str() + " requires framework " +
                                               *package.framework_range);
            error.resource    = ref.id.str();
            error.recoverable = true;
            return Unexpected{std::move(error)};
        }
    }

    diagnostics.push_back(Diagnostic{Severity::info,
                                     "package " + ref.id.str() + " resolved; payload materialization is "
                                     "not implemented in this engine build",
                                     ref.id.str(), {}});

    resolved->tree_prefix = payload_prefix(*resolved->cartridge, {});
    return resolved;
}

// ---------------------------------------------------------------------------
// AssetService
// ---------------------------------------------------------------------------

Result<ResolvedResource> AssetService::resolve(const ResourceRef& ref, const ResolutionContext& context,
                                               std::vector<Diagnostic>& diagnostics) {
    auto resolved = select_and_open(resources_, ref, ResourceKind::asset, ErrorCategory::asset, "asset");
    if (!resolved) return resolved;

    auto body = resolved->cartridge->manifest().as_assets();
    if (!body) {
        EngineError error = make_error(ErrorCategory::asset, "asset.manifest.invalid",
                                       "manifest declares an asset bundle but carries no asset body");
        error.resource    = ref.id.str();
        return Unexpected{std::move(error)};
    }

    for (const sqcart::AssetsBody::Entry& entry : body->get().entries) {
        if (!platforms_admit(entry.platforms, context.platforms)) {
            // §2.10.6: a skipped asset is reported as a diagnostic, never
            // passed over silently.
            diagnostics.push_back(Diagnostic{Severity::info,
                                             "asset " + entry.id + " has no target for the active platforms",
                                             entry.id, entry.path});
        }
    }

    resolved->tree_prefix = payload_prefix(*resolved->cartridge, {});
    return resolved;
}

}  // namespace squared::pg
