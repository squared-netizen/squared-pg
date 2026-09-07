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
#include <span>

namespace squared::pg {
namespace {

[[nodiscard]] std::vector<std::string> string_list(const Value& value) {
    std::vector<std::string> out;
    if (const Array* array = value.as_array()) {
        for (const Value& item : *array) {
            if (auto s = item.as_string()) out.emplace_back(*s);
        }
    }
    return out;
}

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

/// Check a resource's declared capability requirements against the engine.
///
/// §2.14.1: an unsatisfied capability MUST fail at resolution, never at
/// materialization. This is the check that makes the capability registry
/// usable rather than merely enumerable -- before sqcart carried
/// `requires_capabilities`, a manifest had nowhere to state a requirement and
/// eight of the nine tokens could not be demanded by anything.
///
/// sqcart validated the token's *shape* and deliberately stopped there. Whether
/// `capability.template.substitute@^1.0` names something real, and whether this
/// build satisfies the range, is the consumer's question -- which is to say,
/// this function's.
[[nodiscard]] Result<void> check_capabilities(const sqcart::Manifest& manifest,
                                              std::span<const Capability> provided,
                                              const ResourceId& id, ErrorCategory category) {
    std::vector<std::string> unmet;

    // Also relocated into this engine's section. sqcart no longer
    // shape-checks these tokens, so a malformed one arrives here -- which is
    // right, since this is the only code that knows the grammar and can say
    // what provides a missing capability.
    for (const std::string& requirement :
         string_list(detail::manifest_extension(manifest, "requires.capabilities"))) {
        const std::size_t at = requirement.find('@');
        const std::string_view token{std::string_view{requirement}.substr(0, at)};
        const std::string_view constraint =
            at == std::string::npos ? std::string_view{} : std::string_view{requirement}.substr(at + 1);

        const auto found = std::find_if(provided.begin(), provided.end(),
                                        [&](const Capability& c) { return c.token == token; });
        if (found == provided.end()) {
            unmet.push_back(requirement);
            continue;
        }
        auto range = VersionRange::parse(constraint);
        if (!range || !range->satisfied_by(found->version)) unmet.push_back(requirement);
    }

    if (unmet.empty()) return {};

    EngineError error = make_error(ErrorCategory::capability, "capability.unsatisfied",
                                   id.str() + " requires capabilities this engine does not provide");
    error.resource    = id.str();
    error.recoverable = true;

    // §2.15.3: the list of what the engine *does* provide is what makes this
    // actionable. "Capability unsatisfied" on its own tells the author nothing
    // about whether they mistyped a token or need a newer generator.
    std::vector<std::string> available;
    for (const Capability& capability : provided) {
        available.push_back(capability.token + '@' + capability.version.to_string());
    }
    Value detail = Value::object();
    detail.set("unsatisfied", Value::strings(unmet));
    detail.set("provided", Value::strings(available));
    error.diagnostics = std::move(detail);
    (void)category;
    return Unexpected{std::move(error)};
}

/// Common resolution front half: select a record, open its payload.
[[nodiscard]] Result<ResolvedResource> select_and_open(ResourceService& resources, const ResourceRef& ref,
                                                       ResourceKind kind, ErrorCategory category,
                                                       std::string_view code_prefix,
                                                       std::span<const Capability> provided) {
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

    // Before anything else about the body is looked at. A resource this engine
    // cannot honour should be refused for that reason, not for whatever
    // secondary complaint its body happens to trigger first.
    if (auto satisfied = check_capabilities((*cartridge)->manifest(), provided, record->id, category);
        !satisfied) {
        return Unexpected{satisfied.error()};
    }

    ResolvedResource resolved;
    resolved.record    = record;
    resolved.cartridge = *cartridge;
    return resolved;
}

/// Payload root inside a cartridge (D-031).
///
/// Every kind declares it, in the manifest envelope, as of cartridge format 2.
/// This used to guess: templates carried `template.tree` and the other kinds
/// had no field at all, so the engine looked for entries under `tree/` and
/// fell back to the cartridge root. The guess was correct for every resource
/// that happened to exist and silently wrong for any kit that shipped an
/// unrelated `tree/` directory.
///
/// The manifest's value is "." for a payload at the cartridge root and a
/// trailing-slash path otherwise, so the only work left here is turning "."
/// into the empty prefix this engine uses for "no prefix". SQ-INF/ is not
/// payload either way; that is the format's rule, not this function's.
[[nodiscard]] std::string payload_prefix(const sqcart::Manifest& manifest) {
    const std::string& declared = manifest.tree();
    if (declared == ".") return {};
    return declared;
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
                                    ErrorCategory::template_, "template", capabilities_);
    if (!resolved) return resolved;

