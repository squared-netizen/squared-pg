// SPDX-License-Identifier: MIT
//
// engine/src/services/services.hpp — the core services.
//
// Specification: §2.4.3 (service registration), §2.4.5 (the service layer is
// the fundamental control surface), §2.4.6 (operations must not bypass
// service boundaries).
//
// These types are internal. §1.3 forbids exposing implementation headers as
// public API for convenience; the control surface is the operation registry
// in engine.hpp, and every one of these services is reached through it.
//
// Pattern: Service Locator, scoped to the engine instance. Deviation from the
// textbook form, which is a process-wide singleton: §2.4.12 permits several
// engine instances in one process and §2.4.13 forbids hidden global state, so
// the registry is an instance member. Lookup is by a compile-time constant
// name on each service type, so a typo is a link error rather than a runtime
// null.

#ifndef SQUARED_PG_SERVICES_HPP
#define SQUARED_PG_SERVICES_HPP

#include "squared/pg/error.hpp"
#include "squared/pg/identity.hpp"
#include "squared/pg/operation.hpp"
#include "squared/pg/plan.hpp"
#include "squared/pg/resource.hpp"
#include "squared/pg/result.hpp"
#include "squared/pg/value.hpp"
#include "squared/pg/version.hpp"

#include "sqcart/sqcart.hpp"
#include "support/support.hpp"

#include <filesystem>
#include <map>
#include <span>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace squared::pg {

class Service {
public:
    Service()          = default;
    virtual ~Service() = default;

    Service(const Service&)            = delete;
    Service& operator=(const Service&) = delete;

    [[nodiscard]] virtual std::string_view service_name() const noexcept = 0;
};

/// §2.4.3. Registration is deterministic and happens once, during
/// initialization; a missing required core service prevents Ready.
class ServiceLocator {
public:
    void register_service(Service& service);

    [[nodiscard]] Service* find(std::string_view name) const;

    template <class T>
    [[nodiscard]] T* get() const {
        // static_cast is sound because the name is a property of the type,
        // registered by the same type that reads it.
        return static_cast<T*>(find(T::kServiceName));
    }

    [[nodiscard]] std::vector<std::string> names() const;

private:
    std::vector<std::pair<std::string, Service*>> services_;
};

// ---------------------------------------------------------------------------
// Filesystem
// ---------------------------------------------------------------------------

/// §2.4.3, §2.5.9. Every write the engine performs goes through here, which
/// is what makes "Lua may not mutate a workspace except through engine
/// services" enforceable rather than a request.
class FilesystemService final : public Service {
public:
    static constexpr std::string_view kServiceName = "filesystem";
    [[nodiscard]] std::string_view service_name() const noexcept override { return kServiceName; }

    [[nodiscard]] Result<void> create_directories(const std::filesystem::path& path) const;
    [[nodiscard]] Result<void> write_file(const std::filesystem::path& path, std::string_view content,
                                          bool executable) const;
    [[nodiscard]] Result<std::string> read_file(const std::filesystem::path& path) const;
    [[nodiscard]] Result<void> remove_all(const std::filesystem::path& path) const;
    [[nodiscard]] Result<void> rename(const std::filesystem::path& from,
                                      const std::filesystem::path& to) const;
    [[nodiscard]] bool exists(const std::filesystem::path& path) const;

    /// Flush a file's contents and its directory entry. §2.15.11 `durable`
    /// mode requires both; the directory flush is the half people forget, and
    /// without it a rename can be lost even though the data survived.
    void sync_path(const std::filesystem::path& path) const;
};

// ---------------------------------------------------------------------------
// Resource
// ---------------------------------------------------------------------------

/// §2.4.4, §2.8.4. Builds the immutable index and hands out payloads.
///
/// Validates the manifest *envelope* only — well-formedness, identity syntax,
/// version syntax, integrity. What a template or kit manifest means is the
/// owning typed service's job (D-012).
class ResourceService final : public Service {
public:
    static constexpr std::string_view kServiceName = "resource";
    [[nodiscard]] std::string_view service_name() const noexcept override { return kServiceName; }

