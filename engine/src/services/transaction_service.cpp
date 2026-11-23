// SPDX-License-Identifier: MIT
//
// Metadata (§2.7.3, §2.7.10) and the transaction (§2.15.8–2.15.11).
//
// The staging rule is the load-bearing part. D-024 resolved Q-06 by requiring
// staging to occur on the *target* filesystem so cross-device rename cannot
// arise — the failure mode that makes /tmp staging unusable on Termux, where
// /tmp is frequently a different filesystem and rename(2) returns EXDEV. The
// workspace is therefore built in a sibling directory of the target and landed
// with a single rename.

#include "services/services.hpp"

#include <algorithm>
#include <chrono>
#include <system_error>

namespace squared::pg {

// ---------------------------------------------------------------------------
// MetadataService
// ---------------------------------------------------------------------------

Value MetadataService::build_record(const GenerationPlan& plan, const Version& engine_version) const {
    Value record = Value::object();
    record.set("record_version", static_cast<std::int64_t>(kRecordVersion));
    record.set("generator", "squared-pg");
    record.set("generator_version", engine_version.to_string());

    Value project = Value::object();
    project.set("name", plan.project_name);
    // §2.7.1: identity must not derive from the workspace's filesystem
    // location, so the absolute path that produced it is not recorded here.
    project.set("working_directory", plan.working_directory);
    project.set("platforms", Value::strings(plan.platforms));
    project.set("framework_version", plan.framework_version);
    record.set("project", project);

    Value resources = Value::object();
    Value template_entry = Value::object();
    template_entry.set("id", plan.template_ref.id.str());
    template_entry.set("version", plan.template_version);
    resources.set("template", template_entry);

    Value kits = Value::array();
    for (std::size_t i = 0; i < plan.kits.size(); ++i) {
        Value entry = Value::object();
        entry.set("id", plan.kits[i].id.str());
        entry.set("version", i < plan.kit_versions.size() ? plan.kit_versions[i] : std::string{});
        kits.push(std::move(entry));
    }
    resources.set("kits", kits);

    // Packages are recorded exactly like kits — an identity with a version —
    // so a later update can attribute a path to its package and tell which
    // version it came from (D-079).
    Value packages = Value::array();
    for (std::size_t i = 0; i < plan.packages.size(); ++i) {
        Value entry = Value::object();
        entry.set("id", plan.packages[i].id.str());
        entry.set("version", i < plan.package_versions.size() ? plan.package_versions[i] : std::string{});
        packages.push(std::move(entry));
    }
    resources.set("packages", packages);

    std::vector<std::string> assets;
    for (const ResourceRef& ref : plan.assets) assets.push_back(ref.to_string());
    resources.set("assets", Value::strings(assets));
    record.set("resources", resources);

    Value layers = Value::object();
    layers.set("application", Value::strings({plan.working_directory}));
    record.set("layers", layers);

    // Provenance is filled in by the transaction, which is the only component
    // that knows what was actually written and what its bytes hashed to.
    record.set("provenance", Value::array());
    record.set("parameters", plan.parameters.is_object() ? plan.parameters : Value::object());
    record.set("roles", plan.roles.is_object() ? plan.roles : Value::object());
    return record;
}

namespace {

/// A workspace's identity (sqcart 2.0 5.2: reverse-DNS): the template's
/// package_name when it gives one, else local.<project name>.
std::string workspace_id(const GenerationPlan& plan) {
    if (const Value* given = plan.parameters.find("package_name")) {
        if (auto text = given->as_string(); text && text->find('.') != std::string_view::npos) {
            return std::string{*text};
        }
    }
    std::string id = "local.";
    for (const char c : plan.project_name) {
        const bool keep = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
        id += keep ? c : (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : '_';
    }
    return id;
}

Value versioned(const std::string& id, const std::string& version) {
    return Value{id + "@" + version};
}

}  // namespace

Value MetadataService::workspace_manifest(const GenerationPlan& plan, const Value& record) {
    Value made_from = Value::object();
    made_from.set("generator", "squared-pg@" + std::string{record.string_or("generator_version", {})});
    made_from.set("template", versioned(plan.template_ref.id.str(), plan.template_version));
    Value kits = Value::array();
    for (std::size_t i = 0; i < plan.kits.size(); ++i) {
        kits.push(versioned(plan.kits[i].id.str(), i < plan.kit_versions.size() ? plan.kit_versions[i] : std::string{}));
    }
    made_from.set("kits", kits);
    Value packages = Value::array();
    for (std::size_t i = 0; i < plan.packages.size(); ++i) {
        packages.push(versioned(plan.packages[i].id.str(),
                                i < plan.package_versions.size() ? plan.package_versions[i] : std::string{}));
    }
    made_from.set("packages", packages);
    made_from.set("framework", plan.framework_version);

    Value square = Value::object();
    square.set("made_from", made_from);
    square.set("roles", plan.roles.is_object() ? plan.roles : Value::object());
    Value generator = Value::object();
    generator.set("parameters", plan.parameters.is_object() ? plan.parameters : Value::object());
    generator.set("working_directory", plan.working_directory);
    Value consumers = Value::object();
    consumers.set("square", square);
    consumers.set(std::string{detail::kConsumerId}, generator);

    Value manifest = Value::object();
    manifest.set("format", "squared-cartridge");
    manifest.set("format_version", static_cast<std::int64_t>(3));
    manifest.set("kind", "workspace");
    manifest.set("id", workspace_id(plan));
    manifest.set("version", "0.1.0");
    manifest.set("title", plan.project_name);
    manifest.set("consumers", consumers);
    return manifest;
}

Value MetadataService::workspace_files(const Value& record) {
    Value files = Value::object();
    if (const Value* provenance = record.find("provenance")) {
        if (const Array* entries = provenance->as_array()) {
            for (const Value& entry : *entries) {
                Value file = Value::object();
                file.set("class", std::string{entry.string_or("ownership", {})});
                file.set("origin", std::string{entry.string_or("origin", {})});
                file.set("sha256", std::string{entry.string_or("sha256", {})});
                files.set(std::string{entry.string_or("path", {})}, file);
            }
        }
    }
    return files;
}

// ---------------------------------------------------------------------------
// TransactionService
// ---------------------------------------------------------------------------

namespace {

/// A staging directory beside the target, on the same filesystem by
/// construction. The leading dot keeps it out of casual listings; the suffix
/// makes an abandoned one identifiable if a process is killed between the
/// last write and the rename.
[[nodiscard]] std::filesystem::path staging_path_for(const std::filesystem::path& target) {
    const std::filesystem::path parent = target.has_parent_path() ? target.parent_path() : ".";
    return parent / ("." + target.filename().string() + ".squared-tmp");
}

}  // namespace

Result<TransactionService::Outcome> TransactionService::apply(const GenerationPlan& plan,
                                                              const ResolvedSet& set,
                                                              const Value& metadata_record,
                                                              bool fast_durability) {
    (void)set;
    (void)resources_;

    const std::filesystem::path target = std::filesystem::absolute(plan.workspace);

    // §2.7.13: an incomplete workspace must never be presented as a valid
    // project. Refusing an existing target keeps that structural rather than
    // conditional — regeneration into an existing workspace needs the journal
    // (§2.15.9), which this engine build does not implement.
    if (filesystem_.exists(target)) {
        EngineError error = make_error(ErrorCategory::filesystem, "filesystem.workspace.exists",
                                       "the output location already exists: " + target.string());
        error.path        = target.string();
        error.recoverable = true;
        Value detail      = Value::object();
        detail.set("hint",
                   "this engine build generates new workspaces only; regenerating into an existing "
                   "one requires the journalled update path");
        error.diagnostics = std::move(detail);
        return Unexpected{std::move(error)};
    }

    // A target inside an existing workspace is refused too.
    //
    // Nothing prevented it before, and the result is a workspace the tier
    // commands cannot reach: `promote` takes a name in sandbox/, not a path,
    // so `sandbox/format/w` is invisible to it. The nested tree also sits
    // inside the outer workspace's provenance scope, where `workspace.verify`
    // will eventually report every file of it as untracked.
    //
    // The usual way in is a `sqpg new x` run from inside a workspace, where
    // `-o` defaults to `./x` and nothing looks up. That is an easy mistake and
    // an annoying one to undo, because the fix is moving a tree by hand.
    //
    // Recognised by the metadata record and by nothing else: a Makefile beside
    // an `mk/` directory describes half the C projects in existence, and this
    // check must not refuse to generate into somebody's unrelated source tree.
    for (std::filesystem::path ancestor = target.parent_path();
         !ancestor.empty() && ancestor != ancestor.root_path();
         ancestor = ancestor.parent_path()) {
        const std::filesystem::path record =
            ancestor / std::filesystem::path{std::string{MetadataService::kMetadataFile}};
        if (!filesystem_.exists(record)) continue;

        EngineError error = make_error(ErrorCategory::filesystem, "filesystem.workspace.nested",
                                       "the output location is inside an existing workspace: "
                                           + ancestor.string());
        error.path        = target.string();
        error.recoverable = true;
        Value detail      = Value::object();
        detail.set("workspace", ancestor.string());
        detail.set("hint",
                   "a workspace inside a workspace cannot be promoted or verified on its own; "
                   "generate outside it, or pass -o with a path that is not nested");
        error.diagnostics = std::move(detail);
        return Unexpected{std::move(error)};
    }

    const std::filesystem::path staging = staging_path_for(target);
    if (filesystem_.exists(staging)) {
        // A leftover staging directory means a previous run was interrupted
        // before its rename. Nothing was ever visible as a project, so
        // removing it is safe and is reported by the caller as a diagnostic.
        if (auto removed = filesystem_.remove_all(staging); !removed) return Unexpected{removed.error()};
    }

    if (auto created = filesystem_.create_directories(staging); !created) {
        return Unexpected{created.error()};
    }

    Outcome outcome;

    // From here on, any failure removes the staging tree and returns. The
    // target never existed, so there is nothing to roll back to — which is
    // exactly why D-024 chose this shape for the new-project case.
    const auto abort_with = [&](EngineError error) -> Result<Outcome> {
        (void)filesystem_.remove_all(staging);
        return Unexpected{std::move(error)};
    };

    for (const PlanStep& step : plan.steps) {
        const std::filesystem::path destination = staging / std::filesystem::path{step.path};

        if (step.action == StepAction::create_directory) {
            if (auto created = filesystem_.create_directories(destination); !created) {
                return abort_with(created.error());
            }
            ++outcome.directories_created;
            continue;
        }

        if (auto written = filesystem_.write_file(destination, step.content, step.executable); !written) {
            return abort_with(written.error());
        }
        if (!fast_durability) filesystem_.sync_path(destination);
        ++outcome.files_written;

        outcome.hashes.emplace_back(step.path, support::to_hex(support::sha256(step.content)));
    }

    // The provenance table (§2.7.10). One entry per generator-created path,
    // carrying the hash the file had at generation time — the record that
    // lets a later update distinguish "unchanged since generation" from "the
    // user edited this" without guessing.
    Value record = metadata_record;
    Value provenance = Value::array();
    for (const PlanStep& step : plan.steps) {
        if (step.action == StepAction::create_directory) continue;
        Value entry = Value::object();
        entry.set("path", step.path);
        entry.set("ownership", std::string{to_string(step.ownership)});
        entry.set("origin", step.origin.str());
        entry.set("origin_version", step.origin_version);
        entry.set("phase", std::string{to_string(step.phase)});
        const auto found = std::find_if(outcome.hashes.begin(), outcome.hashes.end(),
                                        [&](const auto& pair) { return pair.first == step.path; });
        entry.set("sha256", found != outcome.hashes.end() ? found->second : std::string{});
        provenance.push(std::move(entry));
    }
    record.set("provenance", provenance);
    record.set("durability", std::string{fast_durability ? "fast" : "durable"});

    const std::string metadata_json = support::json_write(record, true);
    const std::filesystem::path metadata_file = staging / std::filesystem::path{std::string{MetadataService::kMetadataFile}};
    if (auto written = filesystem_.write_file(metadata_file, metadata_json, false); !written) {
        return abort_with(written.error());
    }
    ++outcome.files_written;
    if (!fast_durability) filesystem_.sync_path(metadata_file);

    // The workspace as a cartridge, for square: its manifest and the
    // provenance table by path (square's spec §2.1-2.2).
    const std::pair<std::string_view, Value> workspace_files[] = {
        {MetadataService::kWorkspaceManifest, MetadataService::workspace_manifest(plan, record)},
        {MetadataService::kWorkspaceFiles, MetadataService::workspace_files(record)},
    };
    for (const auto& [path, value] : workspace_files) {
        const std::filesystem::path file = staging / std::filesystem::path{std::string{path}};
        if (auto created = filesystem_.create_directories(file.parent_path()); !created) {
            return abort_with(created.error());
        }
        if (auto written = filesystem_.write_file(file, support::json_write(value, true), false); !written) {
            return abort_with(written.error());
        }
        ++outcome.files_written;
        if (!fast_durability) filesystem_.sync_path(file);
    }

    // Land it. One rename, same filesystem, atomic with respect to any
    // observer: either the workspace exists complete and user-owned, or it
    // does not exist at all (§2.7.12).
    if (auto moved = filesystem_.rename(staging, target); !moved) {
        return abort_with(moved.error());
    }
    if (!fast_durability && target.has_parent_path()) filesystem_.sync_path(target.parent_path());

    return outcome;
}

}  // namespace squared::pg
