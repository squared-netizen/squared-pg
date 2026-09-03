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
class ResourceIndex {
public:
    ResourceIndex() = default;

    /// Insert during construction. Fails on duplicate (id, version): §2.8.1
    /// makes a duplicate identity an initialization failure, never a silent
    /// override.
    [[nodiscard]] Result<void> insert(ResourceRecord record);

    /// Every indexed version of `id`, highest precedence first.
    [[nodiscard]] std::vector<const ResourceRecord*> versions_of(const ResourceId& id) const;

    /// Highest version of `id` satisfying `range`, per the selection policy in
    /// §2.8.2: filter, sort descending, take the first.
    [[nodiscard]] const ResourceRecord* select(const ResourceId& id, const VersionRange& range) const;

    [[nodiscard]] std::vector<const ResourceRecord*> of_kind(ResourceKind kind) const;
    [[nodiscard]] const std::vector<ResourceRecord>& all() const noexcept { return records_; }
    [[nodiscard]] std::size_t size() const noexcept { return records_.size(); }

private:
    /// Sorted by (id, version descending) after seal(); insertion order is
    /// discovery order, which §2.8.4 already requires to be deterministic.
    std::vector<ResourceRecord> records_;
};

}  // namespace squared::pg

#endif  // SQUARED_PG_RESOURCE_HPP
