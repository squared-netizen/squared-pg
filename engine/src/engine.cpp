// SPDX-License-Identifier: MIT
//
// The engine instance: §2.4 in full.
//
// Three patterns meet here and it is worth naming which does what.
//
//   State           — LifecycleState plus one legality matrix (§2.4.11).
//                     Transitions are enforced in execute()/initialize()/
//                     shutdown() rather than being scattered across service
//                     entry points, so "operations must not run before
//                     initialization" is checked in one place.
//   Service Locator — instance-scoped (§2.4.3). Not global: §2.4.12 allows
//                     several engines per process.
//   Command         — the operation registry (§2.4.6). Every capability is a
//                     registered descriptor plus a handler, which is what
//                     makes the surface enumerable and the Lua binding
//                     generated rather than hand-listed (§2.6.3).

#include "squared/pg/engine.hpp"

#include "services/services.hpp"
#include "support/support.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <utility>

namespace squared::pg {

std::string_view to_string(LifecycleState state) noexcept {
    switch (state) {
        case LifecycleState::created: return "created";
        case LifecycleState::initializing: return "initializing";
        case LifecycleState::ready: return "ready";
        case LifecycleState::operating: return "operating";
        case LifecycleState::failed: return "failed";
        case LifecycleState::shutdown: return "shutdown";
        case LifecycleState::destroyed: return "destroyed";
    }
    return "destroyed";
}

// Supplied by the build from the VERSION file, which is the single source:
// CMakeLists.txt reads it, the Makefile reads it, and this is where it lands.
//
// The fallback exists so the engine still compiles when built by hand without
// the define. tools/release.fish refuses to package when the fallback and the
// file have drifted, because a stale fallback would report a version the
// manifests then resolve against.
#ifndef SQUARED_PG_VERSION
#  define SQUARED_PG_VERSION "0.1.0-alpha.1"
#endif

Version engine_version() noexcept {
    auto parsed = Version::parse(SQUARED_PG_VERSION);
    return parsed.value_or(Version{});
}

// ---------------------------------------------------------------------------

struct Engine::Impl {
    using Handler = std::function<OperationResult(Impl&, const Value&)>;

    struct Operation {
        OperationDescriptor descriptor;
        Handler             handler;
    };

    EngineConfig   config;
    LifecycleState state{LifecycleState::created};

    ServiceLocator locator;

    std::vector<Capability> capabilities;

    FilesystemService  filesystem;
    ResourceService    resources;
    // The typed services check manifest capability requirements at resolution
    // (§2.14.1), so they need the engine's capability set. `capabilities` is
    // declared above them and filled by register_capabilities() before any
    // resolution can run, so the span is never dangling and never stale.
    TemplateService    templates{resources, capabilities};
    KitService         kits{resources, capabilities};
    PackageService     packages{resources, capabilities};
    AssetService       assets{resources, capabilities};
    ValidationService  validation;
    MetadataService    metadata;
    ProjectService     project{resources};
    TransactionService transaction{filesystem, resources};

    std::vector<OperationDescriptor> descriptors;
    std::vector<Operation>           operations;
    std::vector<Diagnostic>          startup_diagnostics;

    OperationResult last_result;

    void register_capabilities();
    void register_services();
    void register_operations();

    [[nodiscard]] const Operation* find_operation(std::string_view id) const;

    /// Resolve the complete set named by a configuration, §2.7.9 phases 2–6.
    /// Never mutates anything; a failure here is a failure the user learns
    /// about while the workspace still does not exist.
    [[nodiscard]] Result<ResolvedSet> resolve_set(const Value& config, std::vector<Diagnostic>& diagnostics);

