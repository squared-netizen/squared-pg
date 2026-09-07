// SPDX-License-Identifier: MIT
//
// §2.7.9's resolve-before-mutate rule is only as good as the cross-validation
// that runs at phase 6. Everything this file detects is a failure the user can
// be told about while the workspace still does not exist.

#include "services/services.hpp"

#include <algorithm>
#include <set>

namespace squared::pg {
namespace {

/// Strings out of a Value array, skipping anything that is not one.
///
/// A local copy: resolve_services.cpp has the same helper in its own
/// anonymous namespace. Two four-line functions is cheaper than a header for
/// them, and this file gained its first need for one only when format 2 moved
/// these fields out of typed bodies.
[[nodiscard]] std::vector<std::string> string_list(const Value& value) {
    std::vector<std::string> out;
    if (const Array* array = value.as_array(); array != nullptr) {
        out.reserve(array->size());
        for (const Value& item : *array) {
            if (auto s = item.as_string()) out.emplace_back(*s);
        }
    }
    return out;
}

[[nodiscard]] EngineError missing_inputs(const std::vector<std::string>& missing) {
    EngineError error = make_error(ErrorCategory::configuration, "configuration.input.missing",
                                   "the generation request is missing required inputs");
    error.recoverable = true;
    Value detail      = Value::object();
    // §2.7.2 requires each missing input to be named. Lua can then ask for
    // exactly what is absent instead of restating the whole contract.
    detail.set("missing", Value::strings(missing));
    error.diagnostics = std::move(detail);
    return error;
}

[[nodiscard]] bool identifier_like(std::string_view text) {
    if (text.empty()) return false;
    const char first = text.front();
    if (!((first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z') || first == '_')) return false;
    return std::all_of(text.begin(), text.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
    });
}

[[nodiscard]] bool java_package_like(std::string_view text) {
    if (text.empty()) return false;
    std::size_t start = 0;
    std::size_t parts = 0;
    while (true) {
        const std::size_t dot = text.find('.', start);
        const std::string_view part = text.substr(start, dot == std::string_view::npos ? dot : dot - start);
        if (!identifier_like(part)) return false;
        ++parts;
        if (dot == std::string_view::npos) break;
        start = dot + 1;
    }
    return parts >= 2;
}

[[nodiscard]] bool value_matches_parameter_type(std::string_view type, const Value& value) {
    // §2.8.7 fixes a closed vocabulary so that validation happens once, in the
    // engine, rather than being reimplemented by every template.
    if (type == "string" || type.empty()) return value.as_string().has_value();
    if (type == "identifier") {
        auto text = value.as_string();
        return text && identifier_like(*text);
    }
    if (type == "java_package") {
        auto text = value.as_string();
        return text && java_package_like(*text);
    }
    if (type == "path") {
        auto text = value.as_string();
        return text && !text->empty();
    }
    if (type == "bool" || type == "boolean") return value.as_bool().has_value();
    if (type == "integer" || type == "int") return value.as_int().has_value();
    if (type == "enum") return value.as_string().has_value();
    // An unrecognised declared type is the template's defect, not the user's;
    // accepting the value here means the failure is reported once, by
    // validate_configuration's manifest check, rather than as a confusing
    // rejection of a value that looks fine.
    return true;
}

}  // namespace

Result<void> ValidationService::validate_configuration(const Value& config) const {
    std::vector<std::string> missing;

    // §2.7.2 required inputs: project name, output location, template
    // identity, framework version constraint. The engine supplies no default
    // for any identity-bearing input and never prompts.
    const auto require = [&](std::string_view key) {
        const Value* value = config.find(key);
        if (value == nullptr || !value->as_string() || value->as_string()->empty()) {
            missing.emplace_back(key);
        }
    };
    require("name");
    require("output");
    require("template");

    if (!missing.empty()) return Unexpected{missing_inputs(missing)};

    const std::string_view name = config.string_or("name", {});
    if (!identifier_like(name)) {
        EngineError error = make_error(ErrorCategory::configuration, "configuration.name.invalid",
                                       "project name '" + std::string{name} +
                                           "' must be a C identifier: letters, digits and underscore, "
                                           "not starting with a digit");
        error.recoverable = true;
        return Unexpected{std::move(error)};
    }

    if (!ResourceRef::parse(config.string_or("template", {}))) {
        EngineError error = make_error(ErrorCategory::configuration, "configuration.identity.invalid",
                                       "template reference '" + std::string{config.string_or("template", {})} +
                                           "' is not a valid resource reference");
        error.recoverable = true;
        return Unexpected{std::move(error)};
    }

    const auto check_list = [&](std::string_view key) -> Result<void> {
        const Value* list = config.find(key);
        if (list == nullptr || list->is_null()) return {};
        const Array* array = list->as_array();
        if (array == nullptr) {
            EngineError error = make_error(ErrorCategory::configuration, "configuration.type.invalid",
                                           std::string{key} + " must be an array of resource references");
            error.recoverable = true;
            return Unexpected{std::move(error)};
        }
        for (const Value& item : *array) {
            auto text = item.as_string();
            if (!text || !ResourceRef::parse(*text)) {
                EngineError error =
                    make_error(ErrorCategory::configuration, "configuration.identity.invalid",
                               "'" + std::string{text.value_or("<non-string>")} +
                                   "' in " + std::string{key} + " is not a valid resource reference");
                error.recoverable = true;
                return Unexpected{std::move(error)};
            }
        }
        return {};
    };

    if (auto checked = check_list("kits"); !checked) return checked;
    if (auto checked = check_list("packages"); !checked) return checked;
    if (auto checked = check_list("assets"); !checked) return checked;

    const std::string_view framework = config.string_or("framework", {});
    if (!framework.empty() && !Version::parse(framework)) {
        EngineError error = make_error(ErrorCategory::framework, "framework.version.invalid",
                                       "framework version '" + std::string{framework} + "' is not valid SemVer");
        error.recoverable = true;
        return Unexpected{std::move(error)};
    }

    return {};
}

Result<void> ValidationService::validate_parameters(const ResolvedResource& project_template,
                                                    const Value& parameters) const {
    std::vector<std::string> missing;
    Value invalid = Value::array();

    for (const TemplateParameter& spec : TemplateService::parameters(project_template)) {
        const Value* supplied = parameters.find(spec.name);
        if (supplied == nullptr || supplied->is_null()) {
            if (spec.required) missing.push_back(spec.name);
            continue;
        }
        if (!value_matches_parameter_type(spec.type, *supplied)) {
            Value entry = Value::object();
            entry.set("parameter", spec.name);
            entry.set("expected", spec.type);
            if (spec.description) entry.set("description", *spec.description);
            invalid.push(std::move(entry));
        }
    }

    const bool has_invalid = invalid.as_array() != nullptr && !invalid.as_array()->empty();
    if (missing.empty() && !has_invalid) return {};

    EngineError error =
        make_error(ErrorCategory::template_,
                   missing.empty() ? "template.parameter.invalid" : "template.parameter.missing",
                   "template parameter contract not satisfied");
    error.resource    = project_template.id().str();
    error.recoverable = true;
    Value detail      = Value::object();
    detail.set("missing", Value::strings(missing));
    detail.set("invalid", invalid);
    error.diagnostics = std::move(detail);
    return Unexpected{std::move(error)};
}

Result<void> ValidationService::cross_validate(const ResolvedSet& set, const Value& config,
                                               std::vector<Diagnostic>& diagnostics) const {
    (void)config;

    // --- integration areas -------------------------------------------------
    //
    // §2.9.3: a kit must not write to a point the active template does not
    // declare. Where the template declares none, the check cannot be made and
    // the gap is reported rather than assumed safe.
    const std::vector<std::string>& declared = set.project_template.integration_areas;
    if (declared.empty()) {
        for (const ResolvedResource& kit : set.kits) {
            if (!kit.integration_areas.empty()) {
                diagnostics.push_back(
                    Diagnostic{Severity::warning,
                               "template " + set.project_template.id().str() +
                                   " declares no integration areas, so contributions from kit " +
                                   kit.id().str() + " cannot be checked against it",
                               kit.id().str(), {}});
            }
        }
    } else {
        for (const ResolvedResource& kit : set.kits) {
            for (const std::string& area : kit.integration_areas) {
                if (std::find(declared.begin(), declared.end(), area) == declared.end()) {
                    EngineError error =
                        make_error(ErrorCategory::compatibility, "kit.integration_point.undeclared",
                                   "kit " + kit.id().str() + " writes integration area '" + area +
                                       "', which template " + set.project_template.id().str() +
                                       " does not declare");
                    error.resource    = kit.id().str();
                    error.recoverable = true;
                    Value detail      = Value::object();
                    detail.set("area", area);
                    detail.set("declared", Value::strings(declared));
                    error.diagnostics = std::move(detail);
                    return Unexpected{std::move(error)};
                }
            }
        }
    }

    // --- kit / kit conflicts ----------------------------------------------
    //
    // Two kits contributing the same integration area collide unless the
    // template says the area admits many contributors. The cartridge format
    // models no arity, so the engine reads it from the template's raw JSON
    // (`integration_arity`), defaulting to `single` — the conservative
    // reading, since a false conflict is reported and a missed one corrupts.
    const Value arity_map =
        detail::manifest_extension(set.project_template.cartridge->manifest(), "integration_arity");

    std::map<std::string, std::vector<std::string>> contributors;
    for (const ResolvedResource& kit : set.kits) {
        for (const std::string& area : kit.integration_areas) {
            contributors[area].push_back(kit.id().str());
        }
    }
    for (const auto& [area, kits] : contributors) {
        if (kits.size() < 2) continue;
        const std::string_view arity = arity_map.string_or(area, "single");
        if (arity == "multi") continue;
        EngineError error = make_error(ErrorCategory::compatibility, "kit.integration_point.conflict",
                                       "integration area '" + area + "' admits one contributor but " +
                                           std::to_string(kits.size()) + " kits write it");
        error.recoverable = true;
        Value detail      = Value::object();
        detail.set("area", area);
        detail.set("contributors", Value::strings(kits));
        error.diagnostics = std::move(detail);
        return Unexpected{std::move(error)};
    }

    // --- declared incompatibilities ---------------------------------------
    //
    // §2.9.3: `conflicts_with` is honoured even when no area collides.
    std::set<std::string> present;
    for (const ResolvedResource& kit : set.kits) present.insert(kit.id().str());
    for (const ResolvedResource& kit : set.kits) {
        const Value conflicts = detail::manifest_extension(kit.cartridge->manifest(), "conflicts_with");
        if (const Array* array = conflicts.as_array()) {
            for (const Value& item : *array) {
                auto other = item.as_string();
                if (other && present.count(std::string{*other}) > 0) {
                    EngineError error = make_error(ErrorCategory::compatibility, "kit.compatibility.conflict",
                                                   "kit " + kit.id().str() +
                                                       " declares an incompatibility with " + std::string{*other});
                    error.resource    = kit.id().str();
                    error.recoverable = true;
                    return Unexpected{std::move(error)};
                }
            }
        }
    }

    // --- required packages -------------------------------------------------
    //
    // §2.7.6: a package required by a resolved kit is surfaced to Lua, never
    // injected silently. Reporting it as a diagnostic is exactly that.
    std::set<std::string> resolved_packages;
    for (const ResolvedResource& package : set.packages) resolved_packages.insert(package.id().str());
    for (const ResolvedResource& kit : set.kits) {
        const std::vector<std::string> required_packages =
            string_list(detail::manifest_extension(kit.cartridge->manifest(), "requires.packages"));
        for (const std::string& required : required_packages) {
            if (resolved_packages.count(required) == 0) {
                diagnostics.push_back(
                    Diagnostic{Severity::warning,
                               "kit " + kit.id().str() + " declares a requirement on " + required +
                                   ", which the workflow did not request",
                               kit.id().str(), {}});
            }
        }
    }

    // --- required kits -----------------------------------------------------
    {
        const std::vector<std::string> required_kits = string_list(
            detail::manifest_extension(set.project_template.cartridge->manifest(),
                                       "requires.kits.required"));
        {
            for (const std::string& required : required_kits) {
                if (present.count(required) == 0) {
                    EngineError error =
                        make_error(ErrorCategory::compatibility, "template.kit.required",
                                   "template " + set.project_template.id().str() + " requires kit " +
                                       required + ", which the workflow did not select");
                    error.resource    = set.project_template.id().str();
                    error.recoverable = true;
                    Value detail      = Value::object();
                    detail.set("required_kits", Value::strings(required_kits));
                    detail.set("selected", Value::strings(std::vector<std::string>(present.begin(),
                                                                                   present.end())));
                    error.diagnostics = std::move(detail);
                    return Unexpected{std::move(error)};
                }
            }
        }
    }

    return {};
}

}  // namespace squared::pg
