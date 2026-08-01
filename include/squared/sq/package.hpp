#pragma once

#include <squared/sq/manifest.hpp>
#include <squared/sq/result.hpp>
#include <squared/sq/validation.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace squared::sq {

/**
 * @brief Extraction controls layered over native SQ validation.
 */
struct ExtractionPolicy {
    /// Validation policy applied before extraction.
    ValidationPolicy validation;
    /// Reserved for a future recoverable replacement transaction.
    bool replace_existing{false};
    /// Retain staging state after failure for explicit diagnostics.
    bool preserve_failed_staging{false};
};

/**
 * @brief Summary of one completed transactional extraction.
 */
struct ExtractionReport {
    /// Final installed content directory.
    std::filesystem::path destination;
    /// Number of content files written.
    std::uint64_t files_written{0};
    /// Combined bytes written to content files.
    std::uint64_t bytes_written{0};
    /// Validated package content identity.
    std::string content_digest;
};

/**
 * @brief Move-only read handle for a standard ZIP-backed SQ package.
 *
 * Package is not thread-safe. Separate Package instances may be used on
 * separate threads. Reading never mutates the source archive.
 */
class Package {
public:
    /**
     * @brief Transfer ownership from another Package.
     */
    Package(Package&&) noexcept;
    /**
     * @brief Replace this handle by transferring another Package.
     */
    Package& operator=(Package&&) noexcept;
    /**
     * @brief Package handles cannot be copied.
     */
    Package(const Package&) = delete;
    /**
     * @brief Package handles cannot be copied.
     */
    Package& operator=(const Package&) = delete;
    /**
     * @brief Close the native archive handle and release owned metadata.
     */
    ~Package();

    /**
     * @brief Open an SQ package and parse its strict base manifest.
     *
     * @param archive Existing regular-file path.
     * @return Owning Package or structured failure. No files are extracted.
     */
    [[nodiscard]]
    static Result<Package> open(const std::filesystem::path& archive) noexcept;

    /**
     * @brief Return the immutable base manifest parsed during open().
     */
    [[nodiscard]] const Manifest& manifest() const noexcept;

    /**
     * @brief Validate paths, resource limits, inventory, and file hashes.
     *
     * @param policy Machine-owned validation policy.
     * @return A bounded report or a structural failure.
     */
    [[nodiscard]]
    Result<ValidationReport> validate(
        const ValidationPolicy& policy = {}
    ) const noexcept;

    /**
     * @brief Validate and extract content/ into a new destination atomically.
     *
     * The destination must not exist in version 0. Extraction uses bounded
     * streaming buffers and removes staging state after failure unless the
     * caller explicitly requests preservation.
     */
    [[nodiscard]]
    Result<ExtractionReport> extract_transactionally(
        const std::filesystem::path& destination,
        const ExtractionPolicy& policy = {}
    ) const noexcept;

private:
    class Implementation;

    explicit Package(std::unique_ptr<Implementation> implementation);
    std::unique_ptr<Implementation> implementation_;
};

}  // namespace squared::sq
