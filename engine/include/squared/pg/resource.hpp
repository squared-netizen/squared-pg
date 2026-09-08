// SPDX-License-Identifier: MIT
//
// squared/pg/resource.hpp — the resource index.
//
// Specification: §2.4.4 (resource loading; D-011 immutable index), §2.8.4
// (discovery and indexing), §2.6.7 (identity, never path).
//
// The index maps identity to manifest, version and location. It is built once
// during initialization and is immutable for the session, which is what makes
// hybrid loading deterministic: the *set* of resources is fixed up front and
// only payload retrieval is deferred.
//
// `location` is present because something must eventually open the file, but
// it is deliberately not reachable from Lua. §2.5.9 forbids Lua from scanning
// or reading generator-owned resources directly, so the binding layer exposes
// identity, kind and version and drops the path.

#ifndef SQUARED_PG_RESOURCE_HPP
#define SQUARED_PG_RESOURCE_HPP

#include "squared/pg/error.hpp"
#include "squared/pg/identity.hpp"
#include "squared/pg/value.hpp"
#include "squared/pg/version.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace squared::pg {

/// How a resource is stored. Both forms are opened through sqcart, which
/// detects the form itself; the distinction is retained only for diagnostics.
enum class ResourceForm : std::uint8_t {
    exploded,  ///< a directory containing SQ-INF/manifest.json
    archive,   ///< a .sq container
};

/// One entry in the immutable index. Envelope-level facts only: everything
/// here was validated by the Resource Service (D-012). What a template or kit
/// manifest *means* is validated by the owning typed service when it resolves.
struct ResourceRecord {
    ResourceId            id;
    ResourceKind          kind{ResourceKind::project_template};
    Version               version;
    ResourceForm          form{ResourceForm::exploded};
    std::filesystem::path location;
    std::string           title;
    std::string           description;
    std::string           engine_range;  ///< manifest `engine.version`, empty when absent
    std::vector<std::string> required_features;

    /// Identity, kind and version only — the projection Lua is permitted to
    /// see. The location is withheld on purpose (§2.5.9).
    [[nodiscard]] Value to_public_value() const;
};

/// Immutable for the lifetime of the engine (D-011).
/// Two resources claiming one (id, version).
///
/// `kept` is the one the index holds -- the first in discovery order, which
/// §2.8.4 requires to be deterministic, so which of the two wins is stable
/// across machines even though neither deserves to.
struct ResourceConflict {
    ResourceId            id;
    Version               version;
    std::filesystem::path kept;
    std::filesystem::path discarded;
};

class ResourceIndex {
public:
    ResourceIndex() = default;

    /// Insert during construction.
    ///
    /// A duplicate (id, version) is **recorded, not fatal**. The first wins;
    /// the loser is remembered as a conflict and reported as a warning.
    ///
    /// It used to fail the whole index, which made a duplicate anywhere stop
    /// the tool before it did anything -- including `sqpg list`, which is
    /// exactly the command someone reaches for when trying to find out where
    /// the duplicate is. An index that refuses to be inspected because it
    /// contains a problem is the worst possible shape for diagnosing that
    /// problem (D-058).
    ///
    /// §2.8.1 is preserved where it matters: `conflict_for()` lets resolution
    /// refuse a request for an ambiguous identity. Nothing is silently
    /// overridden -- it is refused at the point where a wrong answer would do
    /// harm rather than at the point where any answer would do.
    ///
    /// Returns the conflict when one occurred, so the caller can report it.
    [[nodiscard]] std::optional<ResourceConflict> insert(ResourceRecord record);

    /// Every indexed version of `id`, highest precedence first.
    [[nodiscard]] std::vector<const ResourceRecord*> versions_of(const ResourceId& id) const;

    /// Highest version of `id` satisfying `range`, per the selection policy in
    /// §2.8.2: filter, sort descending, take the first.
    [[nodiscard]] const ResourceRecord* select(const ResourceId& id, const VersionRange& range) const;

    /// The conflict for `id`, if any version of it was declared twice.
    ///
    /// Resolution consults this before selecting: a request for an ambiguous
    /// identity must fail rather than take whichever copy happened to be
    /// scanned first, because the two may differ in ways nothing downstream
    /// would notice.
    [[nodiscard]] const ResourceConflict* conflict_for(const ResourceId& id) const;

    /// Every conflict found while indexing, for reporting.
    [[nodiscard]] const std::vector<ResourceConflict>& conflicts() const noexcept {
        return conflicts_;
    }

    [[nodiscard]] std::vector<const ResourceRecord*> of_kind(ResourceKind kind) const;
    [[nodiscard]] const std::vector<ResourceRecord>& all() const noexcept { return records_; }
    [[nodiscard]] std::size_t size() const noexcept { return records_.size(); }

private:
    /// Sorted by (id, version descending) after seal(); insertion order is
    /// discovery order, which §2.8.4 already requires to be deterministic.
    std::vector<ResourceRecord> records_;
    std::vector<ResourceConflict> conflicts_;
};

}  // namespace squared::pg

#endif  // SQUARED_PG_RESOURCE_HPP
