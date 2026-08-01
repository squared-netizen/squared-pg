#pragma once

#include <squared/sq/manifest.hpp>
#include <squared/sq/result.hpp>

#include <cstdint>
#include <filesystem>
#include <string>

namespace squared::sq {

/**
 * @brief Deterministic SQ archive-writing controls.
 */
struct WriterOptions {
    /// Must remain true in format version 0.
    bool deterministic{true};
    /// Use Deflate when true and Stored entries when false.
    bool compress{true};
    /// Deflate level in the inclusive range zero through nine.
    std::uint32_t compression_level{6};
};

/**
 * @brief Summary of one completed package creation transaction.
 */
struct WriteReport {
    /// Final installed SQ archive.
    std::filesystem::path archive;
    /// Number of packaged content files.
    std::uint64_t file_count{0};
    /// Combined source content size.
    std::uint64_t expanded_bytes{0};
    /// Canonical identity derived from package paths and hashes.
    std::string content_digest;
    /// SHA-256 of the complete ZIP archive bytes.
    std::string archive_sha256;
};

/**
 * @brief High-level deterministic SQ package writer.
 */
class PackageWriter {
public:
    /**
     * @brief Package one content directory without modifying the source.
     *
     * The destination is written through a temporary sibling, reopened, and
     * validated before it becomes visible. Existing destinations are refused.
     */
    [[nodiscard]]
    static Result<WriteReport> create_from_directory(
        const Manifest& manifest,
        const std::filesystem::path& content_directory,
        const std::filesystem::path& destination,
        const WriterOptions& options = {}
    ) noexcept;
};

}  // namespace squared::sq
