// SPDX-License-Identifier: MIT
//
// squared/pg/plan.hpp — the generation plan.
//
// Specification: §2.7.9 (the generation plan), §2.7.10 (ownership classes and
// provenance), §2.4.6 (operations are reified).
//
// Pattern: Command. Every workspace mutation is a PlanStep object before it
// is an action. `project.plan` builds the list and returns it;
// `project.generate` builds the same list and then executes it. Plan and
// apply consume one list, so they cannot describe different things — which is
// the property that makes a dry run trustworthy rather than approximate.
//
// Deviation from the textbook Command: a step carries no execute() method.
// Execution lives in the Transaction Service so that ordering, journalling
// and rollback are decided in one place rather than distributed across step
// subclasses, and so a plan can be produced by a read-only engine that holds
// no workspace lock (§2.15.12).

#ifndef SQUARED_PG_PLAN_HPP
#define SQUARED_PG_PLAN_HPP

#include "squared/pg/error.hpp"
#include "squared/pg/identity.hpp"
#include "squared/pg/value.hpp"
#include "squared/pg/version.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace squared::pg {

/// §2.7.10. `merged` is reserved and unimplemented in v1 (D-020); the name is
/// held so adding it later is not a breaking change.
enum class OwnershipClass : std::uint8_t {
    generated,  ///< generator-managed; may be overwritten
    seeded,     ///< created once, user-owned thereafter; never overwritten
    user,       ///< created by the user; never touched
    metadata,   ///< generator record; may be rewritten
};

[[nodiscard]] std::string_view to_string(OwnershipClass cls) noexcept;
[[nodiscard]] OwnershipClass ownership_class_from_string(std::string_view text) noexcept;

/// The kind of mutation a step performs.
enum class StepAction : std::uint8_t {
    create_directory,
    write_file,   ///< content carried in the step (substituted or generated)
    copy_entry,   ///< payload copied from the originating resource
};

/// Which §2.7.9 phase produced the step. Recorded in provenance so an update
/// can tell a template file from a kit file without re-resolving.
enum class GenerationPhase : std::uint8_t {
    workspace,
    template_instantiate,
    kit_apply,
    package_materialize,
    framework_integrate,
    asset_materialize,
    build_emit,
    metadata_write,
};

[[nodiscard]] std::string_view to_string(GenerationPhase phase) noexcept;

/// One reified mutation.
struct PlanStep {
    StepAction      action{StepAction::write_file};
    GenerationPhase phase{GenerationPhase::workspace};
    std::string     path;    ///< workspace-relative, always; §2.7.10 forbids absolute host paths
    OwnershipClass  ownership{OwnershipClass::seeded};

    ResourceId  origin;          ///< resource that produced this path
    std::string origin_version;

    /// For copy_entry: the entry path inside the originating cartridge.
    std::string source_entry;
    /// For write_file: the exact bytes to write.
    std::string content;
    /// Whether the destination should be marked executable.
    bool executable{false};

    [[nodiscard]] Value to_value() const;
};

/// §2.7.9. Contains everything needed to decide whether to proceed, without
/// having created anything.
struct GenerationPlan {
    std::string project_name;
    std::string workspace;          ///< absolute target path, host-side only
    std::string working_directory;  ///< workspace-relative, §2.7.3

    ResourceRef              template_ref;
    std::string              template_version;
    std::vector<ResourceRef> kits;
    std::vector<std::string> kit_versions;
    std::vector<ResourceRef> packages;
    std::vector<ResourceRef> assets;
    std::vector<std::string> platforms;
    std::string              framework_version;

    std::vector<PlanStep>  steps;
    std::vector<Diagnostic> diagnostics;

    /// The plan as it crosses the boundary. Steps are listed in execution
    /// order; §2.7.9 requires the path set and its ownership classes to be
    /// visible before anything is written.
    [[nodiscard]] Value to_value() const;
};

}  // namespace squared::pg

#endif  // SQUARED_PG_PLAN_HPP