    /// Scan roots in declared order, entries within a root sorted (§2.8.4).
    /// Diagnostics accumulate; a malformed resource is reported and skipped,
    /// but a duplicate identity fails initialization outright (§2.8.1).
    [[nodiscard]] Result<void> build_index(const std::vector<std::filesystem::path>& roots,
                                           const Version& engine_version, bool strict_roots,
                                           std::vector<Diagnostic>& diagnostics);

    [[nodiscard]] const ResourceIndex& index() const noexcept { return index_; }

    /// Open a resource payload. Lazy: called only when a resolved resource is
    /// about to be read (§2.4.4). Cached, because a kit resolved and then
    /// applied would otherwise be opened twice.
    [[nodiscard]] Result<std::shared_ptr<sqcart::Cartridge>> open(const ResourceRecord& record);

private:
    ResourceIndex index_;
    std::map<std::string, std::shared_ptr<sqcart::Cartridge>> cache_;
};

// ---------------------------------------------------------------------------
// Resolved resources
// ---------------------------------------------------------------------------

/// A resource that resolution has accepted, with its payload open.
///
/// Holds a shared_ptr to the cartridge because `body` points into the
/// manifest the cartridge owns; keeping the pointer without keeping the
/// cartridge would be a dangling reference the type system would not catch.
/// Ownership globs for a template's payload (§2.7.10).
///
/// Formerly `sqcart::OwnershipRules`. Cartridge format 2 removed it along
/// with every other kind body: which files a *generator* may rewrite is a
/// generator's question, and a cartridge reader that answered it was the
/// coupling format spec §0.1 forbids. The declaration now lives here, and is
/// read from `consumers.squared_pg.ownership`.
///
/// The three pattern lists MUST NOT overlap each other. A path matching two
/// of them is a manifest defect, not something to be resolved: the author has
/// said two contradictory things about one file and only they know which was
/// meant.
///
/// `default_class` is what an *unmatched* path becomes, and it is the reason
/// the lists can be required not to overlap. Before it existed, a template
/// wanting "everything else is the user's" had to say so with a pattern --
/// `user: ["**"]` -- and a pattern meaning "everything" necessarily overlaps
/// every other pattern in the manifest. The field separates the two ideas
/// that spelling was conflating: *claiming specific files* and *a fallback
/// for the rest*.
struct OwnershipRules {
    std::vector<std::string> generated;
    std::vector<std::string> user;
    std::vector<std::string> shared;

    /// Class for a path no pattern matches. Absent in the manifest means
    /// `seeded`, which is the conservative reading: written once, never
    /// rewritten, so a template that says nothing cannot lose user work.
    OwnershipClass           default_class{OwnershipClass::seeded};

    /// Whether the manifest stated `default` explicitly.
    ///
    /// Kept because "said nothing" and "said seeded" are the same outcome but
    /// different facts, and `workspace.verify` should be able to tell an
    /// author which one they are relying on.
    bool                     default_declared{false};
};

/// One declared template parameter (§2.8.7).
///
/// Formerly `sqcart::TemplateBody::Parameter`, moved here for the same reason
/// as OwnershipRules.
struct TemplateParameter {
    std::string                name;
    std::string                type{"string"};
    bool                       required{false};
    std::optional<std::string> description;
    std::optional<Value>       default_value;
    std::optional<std::string> default_from;
};

struct ResolvedResource {
    const ResourceRecord*              record{nullptr};
    std::shared_ptr<sqcart::Cartridge> cartridge;

    /// Payload root inside the cartridge, from the manifest envelope's
    /// `tree` (D-031). Empty means the cartridge root, which the manifest
    /// spells "."; otherwise a path with a trailing separator.
    std::string tree_prefix;

    /// Ownership globs (§2.7.10).
    OwnershipRules ownership;

    /// Integration areas: declared by a template, written by a kit.
    std::vector<std::string> integration_areas;

    [[nodiscard]] const ResourceId& id() const noexcept { return record->id; }
    [[nodiscard]] std::string version() const { return record->version.to_string(); }
};

/// Resolution context shared by the typed services. Carries the accumulated
/// facts a later resolution needs to check compatibility against — the
/// template a kit must support, the platforms in play.
struct ResolutionContext {
    std::vector<std::string> platforms;
    std::string              framework_version;
    ResourceId               template_id;
};

