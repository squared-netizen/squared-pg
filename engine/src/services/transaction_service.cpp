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

    std::vector<std::string> packages;
    for (const ResourceRef& ref : plan.packages) packages.push_back(ref.to_string());
    resources.set("packages", Value::strings(packages));

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
    return record;
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
