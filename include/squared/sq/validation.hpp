#pragma once

#include <squared/sq/error.hpp>
#include <squared/sq/manifest.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace squared::sq {

/**
 * @brief Caller-owned resource ceilings applied during validation.
 */
struct ResourceLimits {
    /// Maximum expanded byte count for .squared/manifest.json.
    std::uint64_t maximum_manifest_bytes{1024ULL * 1024ULL};
    /// Maximum number of central-directory entries.
    std::uint64_t maximum_entries{100000ULL};
    /// Maximum expanded byte count for one regular file.
    std::uint64_t maximum_file_bytes{512ULL * 1024ULL * 1024ULL};
    /// Maximum combined expanded byte count.
    std::uint64_t maximum_total_bytes{2ULL * 1024ULL * 1024ULL * 1024ULL};
    /// Maximum portable archive-path byte count.
    std::uint64_t maximum_path_bytes{240ULL};
    /// Maximum byte count for one archive-path component.
    std::uint64_t maximum_component_bytes{100ULL};
    /// Maximum expanded-to-compressed ratio for one entry.
    double maximum_compression_ratio{200.0};
};

/**
 * @brief Complete policy for validating one SQ package.
 */
struct ValidationPolicy {
    /// Optional semantic kind required by the caller.
    std::optional<PackageKind> expected_kind;
    /// Machine-owned resource ceilings.
    ResourceLimits limits;
    /// Recompute and compare every regular-file SHA-256.
    bool verify_checksums{true};
    /// Reject inventory records without corresponding files.
    bool require_complete_inventory{true};
    /// Reject paths that collide after ASCII case folding.
    bool reject_case_collisions{true};
};

/**
 * @brief Validation issue severity.
 */
enum class IssueSeverity {
    Warning, ///< Informational incompatibility that does not invalidate.
    Error    ///< Validation failure that invalidates the package.
};

/**
 * @brief One bounded validation issue.
 */
struct ValidationIssue {
    /// Issue severity.
    IssueSeverity severity{IssueSeverity::Error};
    /// Stable issue category.
    ErrorCode code{ErrorCode::InvalidArchive};
    /// Human-readable explanation.
    std::string message;
    /// Portable archive path involved in the issue.
    std::string archive_path;
};

/**
 * @brief Read-only validation result and package resource summary.
 */
struct ValidationReport {
    /// Parsed base manifest.
    Manifest manifest;
    /// Bounded list of validation warnings and errors.
    std::vector<ValidationIssue> issues;
    /// Central-directory entry count.
    std::uint64_t entry_count{0};
    /// Combined compressed bytes for regular files.
    std::uint64_t compressed_bytes{0};
    /// Combined expanded bytes for regular files.
    std::uint64_t expanded_bytes{0};
    /// Canonical SHA-256 identity derived from paths and file hashes.
    std::string content_digest;

    /**
     * @brief Return true when the report contains no error issues.
     */
    [[nodiscard]] bool valid() const noexcept;
};

}  // namespace squared::sq