// ---------------------------------------------------------------------------
// Typed resource services
// ---------------------------------------------------------------------------

class TemplateService final : public Service {
public:
    static constexpr std::string_view kServiceName = "template";
    [[nodiscard]] std::string_view service_name() const noexcept override { return kServiceName; }

    TemplateService(ResourceService& resources, const std::vector<Capability>& capabilities)
        : resources_(resources), capabilities_(capabilities) {}

    /// §2.7.4. Resolves by identity, never by path, and never substitutes an
    /// alternative when resolution fails — that decision belongs to Lua.
    [[nodiscard]] Result<ResolvedResource> resolve(const ResourceRef& ref, const ResolutionContext& context,
                                                   std::vector<Diagnostic>& diagnostics);

    /// The template's declared parameters, as the manifest states them.
    ///
    /// By value, not a span. Format 1 let this borrow from a typed body the
    /// Cartridge owned; the parameters are now parsed out of the consumer
    /// section on demand, so there is nothing stable to point at.
    [[nodiscard]] static std::vector<TemplateParameter>
    parameters(const ResolvedResource& resolved);

    [[nodiscard]] static std::string working_directory(const ResolvedResource& resolved);

private:
    ResourceService&            resources_;
    /// A reference, not a span. The engine's capability list is filled by
    /// register_capabilities() *after* the services are constructed, and a
    /// span captured at construction would be a stale view over an empty
    /// vector that later reallocated -- so every requirement would silently
    /// pass. A reference sees the list as it ends up.
    const std::vector<Capability>& capabilities_;
};

class KitService final : public Service {
public:
    static constexpr std::string_view kServiceName = "kit";
    [[nodiscard]] std::string_view service_name() const noexcept override { return kServiceName; }

    KitService(ResourceService& resources, const std::vector<Capability>& capabilities)
        : resources_(resources), capabilities_(capabilities) {}

    /// §2.7.5. Validates template compatibility, platform support and
    /// framework range at resolution — before any workspace mutation.
    [[nodiscard]] Result<ResolvedResource> resolve(const ResourceRef& ref, const ResolutionContext& context,
                                                   std::vector<Diagnostic>& diagnostics);

private:
    ResourceService&            resources_;
    /// A reference, not a span. The engine's capability list is filled by
    /// register_capabilities() *after* the services are constructed, and a
    /// span captured at construction would be a stale view over an empty
    /// vector that later reallocated -- so every requirement would silently
    /// pass. A reference sees the list as it ends up.
    const std::vector<Capability>& capabilities_;
};

/// §2.7.6. Packages resolve and are recorded; materialization of package
/// payloads is not implemented in this pass and is refused explicitly rather
/// than silently skipped — see docs/developer/engine/limitations.md.
class PackageService final : public Service {
public:
    static constexpr std::string_view kServiceName = "package";
    [[nodiscard]] std::string_view service_name() const noexcept override { return kServiceName; }

    PackageService(ResourceService& resources, const std::vector<Capability>& capabilities)
        : resources_(resources), capabilities_(capabilities) {}

    [[nodiscard]] Result<ResolvedResource> resolve(const ResourceRef& ref, const ResolutionContext& context,
                                                   std::vector<Diagnostic>& diagnostics);

private:
    ResourceService&            resources_;
    /// A reference, not a span. The engine's capability list is filled by
    /// register_capabilities() *after* the services are constructed, and a
    /// span captured at construction would be a stale view over an empty
    /// vector that later reallocated -- so every requirement would silently
    /// pass. A reference sees the list as it ends up.
    const std::vector<Capability>& capabilities_;
};

/// §2.7.7, §2.10. As with packages: resolution is implemented, transformation
/// is not, and an asset requiring a transformation other than `copy` fails at
/// resolution per §2.10.5 rather than at materialization.
class AssetService final : public Service {
public:
    static constexpr std::string_view kServiceName = "asset";
    [[nodiscard]] std::string_view service_name() const noexcept override { return kServiceName; }

    AssetService(ResourceService& resources, const std::vector<Capability>& capabilities)
        : resources_(resources), capabilities_(capabilities) {}

