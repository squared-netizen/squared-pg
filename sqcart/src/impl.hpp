// SPDX-License-Identifier: MIT
//
// Internal implementation header for the sqcart library. NOT part of the
// public interface. It exists so the translation units implementing Cartridge
// and Manifest methods share the pimpl definitions and internal helpers.
//
// No backend type leaks into a public header (FR-CON-3): miniz's
// mz_zip_archive appears here, in a private header, and nowhere reachable
// from sqcart/include/.

#ifndef SQCART_IMPL_HPP
#define SQCART_IMPL_HPP

#include "sqcart/sqcart.hpp"

#include <miniz.h>

namespace sqcart {

// ---------------------------------------------------------------------------
// Manifest::Impl
// ---------------------------------------------------------------------------

struct Manifest::Impl {
    CartridgeId                id;
    Kind                       kind{Kind::cartridge};
    std::string                version;
    int                        format_version{1};

    std::optional<std::string> title;
    std::optional<std::string> description;
    std::optional<std::string> license;
    std::optional<CompatRef>   engine;
    std::vector<std::string>   requires_features;
    std::vector<Author>        authors;

    std::unique_ptr<CartridgeBody> cartridge;
    std::unique_ptr<TemplateBody>  template_;
    std::unique_ptr<KitBody>       kit;
    std::unique_ptr<PackageBody>   package;
    std::unique_ptr<AssetsBody>    assets;
    std::unique_ptr<PluginBody>    plugin;

    std::string raw_json;
};

namespace detail {

/// Which physical form a cartridge takes. The two share one entry index and
/// one read path selector; everything above `read_entry` is form-agnostic
/// (format spec §3.5 — the forms are semantically equivalent).
enum class Form : std::uint8_t {
    archive_file,   ///< .sq on disk, miniz owns a FILE*
    archive_memory, ///< .sq in a caller-owned span
    exploded,       ///< directory tree containing SQ-INF/manifest.json
};

/// The open-cartridge state.
///
/// Separate from Cartridge::Impl because Impl is a private nested type, while
/// the cross-TU helpers (read_entry, find_entry, digest, validate, extract)
/// are free functions and cannot name it. Member functions pass this through.
struct CartridgeState {
    Form                     form{Form::archive_file};
    Conformance              conformance{Conformance::strict};
    Limits                   limits{};
    std::vector<std::string> supported_features;

    /// Engaged once the manifest has been read and parsed during open().
    std::optional<Manifest> manifest;

    /// Every entry, including those under SQ-INF/. Order is stable per
    /// FR-ENTRY-1: central-directory order for archives, lexicographic for
    /// exploded trees.
    std::vector<EntryInfo> entries;

    // --- archived forms ---------------------------------------------------
    mz_zip_archive             archive{};
    bool                       archive_inited{false};
    std::span<const std::byte> memory{};   ///< archive_memory: caller-owned

    // --- exploded form ----------------------------------------------------
    std::filesystem::path  root;
    SymlinkPolicy          symlinks{SymlinkPolicy::skip};
    bool                   reject_native_code{false};
    std::vector<EntryPath> skipped_symlinks;

    [[nodiscard]] bool is_archive() const noexcept
    {
        return form == Form::archive_file || form == Form::archive_memory;
    }

    CartridgeState() = default;
    ~CartridgeState();

    CartridgeState(const CartridgeState&)            = delete;
    CartridgeState& operator=(const CartridgeState&) = delete;
    CartridgeState(CartridgeState&&)                 = delete;
    CartridgeState& operator=(CartridgeState&&)      = delete;
};

}  // namespace detail

struct Cartridge::Impl {
    detail::CartridgeState st;
    ~Impl();
};

// ---------------------------------------------------------------------------
// Shared internal helpers
//
// These cross translation units but never cross the public API.
// ---------------------------------------------------------------------------

/// Validate one path against §3.3 and the limit set, returning the
/// canonicalised path. Never throws; failures are Error values.
Result<EntryPath> validate_entry_path_impl(std::string_view path, const Limits& limits);

/// Case- and NFC-insensitive collision key for FR-PATH-2. Two entries whose
/// keys are equal collide even when their exact paths differ.
std::string collision_key(std::string_view path);

/// True when an entry path names loadable native code, format spec §8.
bool is_native_code_path(std::string_view path) noexcept;

/// Locate an entry by exact path. Empty when absent.
std::optional<std::reference_wrapper<const EntryInfo>>
find_entry(const detail::CartridgeState& st, std::string_view path) noexcept;

/// Read the full uncompressed bytes of one entry, enforcing entry limits and
/// the CRC check. Never returns a truncated buffer (FR-ENTRY-3).
Result<std::vector<std::byte>> read_entry(const detail::CartridgeState& st,
                                          const EntryInfo&              info);

/// Manifest bytes -> parsed Manifest, applying the manifest size limit first.
Result<Manifest> parse_manifest_entry(const detail::CartridgeState& st, const Limits& limits);

/// SHA-256 of a byte range.
ContentDigest sha256(std::span<const std::byte> bytes);

/// The one implementation of format spec §9.2.
///
/// FR-WRITE-7 requires reader and writer to share this, not to maintain two
/// implementations that happen to agree today. Cartridge::content_digest()
/// and Writer::finish() both feed the same accumulator; milestone M0's
/// cross-binary reproducibility check is only meaningful because of that.
class DigestAccumulator {
public:
    /// Record one entry. Entries excluded by participates_in_digest() are
    /// ignored here rather than at the call site, so no caller can forget.
    void add(std::string_view path, std::span<const std::byte> content);

    /// Sort by path, emit "<path> LF <hex> LF" per entry, hash the stream.
    [[nodiscard]] ContentDigest finish();

private:
    std::vector<std::pair<std::string, ContentDigest>> entries_;
};

/// True when an entry participates in the §9.2 content digest: everything
/// except SQ-INF/hashes.json and SQ-INF/signatures/**.
bool participates_in_digest(std::string_view path) noexcept;

}  // namespace sqcart

#endif  // SQCART_IMPL_HPP
