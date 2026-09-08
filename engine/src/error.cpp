// SPDX-License-Identifier: MIT

#include "squared/pg/error.hpp"
#include "squared/pg/operation.hpp"
#include "squared/pg/plan.hpp"
#include "squared/pg/resource.hpp"
#include "squared/pg/result.hpp"

#include <algorithm>
#include <utility>

namespace squared::pg {

// ---------------------------------------------------------------------------
// Errors and diagnostics
// ---------------------------------------------------------------------------

std::string_view to_string(ErrorCategory category) noexcept {
    switch (category) {
        case ErrorCategory::configuration: return "configuration";
        case ErrorCategory::validation: return "validation";
        case ErrorCategory::resource: return "resource";
        case ErrorCategory::template_: return "template";
        case ErrorCategory::kit: return "kit";
        case ErrorCategory::package: return "package";
        case ErrorCategory::asset: return "asset";
        case ErrorCategory::framework: return "framework";
        case ErrorCategory::filesystem: return "filesystem";
        case ErrorCategory::transaction: return "transaction";
        case ErrorCategory::compatibility: return "compatibility";
        case ErrorCategory::capability: return "capability";
        case ErrorCategory::lifecycle: return "lifecycle";
        case ErrorCategory::internal: return "internal";
    }
    return "internal";
}

std::string_view to_string(Severity severity) noexcept {
    switch (severity) {
        case Severity::info: return "info";
        case Severity::warning: return "warning";
        case Severity::error: return "error";
    }
    return "info";
}

Value EngineError::to_value() const {
    Value out = Value::object();
    out.set("category", std::string{to_string(category)});
    out.set("code", code);
    out.set("message", message);
    if (!operation.empty()) out.set("operation", operation);
    if (!resource.empty()) out.set("resource", resource);
    if (!path.empty()) out.set("path", path);
    if (!diagnostics.is_null()) out.set("diagnostics", diagnostics);
    out.set("recoverable", recoverable);
    return out;
}

EngineError make_error(ErrorCategory category, std::string code, std::string message) {
    EngineError error;
    error.category = category;
    error.code     = std::move(code);
    error.message  = std::move(message);
    return error;
}

Unexpected fail(ErrorCategory category, std::string code, std::string message) {
    return Unexpected{make_error(category, std::move(code), std::move(message))};
}

Value Diagnostic::to_value() const {
    Value out = Value::object();
    out.set("severity", std::string{to_string(severity)});
    out.set("message", message);
    if (!resource.empty()) out.set("resource", resource);
    if (!path.empty()) out.set("path", path);
    return out;
}

// ---------------------------------------------------------------------------
// OperationResult
// ---------------------------------------------------------------------------

std::string_view to_string(Status status) noexcept {
    switch (status) {
        case Status::success: return "success";
        case Status::warning: return "warning";
        case Status::failure: return "failure";
        case Status::cancelled: return "cancelled";
    }
    return "failure";
}

OperationResult OperationResult::ok(Value data) {
    OperationResult result;
    result.status_ = Status::success;
    result.data_   = std::move(data);
    return result;
}

OperationResult OperationResult::failed(EngineError error) {
    OperationResult result;
    result.status_ = Status::failure;
    result.errors_.push_back(std::move(error));
    return result;
}

OperationResult& OperationResult::set_data(Value data) {
    data_ = std::move(data);
    return *this;
}

OperationResult& OperationResult::set_metadata(Value metadata) {
    metadata_ = std::move(metadata);
    return *this;
}

OperationResult& OperationResult::add_diagnostic(Diagnostic diagnostic) {
    diagnostics_.push_back(std::move(diagnostic));
    return *this;
}

OperationResult& OperationResult::add_warning(Diagnostic warning) {
    warnings_.push_back(std::move(warning));
    // Promote success to warning, but never rewrite failure or cancellation:
    // §2.4.7 forbids hiding a failure behind a softer status.
    if (status_ == Status::success) status_ = Status::warning;
    return *this;
}

OperationResult& OperationResult::add_error(EngineError error) {
    errors_.push_back(std::move(error));
    status_ = Status::failure;
    return *this;
}

OperationResult& OperationResult::set_operation(std::string_view operation) {
    for (EngineError& error : errors_) {
        if (error.operation.empty()) error.operation = std::string{operation};
    }
    return *this;
}

OperationResult& OperationResult::set_cancelled() {
    status_ = Status::cancelled;
    return *this;
}

Value OperationResult::to_value() const {
    Value out = Value::object();
    out.set("status", std::string{to_string(status_)});
    out.set("ok", succeeded());
    out.set("data", data_);

    Value diagnostics = Value::array();
    for (const Diagnostic& d : diagnostics_) diagnostics.push(d.to_value());
    out.set("diagnostics", diagnostics);

    Value warnings = Value::array();
    for (const Diagnostic& w : warnings_) warnings.push(w.to_value());
    out.set("warnings", warnings);

    Value errors = Value::array();
    for (const EngineError& e : errors_) errors.push(e.to_value());
    out.set("errors", errors);

    // A convenience alias for the overwhelmingly common single-error case.
    // §2.5.8's example workflow reads `result.error.code`, and making that
    // work is cheaper than making every workflow index into a list.
    if (!errors_.empty()) out.set("error", errors_.front().to_value());

    if (!metadata_.is_null()) out.set("metadata", metadata_);
    return out;
}

// ---------------------------------------------------------------------------
// Plan
// ---------------------------------------------------------------------------

std::string_view to_string(OwnershipClass cls) noexcept {
    switch (cls) {
        case OwnershipClass::generated: return "generated";
        case OwnershipClass::seeded: return "seeded";
        case OwnershipClass::user: return "user";
        case OwnershipClass::metadata: return "metadata";
    }
    return "seeded";
}

OwnershipClass ownership_class_from_string(std::string_view text) noexcept {
    if (text == "generated") return OwnershipClass::generated;
    if (text == "user") return OwnershipClass::user;
    if (text == "metadata") return OwnershipClass::metadata;
    // Anything unrecognised — including the reserved "merged" and the
    // cartridge format's "shared" — lands on seeded. §2.7.10 makes the safe
    // default explicit: never overwrite.
    return OwnershipClass::seeded;
}

std::string_view to_string(GenerationPhase phase) noexcept {
    switch (phase) {
        case GenerationPhase::workspace: return "workspace";
        case GenerationPhase::template_instantiate: return "template.instantiate";
        case GenerationPhase::kit_apply: return "kit.apply";
        case GenerationPhase::package_materialize: return "package.materialize";
        case GenerationPhase::framework_integrate: return "framework.integrate";
        case GenerationPhase::asset_materialize: return "asset.materialize";
        case GenerationPhase::build_emit: return "build.emit";
        case GenerationPhase::metadata_write: return "metadata.write";
    }
    return "workspace";
}

namespace {
[[nodiscard]] std::string_view step_action_name(StepAction action) noexcept {
    switch (action) {
        case StepAction::create_directory: return "create_directory";
        case StepAction::write_file: return "write_file";
        case StepAction::copy_entry: return "copy_entry";
    }
    return "write_file";
}
}  // namespace

Value PlanStep::to_value() const {
    Value out = Value::object();
    out.set("action", std::string{step_action_name(action)});
    out.set("phase", std::string{to_string(phase)});
    out.set("path", path);
    out.set("ownership", std::string{to_string(ownership)});
    if (!origin.empty()) out.set("origin", origin.str());
    if (!origin_version.empty()) out.set("origin_version", origin_version);
    if (action != StepAction::create_directory) {
        out.set("bytes", static_cast<std::int64_t>(content.size()));
    }
    if (executable) out.set("executable", true);
    return out;
}

Value GenerationPlan::to_value() const {
    Value out = Value::object();
    out.set("project_name", project_name);
    out.set("workspace", workspace);
    out.set("working_directory", working_directory);
    out.set("template", template_ref.id.str());
    out.set("template_version", template_version);

    Value kit_list = Value::array();
    for (std::size_t i = 0; i < kits.size(); ++i) {
        Value entry = Value::object();
        entry.set("id", kits[i].id.str());
        entry.set("version", i < kit_versions.size() ? kit_versions[i] : std::string{});
        kit_list.push(std::move(entry));
    }
    out.set("kits", kit_list);

    std::vector<std::string> package_ids;
    package_ids.reserve(packages.size());
    for (const ResourceRef& ref : packages) package_ids.push_back(ref.to_string());
    out.set("packages", Value::strings(package_ids));

    std::vector<std::string> asset_ids;
    asset_ids.reserve(assets.size());
    for (const ResourceRef& ref : assets) asset_ids.push_back(ref.to_string());
    out.set("assets", Value::strings(asset_ids));

    out.set("platforms", Value::strings(platforms));
    out.set("framework_version", framework_version);

    Value step_list = Value::array();
    for (const PlanStep& step : steps) step_list.push(step.to_value());
    out.set("steps", step_list);
    out.set("step_count", static_cast<std::int64_t>(steps.size()));

    Value diagnostic_list = Value::array();
    for (const Diagnostic& d : diagnostics) diagnostic_list.push(d.to_value());
    out.set("diagnostics", diagnostic_list);

    return out;
}

// ---------------------------------------------------------------------------
// Operations
// ---------------------------------------------------------------------------

Value Capability::to_value() const {
    Value out = Value::object();
    out.set("token", token);
    out.set("version", version.to_string());
    out.set("summary", summary);
    return out;
}

std::string_view to_string(ParameterType type) noexcept {
    switch (type) {
        case ParameterType::string: return "string";
        case ParameterType::identifier: return "identifier";
        case ParameterType::path: return "path";
        case ParameterType::boolean: return "boolean";
        case ParameterType::integer: return "integer";
        case ParameterType::object: return "object";
        case ParameterType::array: return "array";
    }
    return "string";
}

Value ParameterSpec::to_value() const {
    Value out = Value::object();
    out.set("name", name);
    out.set("type", std::string{to_string(type)});
    out.set("required", required);
    out.set("summary", summary);
    return out;
}

Value OperationDescriptor::to_value() const {
    Value out = Value::object();
    out.set("id", id);
    out.set("summary", summary);
    Value params = Value::array();
    for (const ParameterSpec& spec : parameters) params.push(spec.to_value());
    out.set("parameters", params);
    out.set("required_services", Value::strings(required_services));
    out.set("required_capabilities", Value::strings(required_capabilities));
    out.set("mutates_workspace", mutates_workspace);
    out.set("error_codes", Value::strings(error_codes));
    return out;
}

namespace {

[[nodiscard]] bool type_matches(ParameterType expected, const Value& value) {
    switch (expected) {
        case ParameterType::string:
        case ParameterType::identifier:
        case ParameterType::path:
            return value.as_string().has_value();
        case ParameterType::boolean:
            return value.as_bool().has_value();
        case ParameterType::integer:
            return value.as_int().has_value();
        case ParameterType::object:
            // An empty Lua table is indistinguishable from an empty list, and
            // the binding has to guess one (§2.6.5). Accepting either shape
            // when it is empty means a workflow passing `parameters = {}` is
            // not rejected for a distinction Lua cannot express.
            return value.is_object() || (value.is_array() && value.as_array()->empty());
        case ParameterType::array:
            return value.is_array() || (value.is_object() && value.as_object()->empty());
    }
    return false;
}

}  // namespace

Result<void> validate_parameters(const OperationDescriptor& descriptor, const Value& params) {
    std::vector<std::string> missing;
    Value type_errors = Value::array();

    for (const ParameterSpec& spec : descriptor.parameters) {
        const Value* supplied = params.find(spec.name);
        if (supplied == nullptr || supplied->is_null()) {
            if (spec.required) missing.push_back(spec.name);
            continue;
        }
        if (!type_matches(spec.type, *supplied)) {
            Value entry = Value::object();
            entry.set("parameter", spec.name);
            entry.set("expected", std::string{to_string(spec.type)});
            entry.set("actual", std::string{to_string(supplied->kind())});
            type_errors.push(std::move(entry));
        }
    }

    const bool has_type_errors = type_errors.as_array() != nullptr && !type_errors.as_array()->empty();
    if (missing.empty() && !has_type_errors) return {};

    EngineError error = make_error(ErrorCategory::validation, "validation.parameter.invalid",
                                   "one or more operation parameters are missing or ill-typed");
    error.operation   = descriptor.id;
    error.recoverable = true;
    Value detail      = Value::object();
    detail.set("missing", Value::strings(missing));
    detail.set("type_errors", type_errors);
    error.diagnostics = std::move(detail);
    return Unexpected{std::move(error)};
}

// ---------------------------------------------------------------------------
// Resource index
// ---------------------------------------------------------------------------

Value ResourceRecord::to_public_value() const {
    Value out = Value::object();
    out.set("id", id.str());
    out.set("kind", std::string{to_string(kind)});
    out.set("version", version.to_string());
    if (!title.empty()) out.set("title", title);
    if (!description.empty()) out.set("description", description);
    // location is deliberately absent: §2.5.9 keeps Lua independent of
    // repository layout, and a path it can see is a path it will eventually
    // depend on.
    return out;
}

std::optional<ResourceConflict> ResourceIndex::insert(ResourceRecord record) {
    const auto clash = std::find_if(records_.begin(), records_.end(), [&](const ResourceRecord& existing) {
        return existing.id == record.id && existing.version == record.version;
    });
    if (clash != records_.end()) {
        // The first wins and the second is dropped. Discovery order is
        // deterministic (§2.8.4), so which one that is stays stable across
        // machines -- important only because an unstable answer would make
        // the conflict report itself unreproducible.
        ResourceConflict conflict{record.id, record.version, clash->location, record.location};
        conflicts_.push_back(conflict);
        return conflict;
    }
    records_.push_back(std::move(record));
    return std::nullopt;
}

const ResourceConflict* ResourceIndex::conflict_for(const ResourceId& id) const {
    const auto found = std::find_if(conflicts_.begin(), conflicts_.end(),
                                    [&](const ResourceConflict& c) { return c.id == id; });
    return found == conflicts_.end() ? nullptr : &*found;
}

std::vector<const ResourceRecord*> ResourceIndex::versions_of(const ResourceId& id) const {
    std::vector<const ResourceRecord*> found;
    for (const ResourceRecord& record : records_) {
        if (record.id == id) found.push_back(&record);
    }
    std::sort(found.begin(), found.end(), [](const ResourceRecord* a, const ResourceRecord* b) {
        return a->version > b->version;
    });
    return found;
}

const ResourceRecord* ResourceIndex::select(const ResourceId& id, const VersionRange& range) const {
    // §2.8.2 selection policy: collect, discard non-satisfying, sort
    // descending, take the highest. versions_of() has already sorted.
    for (const ResourceRecord* record : versions_of(id)) {
        if (range.satisfied_by(record->version)) return record;
    }
    return nullptr;
}

std::vector<const ResourceRecord*> ResourceIndex::of_kind(ResourceKind kind) const {
    std::vector<const ResourceRecord*> found;
    for (const ResourceRecord& record : records_) {
        if (record.kind == kind) found.push_back(&record);
    }
    std::sort(found.begin(), found.end(), [](const ResourceRecord* a, const ResourceRecord* b) {
        if (a->id == b->id) return a->version > b->version;
        return a->id < b->id;
    });
    return found;
}

}  // namespace squared::pg