    const sqcart::Manifest& manifest = resolved->cartridge->manifest();

    // Cartridge format 2 has no kind bodies. A template that says nothing to
    // this engine is no longer a malformed manifest -- it is a cartridge
    // addressed to somebody else, and the failure now surfaces as an
    // unsatisfied requirement rather than as "carries no template body".
    if (!manifest.consumer(detail::kConsumerId)) {
        EngineError error = make_error(ErrorCategory::template_, "template.manifest.invalid",
                                       "template " + ref.id.str() +
                                           " carries no section addressed to squared-pg");
        error.resource    = ref.id.str();
        return Unexpected{std::move(error)};
    }

    const std::vector<std::string> template_platforms =
        string_list(detail::manifest_extension(manifest, "platforms"));
    const Value framework_declared = detail::manifest_extension(manifest, "requires.framework");
    const std::optional<std::string> framework_range =
        framework_declared.as_string()
            ? std::optional<std::string>{std::string{*framework_declared.as_string()}}
            : std::nullopt;

    if (!platforms_admit(template_platforms, context.platforms)) {
        EngineError error = make_error(ErrorCategory::compatibility, "template.compatibility.platform",
                                       "template " + ref.id.str() + " does not support every requested platform");
        error.resource    = ref.id.str();
        error.recoverable = true;
        Value detail      = Value::object();
        detail.set("supported", Value::strings(template_platforms));
        detail.set("requested", Value::strings(context.platforms));
        error.diagnostics = std::move(detail);
        return Unexpected{std::move(error)};
    }

    if (framework_range && !context.framework_version.empty()) {
        auto range   = VersionRange::parse(*framework_range);
        auto version = Version::parse(context.framework_version);
        if (range && version && !range->satisfied_by(*version)) {
            EngineError error = make_error(ErrorCategory::framework, "framework.compatibility.template",
                                           "template " + ref.id.str() + " requires framework " +
                                               *framework_range + ", requested " +
                                               context.framework_version);
            error.resource    = ref.id.str();
            error.recoverable = true;
            return Unexpected{std::move(error)};
        }
    }

    resolved->tree_prefix = payload_prefix(resolved->cartridge->manifest());

    // The old form of this check tested whether the template *declared* a
    // payload root. It cannot any more: `tree` is required by the format, so
    // it is never absent. What is still worth warning about is the case that
    // check was actually reaching for -- a declared root with nothing under
    // it, which yields a workspace containing only generated files.
    {
        const std::string& prefix = resolved->tree_prefix;
        const auto& entries = resolved->cartridge->entries();
        const bool populated = std::any_of(
            entries.begin(), entries.end(), [&](const sqcart::EntryInfo& entry) {
                return entry.path.starts_with(prefix)
                       && !entry.path.starts_with(sqcart::kMetaDir);
            });
        if (!populated) {
            diagnostics.push_back(Diagnostic{Severity::warning,
                                             "template payload tree is empty; the workspace will "
                                             "contain only generated files",
                                             ref.id.str(), {}});
        }
    }