    [[nodiscard]] Result<ResolvedResource> resolve(const ResourceRef& ref, const ResolutionContext& context,
                                                   std::vector<Diagnostic>& diagnostics);

private:
    ResourceService&            resources_;
    /// A reference, not a span. The engine's capability list is filled by
    /// register_capabilities() *after* the services are constructed, and a
    /// span captured at construction would be a stale view over an empty
    /// vector that later reallocated -- so every requirement would silently
    /// pass. A reference sees the list as it ends up.
    const std::vector<Capability>& capabilities_;
};

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

/// The resolved set, before any mutation. §2.7.9's resolve-before-mutate rule
/// is enforced by the fact that nothing downstream of here can run without
/// one of these.
struct ResolvedSet {
    ResolvedResource              project_template;
    std::vector<ResolvedResource> kits;
    std::vector<ResolvedResource> packages;
    std::vector<ResolvedResource> assets;
    ResolutionContext             context;
};

class ValidationService final : public Service {
public:
    static constexpr std::string_view kServiceName = "validation";
    [[nodiscard]] std::string_view service_name() const noexcept override { return kServiceName; }

    /// §2.7.2. Required inputs, parameter contract, identity syntax.
    [[nodiscard]] Result<void> validate_configuration(const Value& config) const;

    /// §2.7.9 cross-validation. Runs after everything resolves and before
    /// phase 8 may begin. A failure here prevents the transaction from
    /// opening at all.
    [[nodiscard]] Result<void> cross_validate(const ResolvedSet& set, const Value& config,
                                              std::vector<Diagnostic>& diagnostics) const;

    /// §2.8.7. Values are checked against the template's declared parameter
    /// contract; every offender is named in one error, not the first.
    [[nodiscard]] Result<void> validate_parameters(const ResolvedResource& project_template,
                                                   const Value& parameters) const;
};

// ---------------------------------------------------------------------------
// Metadata
// ---------------------------------------------------------------------------

/// §2.7.3 (workspace metadata), §2.7.10 (provenance table).
class MetadataService final : public Service {
public:
    static constexpr std::string_view kServiceName = "metadata";
    [[nodiscard]] std::string_view service_name() const noexcept override { return kServiceName; }

    static constexpr std::string_view kMetadataDir  = ".squared";
    static constexpr std::string_view kMetadataFile = ".squared/metadata.json";
    static constexpr int              kRecordVersion = 1;

    /// Build the record. Deliberately excludes absolute host paths, temporary
    /// locations, user account information and engine handles (§2.7.3), and
    /// carries no timestamp so that two identical generations produce
    /// byte-identical metadata (§2.7.13).
    [[nodiscard]] Value build_record(const GenerationPlan& plan, const Version& engine_version) const;
};

// ---------------------------------------------------------------------------
// Transaction
// ---------------------------------------------------------------------------

/// §2.15.8 (staging strategy; D-024), §2.15.11 (durability).
///
/// This pass implements the new-project case only: build the whole workspace
/// in a sibling temporary directory on the *target* filesystem, then land it
/// with one rename. Cross-device rename cannot arise, which is the property
/// Termux needs. Regeneration into an existing workspace requires the journal
/// and is refused with a structured error rather than approximated.
class TransactionService final : public Service {
public:
    static constexpr std::string_view kServiceName = "transaction";
    [[nodiscard]] std::string_view service_name() const noexcept override { return kServiceName; }

    TransactionService(FilesystemService& filesystem, ResourceService& resources)
        : filesystem_(filesystem), resources_(resources) {}

    struct Outcome {
        std::size_t files_written{0};
        std::size_t directories_created{0};
        /// path -> content hash at generation time, for the provenance table.
        std::vector<std::pair<std::string, std::string>> hashes;
    };

    /// Execute the plan. On any failure the staging directory is removed and
    /// the target never existed — §2.7.13's "an incomplete workspace must not
    /// be presented as a valid project" is satisfied structurally rather than
    /// by cleanup discipline.
    [[nodiscard]] Result<Outcome> apply(const GenerationPlan& plan, const ResolvedSet& set,
                                        const Value& metadata_record, bool fast_durability);

private:
    FilesystemService& filesystem_;
    ResourceService&   resources_;
};

// ---------------------------------------------------------------------------
// Project
// ---------------------------------------------------------------------------

