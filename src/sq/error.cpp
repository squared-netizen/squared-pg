#include <squared/sq/error.hpp>
#include <squared/sq/manifest.hpp>
#include <squared/sq/validation.hpp>

namespace squared::sq {

const char* to_string(ErrorCode code) noexcept
{
    switch (code) {
    case ErrorCode::None: return "none";
    case ErrorCode::FileNotFound: return "file_not_found";
    case ErrorCode::PermissionDenied: return "permission_denied";
    case ErrorCode::InvalidArchive: return "invalid_archive";
    case ErrorCode::UnsupportedArchive: return "unsupported_archive";
    case ErrorCode::InvalidManifest: return "invalid_manifest";
    case ErrorCode::UnsupportedFormat: return "unsupported_format";
    case ErrorCode::UnsupportedKind: return "unsupported_kind";
    case ErrorCode::UnsafePath: return "unsafe_path";
    case ErrorCode::DuplicatePath: return "duplicate_path";
    case ErrorCode::ResourceLimit: return "resource_limit";
    case ErrorCode::ChecksumMismatch: return "checksum_mismatch";
    case ErrorCode::ExtractionFailed: return "extraction_failed";
    case ErrorCode::TransactionFailed: return "transaction_failed";
    case ErrorCode::Cancelled: return "cancelled";
    case ErrorCode::InternalError: return "internal_error";
    }
    return "internal_error";
}

const char* to_string(Phase phase) noexcept
{
    switch (phase) {
    case Phase::Opening: return "opening";
    case Phase::Inspecting: return "inspecting";
    case Phase::Validating: return "validating";
    case Phase::Extracting: return "extracting";
    case Phase::Writing: return "writing";
    case Phase::Committing: return "committing";
    }
    return "opening";
}

const char* to_string(PackageKind kind) noexcept
{
    switch (kind) {
    case PackageKind::Template: return "template";
    case PackageKind::Module: return "module";
    case PackageKind::Kit: return "kit";
    case PackageKind::Cartridge: return "cartridge";
    }
    return "template";
}

bool ValidationReport::valid() const noexcept
{
    for (const auto& issue : issues) {
        if (issue.severity == IssueSeverity::Error) return false;
    }
    return true;
}

}  // namespace squared::sq