    resolved->ownership         = detail::ownership_rules(manifest);
    resolved->integration_areas = string_list(detail::manifest_extension(manifest, "integration_areas"));
    return resolved;
}

std::vector<TemplateParameter> TemplateService::parameters(const ResolvedResource& resolved) {
    std::vector<TemplateParameter> out;
    const Value declared =
        detail::manifest_extension(resolved.cartridge->manifest(), "parameters");
    const Array* array = declared.as_array();
    if (array == nullptr) return out;

    for (const Value& item : *array) {
        TemplateParameter spec;
        if (const Value* v = item.find("name"); v != nullptr) {
            if (auto t = v->as_string()) spec.name = *t;
        }
        // A parameter without a name is dropped rather than reported. The
        // manifest schema is squared-pg's own and `workspace.verify` is where
        // a malformed one should be caught; failing resolution here would
        // turn a schema defect into an unexplained resolve failure.
        if (spec.name.empty()) continue;

        if (const Value* v = item.find("type"); v != nullptr) {
            if (auto t = v->as_string()) spec.type = *t;
        }
        if (const Value* v = item.find("required"); v != nullptr) {
            spec.required = v->as_bool().value_or(false);
        }
        if (const Value* v = item.find("description"); v != nullptr) {
            if (auto t = v->as_string()) spec.description = std::string{*t};
        }
        if (const Value* v = item.find("default"); v != nullptr) {
            spec.default_value = *v;
        }
        if (const Value* v = item.find("default_from"); v != nullptr) {
            if (auto t = v->as_string()) spec.default_from = std::string{*t};
        }
        out.push_back(std::move(spec));
    }
    return out;
}

std::string TemplateService::working_directory(const ResolvedResource& resolved) {
    // §2.7.3 requires exactly one working directory and §2.8.6 puts it in the
    // manifest. The cartridge format does not model it, so it is read from
    // the preserved raw JSON, with the reference convention `sq_app` as the
    // fallback rather than an error: a template that omits it still generates
    // something sensible.
    const Value declared =
        detail::manifest_extension(resolved.cartridge->manifest(), "working_directory");
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
    auto resolved = select_and_open(resources_, ref, ResourceKind::kit, ErrorCategory::kit, "kit",
                                    capabilities_);
    if (!resolved) return resolved;

    const sqcart::Manifest& manifest = resolved->cartridge->manifest();
    if (!manifest.consumer(detail::kConsumerId)) {
        EngineError error = make_error(ErrorCategory::kit, "kit.manifest.invalid",
                                       "kit " + ref.id.str() +
                                           " carries no section addressed to squared-pg");
        error.resource    = ref.id.str();
        return Unexpected{std::move(error)};
    }

    const std::vector<std::string> kit_compatible_templates =
        string_list(detail::manifest_extension(manifest, "compatible_templates"));
    const std::vector<std::string> kit_platforms =
        string_list(detail::manifest_extension(manifest, "platforms"));
    const std::vector<std::string> kit_integration_areas =
        string_list(detail::manifest_extension(manifest, "integration_areas"));
    const Value kit_framework = detail::manifest_extension(manifest, "requires.framework");
    const std::optional<std::string> kit_framework_range =
        kit_framework.as_string()
            ? std::optional<std::string>{std::string{*kit_framework.as_string()}}
            : std::nullopt;

    // §2.7.5: template compatibility is checked at resolution, before any
    // mutation. An empty list means the kit declares no restriction.
    if (!kit_compatible_templates.empty() && !context.template_id.empty()) {
        const bool compatible = std::find(kit_compatible_templates.begin(), kit_compatible_templates.end(),
                                          context.template_id.str()) != kit_compatible_templates.end();
        if (!compatible) {
            EngineError error = make_error(ErrorCategory::compatibility, "kit.compatibility.template",
                                           "kit " + ref.id.str() + " does not support template " +
                                               context.template_id.str());
            error.resource    = ref.id.str();
            error.recoverable = true;
            Value detail      = Value::object();
            detail.set("supported_templates", Value::strings(kit_compatible_templates));
            detail.set("template", context.template_id.str());
            error.diagnostics = std::move(detail);
            return Unexpected{std::move(error)};
        }
    }

    if (!platforms_admit(kit_platforms, context.platforms)) {
        EngineError error = make_error(ErrorCategory::compatibility, "kit.compatibility.platform",
                                       "kit " + ref.id.str() + " does not support every requested platform");
        error.resource    = ref.id.str();
        error.recoverable = true;
        Value detail      = Value::object();
        detail.set("supported", Value::strings(kit_platforms));
        detail.set("requested", Value::strings(context.platforms));
        error.diagnostics = std::move(detail);
        return Unexpected{std::move(error)};
    }

    if (kit_framework_range && !context.framework_version.empty()) {
        auto range   = VersionRange::parse(*kit_framework_range);
        auto version = Version::parse(context.framework_version);
        if (range && version && !range->satisfied_by(*version)) {
            EngineError error = make_error(ErrorCategory::framework, "framework.compatibility.kit",
                                           "kit " + ref.id.str() + " requires framework " +
                                               *kit_framework_range + ", requested " +
                                               context.framework_version);
            error.resource    = ref.id.str();
            error.recoverable = true;
            return Unexpected{std::move(error)};
        }
    }

    // §2.9.4: a `system` acquisition may require host software at build time
    // but never network access at generation time. The distinction matters
    // enough to the user that it is surfaced rather than assumed.
    const Value acquisition = detail::manifest_extension(manifest, "external");
    if (const Value* mode = acquisition.find("acquisition")) {
        if (auto text = mode->as_string(); text && *text == "system") {
            std::string external_id{"its external dependency"};
            if (const Value* eid = acquisition.find("id"); eid != nullptr) {
                if (auto t = eid->as_string(); t && !t->empty()) external_id = std::string{*t};
            }
            diagnostics.push_back(
                Diagnostic{Severity::info,
                           "kit " + ref.id.str() + " expects " + external_id +
                               " to be present on the build host; generation does not require it",
                           ref.id.str(), {}});
        }
    }

    if (kit_integration_areas.empty()) {
        diagnostics.push_back(Diagnostic{Severity::warning,
                                         "kit declares no integration areas; conflicts with other kits "
                                         "cannot be detected by declaration",
                                         ref.id.str(), {}});
    }

    resolved->tree_prefix       = payload_prefix(resolved->cartridge->manifest());
    resolved->ownership         = detail::ownership_rules(manifest);
    resolved->integration_areas = kit_integration_areas;
    return resolved;
}

// ---------------------------------------------------------------------------
// PackageService
// ---------------------------------------------------------------------------

Result<ResolvedResource> PackageService::resolve(const ResourceRef& ref, const ResolutionContext& context,
                                                 std::vector<Diagnostic>& diagnostics) {
    auto resolved =
        select_and_open(resources_, ref, ResourceKind::package, ErrorCategory::package,
                        "package", capabilities_);
    if (!resolved) return resolved;

    const sqcart::Manifest& manifest = resolved->cartridge->manifest();
    if (!manifest.consumer(detail::kConsumerId)) {
        EngineError error = make_error(ErrorCategory::package, "package.manifest.invalid",
                                       "package " + ref.id.str() +
                                           " carries no section addressed to squared-pg");
        error.resource    = ref.id.str();
        return Unexpected{std::move(error)};
    }

    const std::vector<std::string> package_platforms =
        string_list(detail::manifest_extension(manifest, "platforms"));
    const Value package_framework = detail::manifest_extension(manifest, "requires.framework");
    const std::optional<std::string> package_framework_range =
        package_framework.as_string()
            ? std::optional<std::string>{std::string{*package_framework.as_string()}}
            : std::nullopt;

    if (!platforms_admit(package_platforms, context.platforms)) {
        EngineError error =
            make_error(ErrorCategory::compatibility, "package.compatibility.platform",
                       "package " + ref.id.str() + " does not support every requested platform");
        error.resource    = ref.id.str();
        error.recoverable = true;
        return Unexpected{std::move(error)};
    }

    if (package_framework_range && !context.framework_version.empty()) {
        auto range   = VersionRange::parse(*package_framework_range);
        auto version = Version::parse(context.framework_version);
        if (range && version && !range->satisfied_by(*version)) {
            EngineError error = make_error(ErrorCategory::framework, "framework.compatibility.package",
                                           "package " + ref.id.str() + " requires framework " +
                                               *package_framework_range);
            error.resource    = ref.id.str();
            error.recoverable = true;
            return Unexpected{std::move(error)};
        }
    }

    diagnostics.push_back(Diagnostic{Severity::info,
                                     "package " + ref.id.str() + " resolved; payload materialization is "
                                     "not implemented in this engine build",
                                     ref.id.str(), {}});

    resolved->tree_prefix = payload_prefix(resolved->cartridge->manifest());
    return resolved;
}

// ---------------------------------------------------------------------------
// AssetService
// ---------------------------------------------------------------------------

Result<ResolvedResource> AssetService::resolve(const ResourceRef& ref, const ResolutionContext& context,
                                               std::vector<Diagnostic>& diagnostics) {
    auto resolved = select_and_open(resources_, ref, ResourceKind::asset, ErrorCategory::asset, "asset",
                                    capabilities_);
    if (!resolved) return resolved;

    const sqcart::Manifest& manifest = resolved->cartridge->manifest();
    if (!manifest.consumer(detail::kConsumerId)) {
        EngineError error = make_error(ErrorCategory::asset, "asset.manifest.invalid",
                                       "asset bundle " + ref.id.str() +
                                           " carries no section addressed to squared-pg");
        error.resource    = ref.id.str();
        return Unexpected{std::move(error)};
    }

    // §2.10.6: a skipped asset is reported as a diagnostic, never passed over
    // silently. Entries are read from the consumer section now; sqcart no
    // longer checks that a declared path is present in the payload, so
    // `workspace.verify` inherits that check (see MIGRATION.md).
    const Value entries = detail::manifest_extension(manifest, "entries");
    if (const Array* array = entries.as_array(); array != nullptr) {
        for (const Value& item : *array) {
            const Value* id_value = item.find("id");
            if (id_value == nullptr) continue;
            const std::string entry_id{id_value->as_string().value_or("")};

            std::vector<std::string> entry_platforms;
            if (const Value* p = item.find("platforms"); p != nullptr) {
                entry_platforms = string_list(*p);
            }
            if (!platforms_admit(entry_platforms, context.platforms)) {
                std::string entry_path;
                if (const Value* pv = item.find("path"); pv != nullptr) {
                    entry_path = std::string{pv->as_string().value_or("")};
                }
                diagnostics.push_back(Diagnostic{Severity::info,
                                                 "asset " + entry_id +
                                                     " has no target for the active platforms",
                                                 entry_id, entry_path});
            }
        }
    }

    resolved->tree_prefix = payload_prefix(resolved->cartridge->manifest());
    return resolved;
}

}  // namespace squared::pg