/// §2.7. Builds the generation plan from a resolved set. Owns no filesystem
/// access at all: producing a plan must be possible without the workspace
/// lock (§2.15.12) and without creating anything (§2.7.9).
class ProjectService final : public Service {
public:
    static constexpr std::string_view kServiceName = "project";
    [[nodiscard]] std::string_view service_name() const noexcept override { return kServiceName; }

    explicit ProjectService(ResourceService& resources) : resources_(resources) {}

    [[nodiscard]] Result<GenerationPlan> build_plan(const ResolvedSet& set, const Value& config,
                                                    const Version& engine_version,
                                                    std::vector<Diagnostic>& diagnostics);

private:
    ResourceService& resources_;
};

// ---------------------------------------------------------------------------
// Shared helpers used by more than one service
// ---------------------------------------------------------------------------

namespace detail {

/// Ownership class for a workspace-relative path under a resource's rules.
/// The most specific matching pattern wins; an unmatched path is `seeded`
/// (§2.7.10 — the safe default is never to overwrite).
[[nodiscard]] OwnershipClass classify_path(const OwnershipRules& rules, std::string_view path,
                                           std::vector<Diagnostic>* diagnostics);

/// A member of this engine's section of a cartridge manifest.
///
/// Cartridge format 2 removed typed kind bodies. Everything squared-pg needs
/// from a manifest beyond the envelope -- ownership, parameters, integration
/// areas, working directory, the external acquisition block -- now lives
/// under `consumers.squared_pg`, as JSON this engine parses and sqcart never
/// opens (format spec 5.6).
///
/// That is a promotion, not a workaround. The previous form of this function
/// read raw_json() for eight fields the cartridge format had no place for,
/// alongside a typed API for the fields it did. There is one path now, and
/// the fields that used to be second-class are the only kind there is.
///
/// `member` is looked up in this engine's section, and may be a dotted path:
/// `"requires.kits.required"` walks three levels. Returns a null Value when
/// the section is absent or any segment of the path is not present; callers distinguish
/// the two only where it matters, because for most fields "no section" and
/// "no such field" both mean "fall back to the default".
///
/// The `body_key` parameter is gone. It named the kind body -- "template",
/// "kit" -- and there are no kind bodies to name.
[[nodiscard]] Value manifest_extension(const sqcart::Manifest& manifest, std::string_view member);

/// Ownership globs out of this engine's manifest section (§2.7.10).
///
/// Sits beside manifest_extension() rather than with the struct, because it
/// is the same act: reading a member of `consumers.squared_pg` and giving it
/// a shape. An absent or malformed `ownership` yields three empty lists,
/// which classify_path treats as "everything is seeded" -- the conservative
/// reading, and the one that cannot destroy user work.
[[nodiscard]] OwnershipRules ownership_rules(const sqcart::Manifest& manifest);

/// This engine's consumer identity in a cartridge manifest (format spec 5.6).
inline constexpr std::string_view kConsumerId = "squared_pg";

/// The parameter set a template is actually instantiated with.
///
/// Three layers, later winning over earlier: the template's declared defaults
/// (§2.8.7), the engine's built-ins derived from the request, and the values
/// the workflow supplied.
///
/// Shared by validation and plan building on purpose. If validation checked
/// the raw workflow values while substitution used the effective ones, every
/// workflow would have to restate `project_name = name` to get past a check
/// the engine was about to satisfy itself.
[[nodiscard]] Value effective_parameters(const ResolvedResource& project_template, const Value& config,
                                         std::string_view working_directory,
                                         const Version& engine_version,
                                         const std::vector<std::string>& kit_ids);

/// Whether the payload looks like text that may be substituted. A NUL byte in
/// the first few kilobytes means binary; substitution into a PNG would corrupt
/// it silently, which is exactly the failure §2.8.8 warns about.
[[nodiscard]] bool looks_textual(std::string_view content);

/// Expand `{{ name }}` references. An unknown parameter is an error, never an
/// empty string (§2.8.8): a silent substitution failure produces a project
/// that looks generated correctly and does not build.
[[nodiscard]] Result<std::string> substitute(std::string_view content, const Value& parameters,
                                             std::string_view origin_path);

}  // namespace detail

}  // namespace squared::pg

#endif  // SQUARED_PG_SERVICES_HPP
