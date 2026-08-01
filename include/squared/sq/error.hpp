#pragma once

#include <filesystem>
#include <string>

namespace squared::sq {

/**
 * @brief Stable machine-readable error codes returned by the SQ API.
 */
enum class ErrorCode {
    None,               ///< No failure.
    FileNotFound,       ///< Required filesystem object is absent.
    PermissionDenied,   ///< Host filesystem rejected access.
    InvalidArchive,     ///< ZIP structure is malformed.
    UnsupportedArchive, ///< Valid ZIP feature is outside SQ version 0.
    InvalidManifest,    ///< Strict SQ manifest validation failed.
    UnsupportedFormat,  ///< SQ envelope version is unsupported.
    UnsupportedKind,    ///< Semantic package kind is unsupported.
    UnsafePath,         ///< Archive path violates portability or safety rules.
    DuplicatePath,      ///< Exact or case-folded archive paths collide.
    ResourceLimit,      ///< Caller-owned resource ceiling was exceeded.
    ChecksumMismatch,   ///< Inventory and streamed content differ.
    ExtractionFailed,   ///< Content could not be streamed to staging.
    TransactionFailed,  ///< Staging or atomic commit failed.
    Cancelled,          ///< Caller requested cooperative cancellation.
    InternalError       ///< Unexpected implementation failure.
};

/**
 * @brief Operation phase in which an SQ error occurred.
 */
enum class Phase {
    Opening,    ///< Opening the archive container.
    Inspecting, ///< Reading metadata and the base manifest.
    Validating, ///< Enforcing SQ policy and content identity.
    Extracting, ///< Streaming content into private staging.
    Writing,    ///< Creating a new archive.
    Committing  ///< Publishing staged output atomically.
};

/**
 * @brief Structured failure information for native and Lua callers.
 *
 * Error values own all strings and paths. They remain valid independently of
 * the Package or writer operation that produced them.
 */
struct Error {
    /// Stable machine-readable failure category.
    ErrorCode code{ErrorCode::None};
    /// Phase that detected the failure.
    Phase phase{Phase::Opening};
    /// Human-readable diagnostic; callers must not parse it for policy.
    std::string message;
    /// Portable path inside the SQ archive, when applicable.
    std::string archive_path;
    /// Host filesystem path involved in the failure, when applicable.
    std::filesystem::path filesystem_path;

    /**
     * @brief Return true when this object represents a failure.
     */
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return code != ErrorCode::None;
    }
};

/**
 * @brief Return the stable lowercase identifier for an error code.
 */
[[nodiscard]] const char* to_string(ErrorCode code) noexcept;

/**
 * @brief Return the stable lowercase identifier for an operation phase.
 */
[[nodiscard]] const char* to_string(Phase phase) noexcept;

}  // namespace squared::sq