    [[nodiscard]] Result<GenerationPlan> plan_for(const Value& config, std::vector<Diagnostic>& diagnostics,
                                                  ResolvedSet& out_set);
};

// ---------------------------------------------------------------------------
// Capabilities (§2.14.1)
// ---------------------------------------------------------------------------

void Engine::Impl::register_capabilities() {
    const auto add = [&](std::string token, std::string_view version, std::string summary) {
        Capability capability;
        capability.token   = std::move(token);
        capability.version = Version::parse(version).value_or(Version{});
        capability.summary = std::move(summary);
        capabilities.push_back(std::move(capability));
    };

    add("capability.template.copy", "1.0.0", "direct materialization with no substitution");
    add("capability.template.substitute", "1.0.0", "{{ parameter }} substitution in text payloads");
    add("capability.archive.zip", "1.0.0", "read .sq cartridge archives");
    add("capability.archive.exploded", "1.0.0", "read exploded cartridge directories");
    add("capability.resource.index", "1.0.0", "manifest-identity resource index");
    add("capability.project.plan", "1.0.0", "produce a generation plan without mutation");
    add("capability.project.generate", "1.0.0", "transactional new-workspace generation");
    add("capability.transaction.rename", "1.0.0", "same-filesystem staging landed by one rename");
    add("capability.metadata.provenance", "1.0.0", "workspace metadata with a provenance table");

    std::sort(capabilities.begin(), capabilities.end(),
              [](const Capability& a, const Capability& b) { return a.token < b.token; });
}

// ---------------------------------------------------------------------------
// Services (§2.4.3)
// ---------------------------------------------------------------------------

void Engine::Impl::register_services() {
    // Registration order is dependency order, and shutdown releases in
    // reverse (§2.4.9, §2.4.10). Because every service is a member of Impl,
    // destruction order is already reverse declaration order; registering in
    // the same order keeps the two descriptions agreeing.
    locator.register_service(filesystem);
    locator.register_service(resources);
    locator.register_service(templates);
    locator.register_service(kits);
    locator.register_service(packages);
    locator.register_service(assets);
    locator.register_service(validation);
    locator.register_service(metadata);
    locator.register_service(project);
    locator.register_service(transaction);
}

// ---------------------------------------------------------------------------
// Resolution shared by plan and generate
// ---------------------------------------------------------------------------

namespace {

[[nodiscard]] std::vector<std::string> config_strings(const Value& config, std::string_view key) {
    std::vector<std::string> out;
    const Value* list = config.find(key);
    if (list == nullptr) return out;
    if (const Array* array = list->as_array()) {
        for (const Value& item : *array) {
            if (auto text = item.as_string()) out.emplace_back(*text);
        }
    }
    return out;
}

}  // namespace

Result<ResolvedSet> Engine::Impl::resolve_set(const Value& config, std::vector<Diagnostic>& diagnostics) {
    if (auto valid = validation.validate_configuration(config); !valid) {
        return Unexpected{valid.error()};
    }

    ResolvedSet set;
    set.context.platforms         = config_strings(config, "platforms");
    set.context.framework_version = std::string{config.string_or("framework", {})};

    auto template_ref = ResourceRef::parse(config.string_or("template", {}));
    if (!template_ref) {
        return fail(ErrorCategory::configuration, "configuration.identity.invalid",
                    "template reference is not a valid resource reference");
    }
    set.context.template_id = template_ref->id;

    // Phase 2: template.
    auto project_template = templates.resolve(*template_ref, set.context, diagnostics);
    if (!project_template) return Unexpected{project_template.error()};
    set.project_template = std::move(*project_template);

    // Phase 3: kits, in the order the workflow named them. §2.9.6 makes
    // application order the resolution order, so a workflow that wants a
    // particular order states it rather than discovering one.
    for (const std::string& text : config_strings(config, "kits")) {
        auto ref = ResourceRef::parse(text);
        if (!ref) {
            return fail(ErrorCategory::configuration, "configuration.identity.invalid",
                        "'" + text + "' is not a valid kit reference");
        }
        auto resolved = kits.resolve(*ref, set.context, diagnostics);
        if (!resolved) return Unexpected{resolved.error()};
        set.kits.push_back(std::move(*resolved));
    }

    // Phase 4: packages.
    for (const std::string& text : config_strings(config, "packages")) {
        auto ref = ResourceRef::parse(text);
        if (!ref) {
            return fail(ErrorCategory::configuration, "configuration.identity.invalid",
                        "'" + text + "' is not a valid package reference");
        }
        auto resolved = packages.resolve(*ref, set.context, diagnostics);
        if (!resolved) return Unexpected{resolved.error()};
        set.packages.push_back(std::move(*resolved));
    }

    // Phase 5: assets.
    for (const std::string& text : config_strings(config, "assets")) {
        auto ref = ResourceRef::parse(text);
        if (!ref) {
            return fail(ErrorCategory::configuration, "configuration.identity.invalid",
                        "'" + text + "' is not a valid asset reference");
        }
        auto resolved = assets.resolve(*ref, set.context, diagnostics);
        if (!resolved) return Unexpected{resolved.error()};
        set.assets.push_back(std::move(*resolved));
    }

    // Phase 6: cross-validate the complete set.
    //
    // The template's parameter contract is checked against the *effective*
    // parameters — declared defaults and engine built-ins included — because
    // that is the set substitution will actually see. Validating the raw
    // workflow values instead would reject a request the engine was about to
    // satisfy itself.
    std::vector<std::string> kit_ids;
    for (const ResolvedResource& kit : set.kits) kit_ids.push_back(kit.id().str());
    const Value parameters = detail::effective_parameters(
        set.project_template, config, TemplateService::working_directory(set.project_template),
        this->config.engine_version, kit_ids);
    if (auto valid = validation.validate_parameters(set.project_template, parameters); !valid) {
        return Unexpected{valid.error()};
    }
    if (auto valid = validation.cross_validate(set, config, diagnostics); !valid) {
        return Unexpected{valid.error()};
    }

    return set;
}

Result<GenerationPlan> Engine::Impl::plan_for(const Value& config, std::vector<Diagnostic>& diagnostics,
                                              ResolvedSet& out_set) {
    auto set = resolve_set(config, diagnostics);
    if (!set) return Unexpected{set.error()};
    out_set = std::move(*set);
    // Phase 7: the plan.
    return project.build_plan(out_set, config, this->config.engine_version, diagnostics);
}

// ---------------------------------------------------------------------------
// Operations (§2.4.6)
// ---------------------------------------------------------------------------

namespace {

[[nodiscard]] ParameterSpec param(std::string name, ParameterType type, bool required,
                                  std::string summary) {
    return ParameterSpec{std::move(name), type, required, std::move(summary)};
}

[[nodiscard]] OperationResult with_diagnostics(OperationResult result,
                                               const std::vector<Diagnostic>& diagnostics) {
    for (const Diagnostic& diagnostic : diagnostics) {
        if (diagnostic.severity == Severity::warning) {
            result.add_warning(diagnostic);
        } else {
            result.add_diagnostic(diagnostic);
        }
    }
    return result;
}

}  // namespace

void Engine::Impl::register_operations() {
    const auto add = [&](OperationDescriptor descriptor, Handler handler) {
        descriptors.push_back(descriptor);
        operations.push_back(Operation{std::move(descriptor), std::move(handler)});
    };

    // --- engine.* : introspection -----------------------------------------

    add(OperationDescriptor{"engine.describe",
                            "engine version, lifecycle state, registered services and capabilities",
                            {},
                            {},
                            {},
                            false,
                            {"lifecycle.invalid_state"}},
        [](Impl& self, const Value&) {
            Value data = Value::object();
            data.set("version", self.config.engine_version.to_string());
            data.set("state", std::string{to_string(self.state)});
            data.set("services", Value::strings(self.locator.names()));
            Value capabilities = Value::array();
            for (const Capability& capability : self.capabilities) capabilities.push(capability.to_value());
            data.set("capabilities", capabilities);
            data.set("resource_count", static_cast<std::int64_t>(self.resources.index().size()));
            data.set("durability", std::string{self.config.fast_durability ? "fast" : "durable"});
            return OperationResult::ok(std::move(data));
        });

    add(OperationDescriptor{"engine.operations",
                            "enumerate the operation registry",
                            {},
                            {},
                            {},
                            false,
                            {}},
        [](Impl& self, const Value&) {
            Value list = Value::array();
            for (const OperationDescriptor& descriptor : self.descriptors) list.push(descriptor.to_value());
            return OperationResult::ok(std::move(list));
        });

    // --- resource.* : the index -------------------------------------------

    add(OperationDescriptor{"resource.list",
                            "list indexed resources, optionally filtered by kind",
                            {param("kind", ParameterType::string, false,
                                   "template | kit | package | asset | plugin")},
                            {"resource"},
                            {"capability.resource.index"},
                            false,
                            {"validation.parameter.invalid"}},
        [](Impl& self, const Value& params) {
            const std::string_view kind_text = params.string_or("kind", {});
            Value list = Value::array();
            if (kind_text.empty()) {
                for (const ResourceRecord& record : self.resources.index().all()) {
                    list.push(record.to_public_value());
                }
            } else {
                auto kind = resource_kind_from_string(kind_text);
                if (!kind) {
                    return OperationResult::failed(
                        make_error(ErrorCategory::validation, "validation.parameter.invalid",
                                   "'" + std::string{kind_text} + "' is not a resource kind"));
                }
                for (const ResourceRecord* record : self.resources.index().of_kind(*kind)) {
                    list.push(record->to_public_value());
                }
            }
            Value data = Value::object();
            data.set("resources", list);
            return OperationResult::ok(std::move(data));
        });

    // --- service-layer resolution (§2.4.5) --------------------------------
    //
    // These are the service layer. project.generate is composed of exactly
    // these calls, so D-008's rule holds: no capability exists only at the
    // operation layer.

    const auto resolve_op = [&](std::string id, std::string summary, ResourceKind kind,
                                std::string service, std::string code_prefix) {
        add(OperationDescriptor{id,
                                std::move(summary),
                                {param("id", ParameterType::identifier, true, "resource reference"),
                                 param("platforms", ParameterType::array, false, "target platform set"),
                                 param("framework", ParameterType::string, false, "framework version"),
                                 param("template", ParameterType::identifier, false,
                                       "template the resource must be compatible with")},
                                {std::move(service)},
                                {"capability.resource.index"},
                                false,
                                {code_prefix + ".not_found", code_prefix + ".version.unsatisfiable"}},
            [kind](Impl& self, const Value& params) {
                auto ref = ResourceRef::parse(params.string_or("id", {}));
                if (!ref) {
                    return OperationResult::failed(
                        make_error(ErrorCategory::configuration, "configuration.identity.invalid",
                                   "'" + std::string{params.string_or("id", {})} +
                                       "' is not a valid resource reference"));
                }

                ResolutionContext context;
                context.platforms         = config_strings(params, "platforms");
                context.framework_version = std::string{params.string_or("framework", {})};
                if (auto template_id = ResourceId::parse(params.string_or("template", {}))) {
                    context.template_id = *template_id;
                }

                std::vector<Diagnostic> diagnostics;
                Result<ResolvedResource> resolved =
                    kind == ResourceKind::project_template ? self.templates.resolve(*ref, context, diagnostics)
                    : kind == ResourceKind::kit            ? self.kits.resolve(*ref, context, diagnostics)
                    : kind == ResourceKind::package        ? self.packages.resolve(*ref, context, diagnostics)
                                                           : self.assets.resolve(*ref, context, diagnostics);
                if (!resolved) {
                    return with_diagnostics(OperationResult::failed(resolved.error()), diagnostics);
                }

                Value data = resolved->record->to_public_value();
                data.set("entry_count", static_cast<std::int64_t>(resolved->cartridge->entries().size()));
                if (kind == ResourceKind::project_template) {
                    data.set("working_directory", TemplateService::working_directory(*resolved));
                    Value parameters = Value::array();
                    for (const sqcart::TemplateBody::Parameter& spec :
                         TemplateService::parameters(*resolved)) {
                        Value entry = Value::object();
                        entry.set("name", spec.name);
                        entry.set("type", spec.type);
                        entry.set("required", spec.required);
                        if (spec.description) entry.set("description", *spec.description);
                        parameters.push(std::move(entry));
                    }
                    data.set("parameters", parameters);
                }
                if (!resolved->integration_areas.empty()) {
                    data.set("integration_areas", Value::strings(resolved->integration_areas));
                }
                return with_diagnostics(OperationResult::ok(std::move(data)), diagnostics);
            });
    };

    resolve_op("template.resolve", "resolve a template by identity", ResourceKind::project_template,
               "template", "template");
    resolve_op("kit.resolve", "resolve a kit by identity", ResourceKind::kit, "kit", "kit");
    resolve_op("package.resolve", "resolve a package by identity", ResourceKind::package, "package",
               "package");
    resolve_op("asset.resolve", "resolve an asset bundle by identity", ResourceKind::asset, "asset",
               "asset");

    // --- configuration validation -----------------------------------------

    const std::vector<ParameterSpec> generation_parameters{
        param("name", ParameterType::string, true, "project name; a C identifier"),
        param("output", ParameterType::path, true, "workspace location"),
        param("template", ParameterType::identifier, true, "template reference"),
        param("kits", ParameterType::array, false, "kit references, applied in order"),
        param("packages", ParameterType::array, false, "package references"),
        param("assets", ParameterType::array, false, "asset bundle references"),
        param("platforms", ParameterType::array, false, "target platform set"),
        param("framework", ParameterType::string, false, "Squared framework version"),
        param("parameters", ParameterType::object, false, "template parameter values"),
    };

    add(OperationDescriptor{"config.validate",
                            "validate a generation configuration without resolving anything",
                            generation_parameters,
                            {"validation"},
                            {},
                            false,
                            {"configuration.input.missing", "configuration.identity.invalid",
                             "configuration.name.invalid"}},
        [](Impl& self, const Value& params) {
            if (auto valid = self.validation.validate_configuration(params); !valid) {
                return OperationResult::failed(valid.error());
            }
            return OperationResult::ok(Value{true});
        });

    add(OperationDescriptor{"project.resolve",
                            "resolve and cross-validate the complete resource set (phases 2-6)",
                            generation_parameters,
                            {"template", "kit", "package", "asset", "validation"},
                            {"capability.resource.index"},
                            false,
                            {"template.not_found", "kit.compatibility.template",
                             "kit.integration_point.conflict"}},
        [](Impl& self, const Value& params) {
            std::vector<Diagnostic> diagnostics;
            auto set = self.resolve_set(params, diagnostics);
            if (!set) return with_diagnostics(OperationResult::failed(set.error()), diagnostics);

            Value data = Value::object();
            data.set("template", set->project_template.record->to_public_value());
            Value kits = Value::array();
            for (const ResolvedResource& kit : set->kits) kits.push(kit.record->to_public_value());
            data.set("kits", kits);
            data.set("working_directory", TemplateService::working_directory(set->project_template));
            return with_diagnostics(OperationResult::ok(std::move(data)), diagnostics);
        });

    add(OperationDescriptor{"project.plan",
                            "produce the generation plan; creates nothing",
                            generation_parameters,
                            {"template", "kit", "project", "validation"},
                            {"capability.project.plan"},
                            false,
                            {"template.parameter.missing", "template.ownership.invalid"}},
        [](Impl& self, const Value& params) {
            std::vector<Diagnostic> diagnostics;
            ResolvedSet             set;
            auto plan = self.plan_for(params, diagnostics, set);
            if (!plan) return with_diagnostics(OperationResult::failed(plan.error()), diagnostics);
            return with_diagnostics(OperationResult::ok(plan->to_value()), diagnostics);
        });

    add(OperationDescriptor{"project.generate",
                            "resolve, plan and generate a new workspace inside a transaction",
                            generation_parameters,
                            {"template", "kit", "project", "validation", "transaction", "metadata",
                             "filesystem"},
                            {"capability.project.generate", "capability.transaction.rename"},
                            true,
                            {"filesystem.workspace.exists", "transaction.commit_failed",
                             "filesystem.space"}},
        [](Impl& self, const Value& params) {
            std::vector<Diagnostic> diagnostics;
            ResolvedSet             set;
            auto plan = self.plan_for(params, diagnostics, set);
            if (!plan) return with_diagnostics(OperationResult::failed(plan.error()), diagnostics);

            const Value record = self.metadata.build_record(*plan, self.config.engine_version);
            auto outcome = self.transaction.apply(*plan, set, record, self.config.fast_durability);
            if (!outcome) return with_diagnostics(OperationResult::failed(outcome.error()), diagnostics);

            Value data = Value::object();
            data.set("workspace", plan->workspace);
            data.set("working_directory", plan->working_directory);
            data.set("files_written", static_cast<std::int64_t>(outcome->files_written));
            data.set("directories_created", static_cast<std::int64_t>(outcome->directories_created));
            data.set("template", plan->template_ref.id.str());
            std::vector<std::string> kit_ids;
            for (const ResourceRef& ref : plan->kits) kit_ids.push_back(ref.id.str());
            data.set("kits", Value::strings(kit_ids));
            data.set("metadata_file", std::string{MetadataService::kMetadataFile});
            data.set("durability", std::string{self.config.fast_durability ? "fast" : "durable"});

            Value metadata = Value::object();
            metadata.set("step_count", static_cast<std::int64_t>(plan->steps.size()));
            return with_diagnostics(OperationResult::ok(std::move(data)).set_metadata(std::move(metadata)),
                                    diagnostics);
        });

    add(OperationDescriptor{"workspace.inspect",
                            "read the metadata record of an existing workspace",
                            {param("workspace", ParameterType::path, true, "workspace location")},
                            {"filesystem", "metadata"},
                            {"capability.metadata.provenance"},
                            false,
                            {"filesystem.read", "configuration.json.malformed"}},
        [](Impl& self, const Value& params) {
            const std::filesystem::path root{std::string{params.string_or("workspace", {})}};
            const std::filesystem::path file = root / std::string{MetadataService::kMetadataFile};
            auto text = self.filesystem.read_file(file);
            if (!text) return OperationResult::failed(text.error());
            auto parsed = support::json_parse(*text);
            if (!parsed) return OperationResult::failed(parsed.error());
            return OperationResult::ok(std::move(*parsed));
        });

    std::sort(descriptors.begin(), descriptors.end(),
              [](const OperationDescriptor& a, const OperationDescriptor& b) { return a.id < b.id; });
}

const Engine::Impl::Operation* Engine::Impl::find_operation(std::string_view id) const {
    for (const Operation& operation : operations) {
        if (operation.descriptor.id == id) return &operation;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Engine
// ---------------------------------------------------------------------------

Engine::Engine() : impl_(std::make_unique<Impl>()) {}

Engine::~Engine() {
    if (impl_ && impl_->state != LifecycleState::destroyed) {
        // §2.4.9: the engine never relies on process termination to release
        // resources. A host that forgets to call shutdown() still gets one.
        (void)shutdown();
    }
}

std::unique_ptr<Engine> Engine::create(EngineConfig config) {
    // §2.4.1: creation allocates and prepares. It reads no configuration
    // file, resolves no resource, and touches no filesystem.
    std::unique_ptr<Engine> engine{new Engine()};
    if (config.engine_version == Version{}) config.engine_version = engine_version();
    engine->impl_->config = std::move(config);
    engine->impl_->state  = LifecycleState::created;
    return engine;
}

OperationResult Engine::initialize() {
    Impl& self = *impl_;

    if (self.state != LifecycleState::created) {
        EngineError error = make_error(ErrorCategory::lifecycle, "lifecycle.invalid_state",
                                       "initialize is legal only from 'created'; the engine is '" +
                                           std::string{to_string(self.state)} + "'");
        error.operation   = "engine.initialize";
        self.last_result  = OperationResult::failed(std::move(error));
        return self.last_result;
    }

    self.state = LifecycleState::initializing;

    // §2.4.2 order: validate startup configuration, initialize subsystems,
    // register services, build the resource index, expose the control
    // surface, mark Ready.
    self.register_capabilities();
    self.register_services();
    self.register_operations();

    // A missing required core service must prevent Ready (§2.4.3). Checking
    // the registry rather than trusting the constructor means the check
    // survives a future refactor that moves a service out of Impl.
    static constexpr std::string_view kRequired[] = {"filesystem", "resource",   "template", "kit",
                                                     "package",    "asset",      "validation",
                                                     "metadata",   "project",    "transaction"};
    for (std::string_view required : kRequired) {
        if (self.locator.find(required) == nullptr) {
            self.state        = LifecycleState::failed;
            EngineError error = make_error(ErrorCategory::internal, "internal.service.missing",
                                           "required core service '" + std::string{required} +
                                               "' is not registered");
            error.operation   = "engine.initialize";
            self.last_result  = OperationResult::failed(std::move(error));
            return self.last_result;
        }
    }

    std::vector<Diagnostic> diagnostics;
    if (auto indexed = self.resources.build_index(self.config.resource_roots, self.config.engine_version,
                                                  self.config.strict_roots, diagnostics);
        !indexed) {
        self.state       = LifecycleState::failed;
        self.last_result = with_diagnostics(OperationResult::failed(indexed.error()), diagnostics);
        self.last_result.set_operation("engine.initialize");
        return self.last_result;
    }

    self.startup_diagnostics = diagnostics;
    self.state               = LifecycleState::ready;

    Value data = Value::object();
    data.set("state", std::string{to_string(self.state)});
    data.set("resources", static_cast<std::int64_t>(self.resources.index().size()));
    data.set("operations", static_cast<std::int64_t>(self.descriptors.size()));
    data.set("services", Value::strings(self.locator.names()));
    self.last_result = with_diagnostics(OperationResult::ok(std::move(data)), diagnostics);
    return self.last_result;
}

LifecycleState Engine::state() const noexcept { return impl_->state; }

OperationResult Engine::execute(std::string_view operation, const Value& parameters) {
    Impl& self = *impl_;

    // §2.4.6 rejection rules, in order: lifecycle first, then existence, then
    // parameters. Every one of them fails before any partial modification.
    if (self.state != LifecycleState::ready) {
        EngineError error =
            make_error(ErrorCategory::lifecycle, "lifecycle.invalid_state",
                       "operations execute only from 'ready'; the engine is '" +
                           std::string{to_string(self.state)} + "'");
        error.operation = std::string{operation};
        // Nested execution is rejected by this same check: an engine in
        // 'operating' is not 'ready' (§2.4.11).
        self.last_result = OperationResult::failed(std::move(error));
        return self.last_result;
    }

    const Impl::Operation* found = self.find_operation(operation);
    if (found == nullptr) {
        EngineError error = make_error(ErrorCategory::validation, "validation.operation.unknown",
                                       "no operation named '" + std::string{operation} + "'");
        error.operation   = std::string{operation};
        error.recoverable = true;
        std::vector<std::string> known;
        for (const OperationDescriptor& descriptor : self.descriptors) known.push_back(descriptor.id);
        Value detail = Value::object();
        detail.set("known_operations", Value::strings(known));
        error.diagnostics = std::move(detail);
        self.last_result  = OperationResult::failed(std::move(error));
        return self.last_result;
    }

    if (auto valid = validate_parameters(found->descriptor, parameters); !valid) {
        self.last_result = OperationResult::failed(valid.error());
        return self.last_result;
    }

    self.state = LifecycleState::operating;
    OperationResult result = found->handler(self, parameters);
    result.set_operation(operation);
    self.state = LifecycleState::ready;

    self.last_result = std::move(result);
    return self.last_result;
}

std::span<const OperationDescriptor> Engine::operations() const noexcept { return impl_->descriptors; }

std::span<const Capability> Engine::capabilities() const noexcept { return impl_->capabilities; }

const ResourceIndex& Engine::resources() const noexcept { return impl_->resources.index(); }

const OperationResult& Engine::last_result() const noexcept { return impl_->last_result; }

Version Engine::version() const noexcept { return impl_->config.engine_version; }

OperationResult Engine::shutdown() {
    Impl& self = *impl_;

    if (self.state == LifecycleState::destroyed) {
        EngineError error = make_error(ErrorCategory::lifecycle, "lifecycle.invalid_state",
                                       "the engine is already destroyed");
        error.operation   = "engine.shutdown";
        return OperationResult::failed(std::move(error));
    }

    // §2.4.9: shutdown is safe from any valid state, including a partially
    // initialized one. Services are members of Impl, so releasing them is
    // Impl's destruction — in reverse declaration order, which is reverse
    // dependency order.
    self.state = LifecycleState::shutdown;

    Value data = Value::object();
    data.set("released_services", Value::strings(self.locator.names()));
    OperationResult result = OperationResult::ok(std::move(data));

    self.state = LifecycleState::destroyed;
    return result;
}

}  // namespace squared::pg
