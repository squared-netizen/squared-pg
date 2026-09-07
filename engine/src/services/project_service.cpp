// SPDX-License-Identifier: MIT
//
// The plan builder. §2.7.9 requires the engine to produce the complete,
// ordered set of operations and paths generation would produce, without
// mutating anything — and then requires executing that plan to produce the
// workspace it described.
//
// The way that guarantee is kept here is that content is resolved *now*.
// Substitution happens at plan time, so a step carries the exact bytes that
// will be written. A plan that promised to substitute later could disagree
// with what the apply phase actually did, and a dry run that can disagree
// with the real run is worse than no dry run.
//
// Pattern: Builder over a Command list. Nothing in this file touches the
// filesystem: producing a plan must work without the workspace lock (§2.15.12)
// and without the target existing.

#include "services/services.hpp"

#include <algorithm>
#include <map>
#include <set>

namespace squared::pg {
namespace {

/// Ordering of contributions. §2.7.9 fixes the phase order and §2.9.6 makes
/// kit application order the kit resolution order, which is itself
/// deterministic — so the plan is reproducible from equivalent inputs.
struct Contribution {
    const ResolvedResource* resource{nullptr};
    GenerationPhase         phase{GenerationPhase::template_instantiate};
};

[[nodiscard]] std::vector<std::string> string_list(const Value& value) {
    std::vector<std::string> out;
    if (const Array* array = value.as_array()) {
        for (const Value& item : *array) {
            if (auto s = item.as_string()) out.emplace_back(*s);
        }
    }
    return out;
}

// body_key() lived here. It mapped a ResourceKind onto the manifest member
// holding that kind's body -- "template", "kit", "package", "assets".
//
// Cartridge format 2 has one section per *consumer*, not one per kind, so
// there is nothing left to select. Every resource this engine reads addresses
// it under the same identity, and which role the resource plays is already
// known from ResourceKind without asking the manifest twice.

/// Apply the template's declared parameter defaults (§2.8.7).
///
/// A parameter may declare `default` (a literal) or `default_from` (the name
/// of another parameter). Both are pure derivations — §2.8.7 forbids a default
/// that reads the environment, because that would make generation depend on
/// ambient state the plan cannot show you.
///
/// The cartridge format does not model these fields, so they are read from the
/// manifest's preserved raw JSON alongside the declared parameters.
void apply_declared_defaults(Value& parameters, const ResolvedResource& project_template) {
    const Value declared =
        detail::manifest_extension(project_template.cartridge->manifest(), "parameters");
    const Array* entries = declared.as_array();
    if (entries == nullptr) return;

    // Two passes, so `default_from` can name a parameter that itself has a
    // literal default regardless of declaration order.
    for (int pass = 0; pass < 2; ++pass) {
        for (const Value& entry : *entries) {
            const std::string_view name = entry.string_or("name", {});
            if (name.empty()) continue;
            const Value* existing = parameters.find(name);
            if (existing != nullptr && !existing->is_null()) continue;

            if (pass == 0) {
                if (const Value* literal = entry.find("default"); literal != nullptr) {
                    parameters.set(std::string{name}, *literal);
                }
            } else if (const Value* from = entry.find("default_from"); from != nullptr) {
                if (auto source = from->as_string()) {
                    if (const Value* value = parameters.find(*source)) {
                        parameters.set(std::string{name}, *value);
                    }
                }
            }
        }
    }
}

}  // namespace

namespace detail {

Value effective_parameters(const ResolvedResource& project_template, const Value& config,
                           std::string_view working_directory, const Version& engine_version,
                           const std::vector<std::string>& kit_ids) {
    Value parameters;
    if (const Value* supplied = config.find("parameters"); supplied != nullptr && supplied->is_object()) {
        parameters = *supplied;
    } else {
        parameters = Value::object();
    }

    // Built-ins. Everything here derives from the request or from the engine,
    // never from the environment or the clock: §2.7.2 forbids ambient inputs
    // and §2.7.13 asks for byte-identical output from equivalent inputs, which
    // a timestamp would destroy.
    const auto put_if_absent = [&](std::string key, Value value) {
        const Value* existing = parameters.find(key);
        if (existing == nullptr || existing->is_null()) parameters.set(std::move(key), std::move(value));
    };

    const std::string name{config.string_or("name", {})};
    put_if_absent("project_name", name);
    put_if_absent("namespace", name);
    put_if_absent("working_directory", std::string{working_directory});
    put_if_absent("generator_version", engine_version.to_string());
    put_if_absent("template_id", project_template.id().str());
    put_if_absent("template_version", project_template.version());
    put_if_absent("framework_version", std::string{config.string_or("framework", "0.0.0")});

    std::string joined;
    for (std::size_t i = 0; i < kit_ids.size(); ++i) {
        if (i > 0) joined += ", ";
        joined += kit_ids[i];
    }
    put_if_absent("kit_list", joined.empty() ? std::string{"none"} : joined);

    // Declared defaults come *after* the built-ins, so that a template may
    // write `"default_from": "project_name"` and have it resolve. The order
    // costs nothing: every step is put-if-absent, so a value the workflow
    // supplied still wins over both, and a template default for a name the
    // engine also provides is simply redundant rather than in conflict.
    apply_declared_defaults(parameters, project_template);

    return parameters;
}

}  // namespace detail

Result<GenerationPlan> ProjectService::build_plan(const ResolvedSet& set, const Value& config,
                                                  const Version& engine_version,
                                                  std::vector<Diagnostic>& diagnostics) {
    (void)resources_;

    GenerationPlan plan;
    plan.project_name      = std::string{config.string_or("name", {})};
    plan.workspace         = std::string{config.string_or("output", {})};
    plan.working_directory = TemplateService::working_directory(set.project_template);
    plan.framework_version = std::string{config.string_or("framework", {})};
    plan.platforms         = set.context.platforms;

    plan.template_ref     = ResourceRef{set.project_template.id(), {}};
    plan.template_version = set.project_template.version();
    for (const ResolvedResource& kit : set.kits) {
        plan.kits.push_back(ResourceRef{kit.id(), {}});
        plan.kit_versions.push_back(kit.version());
    }
    for (const ResolvedResource& package : set.packages) {
        plan.packages.push_back(ResourceRef{package.id(), {}});
    }
    for (const ResolvedResource& asset : set.assets) {
        plan.assets.push_back(ResourceRef{asset.id(), {}});
    }

    std::vector<std::string> kit_ids;
    for (const ResolvedResource& kit : set.kits) kit_ids.push_back(kit.id().str());
    const Value parameters = detail::effective_parameters(set.project_template, config,
                                                          plan.working_directory, engine_version, kit_ids);

    // Contributions in §2.7.9 phase order: template first, then kits in
    // resolution order.
    std::vector<Contribution> contributions;
    contributions.push_back(Contribution{&set.project_template, GenerationPhase::template_instantiate});
    for (const ResolvedResource& kit : set.kits) {
        contributions.push_back(Contribution{&kit, GenerationPhase::kit_apply});
    }

    std::vector<PlanStep>              steps;
    std::map<std::string, std::string> claimed;  // workspace path -> originating resource
    std::set<std::string>              directories;

    const auto claim_directories = [&](const std::string& path, GenerationPhase phase) {
        for (const std::string& directory : support::ancestor_directories(path)) {
            if (directories.insert(directory).second) {
                PlanStep step;
                step.action    = StepAction::create_directory;
                step.phase     = phase;
                step.path      = directory;
                step.ownership = OwnershipClass::seeded;
                steps.push_back(std::move(step));
            }
        }
    };

    for (const Contribution& contribution : contributions) {
        const ResolvedResource& resource = *contribution.resource;

        const std::vector<std::string> executable_globs =
            string_list(detail::manifest_extension(resource.cartridge->manifest(), "executable"));
        const Value processor_value =
            detail::manifest_extension(resource.cartridge->manifest(), "processor");
        const std::string processor{processor_value.as_string().value_or("substitute")};
        if (processor != "copy" && processor != "substitute") {
            EngineError error = make_error(ErrorCategory::capability, "capability.unsatisfied",
                                           "resource " + resource.id().str() +
                                               " requests template processor '" + processor +
                                               "', which this engine does not provide");
            error.resource    = resource.id().str();
            error.recoverable = true;
            return Unexpected{std::move(error)};
        }

        for (const sqcart::EntryInfo& entry : resource.cartridge->entries()) {
            // SQ-INF/ is container metadata, never payload.
            if (entry.path.starts_with(sqcart::kMetaDir)) continue;
            if (!resource.tree_prefix.empty() && !entry.path.starts_with(resource.tree_prefix)) continue;

            std::string relative = entry.path.substr(resource.tree_prefix.size());
            if (relative.empty() || relative.back() == '/') continue;

            // A path may itself carry parameters, so that a template can emit
            // `{{project_name}}.desktop` without a special-case rule.
            auto expanded_path = detail::substitute(relative, parameters, entry.path);
            if (!expanded_path) {
                EngineError error = expanded_path.error();
                error.resource    = resource.id().str();
                return Unexpected{std::move(error)};
            }
            const std::string workspace_path = support::normalize_relative(*expanded_path);
            if (workspace_path.empty()) {
                EngineError error = make_error(ErrorCategory::validation, "validation.path.unsafe",
                                               "resource entry '" + entry.path +
                                                   "' does not name a path inside the workspace");
                error.resource    = resource.id().str();
                error.path        = entry.path;
                return Unexpected{std::move(error)};
            }

            // §2.9.3, §2.7.10: two resources writing the same path is a
            // conflict detected before any mutation, not a last-writer-wins
            // race decided by iteration order.
            if (auto existing = claimed.find(workspace_path); existing != claimed.end()) {
                EngineError error =
                    make_error(ErrorCategory::compatibility, "kit.integration_point.conflict",
                               "two resources contribute the same path: " + workspace_path);
                error.path        = workspace_path;
                error.resource    = resource.id().str();
                error.recoverable = true;
                Value detail      = Value::object();
                detail.set("first", existing->second);
                detail.set("second", resource.id().str());
                error.diagnostics = std::move(detail);
                return Unexpected{std::move(error)};
            }

            auto bytes = resource.cartridge->read(entry.path);
            if (!bytes) {
                EngineError error = make_error(ErrorCategory::resource, "resource.payload.unreadable",
                                               "could not read '" + entry.path + "': " + bytes.error().message);
                error.resource    = resource.id().str();
                error.path        = entry.path;
                return Unexpected{std::move(error)};
            }
            std::string content(reinterpret_cast<const char*>(bytes->data()), bytes->size());

            const bool textual = detail::looks_textual(content);
            if (processor == "substitute" && textual) {
                auto expanded = detail::substitute(content, parameters, entry.path);
                if (!expanded) {
                    EngineError error = expanded.error();
                    error.resource    = resource.id().str();
                    return Unexpected{std::move(error)};
                }
                content = std::move(*expanded);
            } else if (processor == "substitute" && !textual) {
                diagnostics.push_back(Diagnostic{Severity::info,
                                                 "binary payload copied without substitution",
                                                 resource.id().str(), workspace_path});
            }

            const OwnershipClass ownership =
                detail::classify_path(resource.ownership, workspace_path, &diagnostics);

            // §2.7.3 and §2.7.10: no generated-class file may land inside the
            // user working directory. This is the invariant that makes "you
            // never need to edit outside sq_app, and we never overwrite what
            // you wrote in it" a guarantee rather than a convention.
            if (ownership == OwnershipClass::generated && !plan.working_directory.empty() &&
                (workspace_path == plan.working_directory ||
                 workspace_path.starts_with(plan.working_directory + "/"))) {
                EngineError error =
                    make_error(ErrorCategory::validation, "template.ownership.invalid",
                               "generated-class path '" + workspace_path +
                                   "' falls inside the user working directory '" + plan.working_directory + "'");
                error.resource    = resource.id().str();
                error.path        = workspace_path;
                return Unexpected{std::move(error)};
            }

            claim_directories(workspace_path, contribution.phase);

            PlanStep step;
            step.action         = StepAction::write_file;
            step.phase          = contribution.phase;
            step.path           = workspace_path;
            step.ownership      = ownership;
            step.origin         = resource.id();
            step.origin_version = resource.version();
            step.source_entry   = entry.path;
            step.content        = std::move(content);
            step.executable = std::any_of(executable_globs.begin(), executable_globs.end(),
                                          [&](const std::string& pattern) {
                                              return support::glob_match(pattern, workspace_path);
                                          });

            claimed.emplace(workspace_path, resource.id().str());
            steps.push_back(std::move(step));
        }
    }

    if (claimed.empty()) {
        diagnostics.push_back(Diagnostic{Severity::warning,
                                         "the resolved resource set contributes no files", {}, {}});
    }

    // The working directory must exist even when the template ships nothing
    // inside it: §2.7.3 requires every workspace to designate one, and a
    // designated directory that is not there is a broken promise.
    if (!plan.working_directory.empty() && directories.insert(plan.working_directory).second) {
        PlanStep step;
        step.action    = StepAction::create_directory;
        step.phase     = GenerationPhase::workspace;
        step.path      = plan.working_directory;
        step.ownership = OwnershipClass::seeded;
        steps.insert(steps.begin(), std::move(step));
    }

    // Directory steps sort ahead of the writes that need them within a phase.
    // std::stable_sort keeps contribution order intact among equals, which is
    // what makes two runs over the same inputs emit the same list.
    std::stable_sort(steps.begin(), steps.end(), [](const PlanStep& a, const PlanStep& b) {
        if (a.phase != b.phase) return static_cast<int>(a.phase) < static_cast<int>(b.phase);
        const bool a_dir = a.action == StepAction::create_directory;
        const bool b_dir = b.action == StepAction::create_directory;
        if (a_dir != b_dir) return a_dir;
        return a.path < b.path;
    });

    plan.steps       = std::move(steps);
    plan.diagnostics = diagnostics;
    return plan;
}

}  // namespace squared::pg
