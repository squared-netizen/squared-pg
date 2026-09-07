// SPDX-License-Identifier: MIT
//
// sqcart — reference implementation of the Squared Cartridge Specification 1.0
//
// This header is the complete public surface of the library. Nothing else is
// supported for inclusion by consumers. No backend type (miniz, yyjson)
// appears here or in any header reachable from here; see the component
// specification, §4.2.
//
// Error channel: every fallible operation returns Result<T>. No exception
// escapes this boundary (component spec §5.2).
//
// Threading: Cartridge is not internally synchronised. Concurrent const
// access from multiple threads is safe; any non-const use requires external
// synchronisation.

#ifndef SQCART_SQCART_HPP
#define SQCART_SQCART_HPP

#include "sqcart/expected.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sqcart {

// ---------------------------------------------------------------------------
// Version
// ---------------------------------------------------------------------------

/// Container format version implemented by this library.
///
/// This reader implements exactly one format version. A manifest declaring
/// any other value is refused at parse time with `format_unsupported`; there
/// is no compatibility path for older formats. Format 2 made `tree` a
/// required envelope field (§5.1), which a format 1 manifest by definition
/// does not carry, so accepting one would mean reinstating the layout
/// guesswork the field exists to remove.
inline constexpr int kFormatVersion = 2;

/// Value the manifest's `format` member must carry (§5.1).
inline constexpr std::string_view kFormatTag = "squared-cartridge";

/// Reserved metadata directory, including the trailing separator.
inline constexpr std::string_view kMetaDir = "SQ-INF/";

/// Path of the required manifest entry.
inline constexpr std::string_view kManifestPath = "SQ-INF/manifest.json";

// ---------------------------------------------------------------------------
// Aliases
//
// Aliased because each crosses the public API boundary and names a domain
// concept. EntryPath in particular is not merely a string: a value of this
// type has been validated against format spec §3.3, and that distinction is
// the library's entire defence against path traversal.
// ---------------------------------------------------------------------------

using CartridgeId    = std::string;
using EntryPath      = std::string;
using ContentDigest  = std::array<std::byte, 32>;
using EntryDigestMap = std::map<EntryPath, ContentDigest>;

// ---------------------------------------------------------------------------
// Errors
// ---------------------------------------------------------------------------

/// Error codes as defined by format specification §13. Each maps onto an
/// engine error category (§2.4.8) via engine_category().
enum class ErrorCode : std::uint16_t {
    container_invalid,       ///< Not a conforming ZIP, or ZIP profile violation
    path_unsafe,             ///< §3.3 violation
    limit_exceeded,          ///< §3.4 violation
    manifest_missing,        ///< No SQ-INF/manifest.json
    manifest_malformed,      ///< Invalid JSON, or envelope violation
    format_unsupported,      ///< Unknown format_version
    feature_unsupported,     ///< Unimplemented requires_features token
    kind_invalid,            ///< Unknown kind, or foreign kind body present
    payload_missing,         ///< Manifest references an absent entry
    native_code_prohibited,  ///< §8 violation
    integrity_failed,        ///< Digest mismatch
    io_failed,               ///< Host filesystem or stream failure
    internal,                ///< Library invariant violated; report as a bug
};

/// Engine error categories, per Generator Architecture §2.4.8.
enum class EngineCategory : std::uint8_t {
    resource,
    validation,
    compatibility,
    filesystem,
    internal,
};

/// Render a digest as 64 lowercase hex characters.
///
/// Public because ContentDigest is opaque bytes and a consumer that computes
/// one has no other way to print, record or compare it. Promoted from the
/// internal header during the rewrite when the CLI needed it — a digest the
/// caller cannot render is a digest they cannot use.
[[nodiscard]] std::string to_hex(const ContentDigest& digest);

/// Parse 64 hex characters into a digest. Empty on malformed input.
[[nodiscard]] std::optional<ContentDigest> digest_from_hex(std::string_view hex);

[[nodiscard]] EngineCategory engine_category(ErrorCode code) noexcept;

[[nodiscard]] std::string_view to_string(ErrorCode code) noexcept;

/// A structured failure. Carries enough context for a Lua workflow to decide
/// a response without parsing text (§2.4.8, §2.6.12).
struct Error {
    ErrorCode                  code{ErrorCode::internal};
    std::string                message;
    std::optional<EntryPath>   entry;         ///< Offending entry, where one applies
    std::optional<CartridgeId> id;            ///< Cartridge identity, if the manifest parsed
    bool                       recoverable{false};

    [[nodiscard]] EngineCategory category() const noexcept {
        return engine_category(code);
    }
};

/// std::expected where the toolchain has C++23, a thin substitute otherwise.
/// See docs/developer/sqcart/expected.md.
template <class T>
using Result = expected<T, Error>;

// ---------------------------------------------------------------------------
// Kind
// ---------------------------------------------------------------------------
//
// `kind` is an opaque token. It was a closed enum through format 1; format 2
// makes it a validated identifier this library never enumerates.
//
// The change follows from §0.1. A closed enum obliges sqcart to know the set
// of roles in the ecosystem, and every one of those roles -- template, kit,
// package, plugin -- is a squared-pg concept. A framework runtime linking
// this library was getting the generator's vocabulary compiled into it, which
// is precisely the dependency §0.1 forbids.
//
// What a reader can still do without the enum is everything a reader should
// be doing: report the token, index by it, and hand it to a consumer that
// knows what it means. `kind` is a media type, not a schema selector.
//
// Kind tokens follow the identity grammar of §5.2 restricted to a single
// segment: `[a-z][a-z0-9_]*`. `asset-bundle` was spelled with a hyphen
// through format 1 and is `asset_bundle` in format 2, per the separator
// decision that also narrowed `id`.

/// Whether `token` is a well-formed kind (§5.2). Says nothing about whether
/// any consumer recognises it; that question is not this library's.
[[nodiscard]] bool valid_kind_token(std::string_view token) noexcept;

// ---------------------------------------------------------------------------
// Open options
// ---------------------------------------------------------------------------

/// How a symbolic link encountered in an exploded tree is treated.
///
/// Terminal workspaces use symlinks constantly -- a built binary linked back
/// into the source dir, a shared asset folder linked into several projects.
/// Refusing outright makes the tool unusable there; following blindly is an
/// exfiltration vector, because a link to ~/.ssh/id_rsa would pack that key's
/// contents into a distributable cartridge.
///
/// Format spec §3.5 says links MUST NOT be followed outside the cartridge
/// root and SHOULD be rejected. SHOULD, not MUST -- so skipping with a
/// report is conformant, and it is the default.
enum class SymlinkPolicy : std::uint8_t {
    /// Omit the link from the cartridge and record it. Never silent: the path
    /// is retrievable via skipped_symlinks() and validate() reports each one
    /// as a warning. This is the default.
    skip,
    /// Resolve links whose target stays inside the cartridge root, packing the
    /// target's bytes. Links pointing outside the root are still skipped and
    /// recorded, regardless of this setting -- that rule is not negotiable.
    follow_internal,
    /// Fail on the first link encountered. For pipelines that would rather
    /// stop than ship a cartridge missing files a human expected.
    reject,
};

enum class Conformance : std::uint8_t {
    strict,    ///< Every MUST enforced. Required before generation or execution.
    lenient,   ///< Stages 1-4 of §12.1 only. Inspection and authoring tools only.
};

/// Resource limits, format spec §3.4. Defaults are the specified defaults; a
/// host may raise them but must not disable them.
struct Limits {
    std::uint64_t max_manifest_bytes     = 1u  << 20;   ///<  1 MiB
    std::uint64_t max_meta_file_bytes    = 4u  << 20;   ///<  4 MiB
    std::uint32_t max_path_bytes         = 1024;
    std::uint32_t max_path_depth         = 32;
    std::uint32_t max_entries            = 65'536;
    std::uint64_t max_total_uncompressed = 2ull << 30;  ///<  2 GiB
    std::uint32_t max_entry_ratio        = 200;
    std::uint32_t max_archive_ratio      = 100;
};

struct OpenOptions {
    Conformance conformance      = Conformance::strict;
    Limits      limits           = {};
    /// Verify SQ-INF/hashes.json against entry contents during open. Costs a
    /// full read of every entry; off by default.
    bool        verify_integrity = false;
    /// Feature tokens this consumer implements, checked against the manifest's
    /// requires_features (§5.4). Empty means "v1.0 baseline only".
    std::span<const std::string_view> supported_features = {};
    /// Exploded form only; archives cannot contain links (§3.1).
    SymlinkPolicy symlinks = SymlinkPolicy::skip;
    /// Refuse a cartridge containing .so/.dll/.a/.o and friends.
    ///
    /// Off by default. A cartridge is the ecosystem's distribution unit --
    /// kits ship bridge code, packages ship libraries -- so refusing to open
    /// one that contains compiled artefacts would defeat the format. What
    /// must never happen is a runtime *loading* that code from an unverified
    /// cartridge, which is the runtime's decision and the signature model's
    /// job (§10), not the container reader's.
    ///
    /// Set this only if you have a real no-native-code policy. validate()
    /// reports native entries as warnings either way, so they are never
    /// invisible.
    bool reject_native_code = false;
};

/// Classification of a symbolic link found while walking a directory.
enum class LinkVerdict : std::uint8_t {
    use_target,   ///< Resolves to a regular file inside the root; safe to read.
    skip,         ///< Escapes the root, dangles, or points at a directory.
    error,        ///< Policy is reject, or the link could not be inspected.
};

/// Decide what to do with `link` under `policy`, given the cartridge `root`.
///
/// Public because any tool building a cartridge from a directory needs the
/// same answer the reader uses, and a second implementation would be a second
/// chance to get the escape rule wrong. `why` receives a human-readable
/// reason suitable for reporting.
[[nodiscard]] LinkVerdict classify_symlink(const std::filesystem::path& root,
                                           const std::filesystem::path& link,
                                           SymlinkPolicy policy, std::string& why);

// ---------------------------------------------------------------------------
// Manifest
// ---------------------------------------------------------------------------

struct CompatRef {
    std::string id;
    std::string version;   ///< Range expression, §5.3
};

struct Author {
    std::string name;
    std::optional<std::string> email;
    std::optional<std::string> url;
};


/// Parsed manifest: the envelope this library understands, plus consumer
/// sections it carries without opening.
///
/// Format 1 modelled six kind bodies here -- template, kit, package,
/// asset-bundle, plugin, cartridge -- with a typed struct and an accessor
/// apiece. Every field in every one of them was a squared-pg or Squared
/// framework concept, which made §0.1's independence requirement false in
/// code: a framework runtime linking this library got `integration_areas`
/// and `project_types` compiled into it.
///
/// Format 2 replaces them with `consumers` (§5.6): a namespaced object whose
/// values this library validates for shape and never interprets. The model is
/// not new, only generalised -- `requires_capabilities` has always been an
/// uninterpreted list this library carries on a consumer's behalf.
class Manifest {
public:
    [[nodiscard]] const CartridgeId& id() const noexcept;

    /// The `kind` token, verbatim. An opaque identifier (§5.2); this library
    /// does not enumerate legal values and a token it has never seen is not
    /// an error. See the note above `valid_kind_token`.
    [[nodiscard]] std::string_view   kind() const noexcept;
    [[nodiscard]] const std::string& version() const noexcept;
    [[nodiscard]] int                format_version() const noexcept;

    /// Payload root, relative to the cartridge root (§5.1).
    ///
    /// Either "." (payload begins at the cartridge root) or a relative
    /// directory path with a trailing '/'. Never empty: the field is
    /// required, so a parsed manifest always has one.
    ///
    /// `SQ-INF/` is not payload regardless of this value, including when it
    /// is "." — the reserved directory is excluded by §4, not by the payload
    /// root happening to point elsewhere.
    [[nodiscard]] const EntryPath&   tree() const noexcept;

    [[nodiscard]] std::optional<std::string_view> title() const noexcept;
    [[nodiscard]] std::optional<std::string_view> description() const noexcept;
    [[nodiscard]] std::optional<std::string_view> license() const noexcept;
    [[nodiscard]] std::span<const std::string>    requires_features() const noexcept;

    // No engine() and no requires_capabilities().
    //
    // Both were envelope fields through format 1 and the first half of
    // format 2, and both were consumer data wearing envelope clothes:
    //
    //   "engine": { "id": "squared-pg", "version": ">=0.1.0 <0.2.0" }
    //
    // `engine.id` names the tool the field is addressed to -- which is
    // exactly what a `consumers` key already says, so the member was
    // restating the section it should have been inside. And every
    // `requires_capabilities` token that has ever been written is in
    // squared-pg's namespace (`capability.project.plan@^1.0`), which this
    // library could shape-check but never resolve.
    //
    // They live at `consumers.<id>.requires.engine` and
    // `consumers.<id>.requires.capabilities` now. The consumer that owns the
    // namespace is the only party that can act on either.
    //
    // requires_features stays, and the contrast is the point: it names
    // *container* capabilities -- a compression method, a signature scheme --
    // that this reader either implements or does not. It guards the reader
    // and fails at open. The other two guarded a consumer and could only ever
    // fail later, somewhere else, in code this library does not own.

    [[nodiscard]] std::span<const Author>         authors() const noexcept;

    /// Consumer identities present in `consumers` (§5.6), sorted.
    ///
    /// Enumerable so a tool can report what a cartridge carries and for whom
    /// without recognising any of it. This is the honest form of what a
    /// `kind`-driven schema switch used to do badly: it answers "who is this
    /// addressed to" without pretending to answer "what does it say".
    [[nodiscard]] std::vector<std::string_view> consumers() const;

    /// The section addressed to `id`, as JSON text, or nullopt if absent.
    ///
    /// Returned as text rather than a parsed structure because this library
    /// has no types for it and acquiring some would re-import the coupling
    /// format 2 removed. The consumer parses it with its own JSON reader --
    /// squared-pg already vendors yyjson and already does exactly this for
    /// its extension fields.
    ///
    /// The text is a re-serialisation, not a slice of the original: key order
    /// is preserved, insignificant whitespace is not. Callers needing the
    /// original bytes have raw_json().
    ///
    /// Absent is not an error and never means empty-by-default. A consumer
    /// finding no section for itself is looking at a cartridge that was not
    /// addressed to it, which it must report rather than resolve.
    [[nodiscard]] std::optional<std::string_view> consumer(std::string_view id) const noexcept;

    /// Raw JSON of the manifest, for consumers needing fields this version
    /// does not model. Unrecognised members are preserved verbatim (§5.4).
    [[nodiscard]] std::string_view raw_json() const noexcept;

    /// Parse a manifest in isolation, without a surrounding container.
    [[nodiscard]] static Result<Manifest> parse(std::string_view json,
                                                const Limits& limits = {});

    Manifest(Manifest&&) noexcept;
    Manifest& operator=(Manifest&&) noexcept;
    ~Manifest();

private:
    Manifest();
    struct Impl;
    std::unique_ptr<Impl> impl_;
    friend class Cartridge;
};

// ---------------------------------------------------------------------------
// Cartridge
// ---------------------------------------------------------------------------

struct EntryInfo {
    EntryPath     path;
    std::uint64_t uncompressed_size{0};
    std::uint64_t compressed_size{0};
    std::uint32_t crc32{0};
    bool          stored{false};   ///< true = method 0, false = method 8
};

/// An opened cartridge, archived or exploded. Move-only; owns its backend.
///
/// Facade over the container codec, JSON parser, path validator and limit
/// accountant (component spec §5.1).
class Cartridge {
public:
    /// Open a .sq archive or an exploded directory. The form is detected;
    /// callers need not care which they have (format spec §3.5).
    [[nodiscard]] static Result<Cartridge> open(const std::filesystem::path& path,
                                                const OpenOptions& options = {});

    /// Open from a memory image. The span must outlive the Cartridge; the
    /// library does not copy it.
    [[nodiscard]] static Result<Cartridge> open_memory(std::span<const std::byte> bytes,
                                                       const OpenOptions& options = {});

    [[nodiscard]] const Manifest& manifest() const noexcept;
    [[nodiscard]] bool            exploded() const noexcept;

    [[nodiscard]] std::span<const EntryInfo> entries() const noexcept;

    /// Paths of symbolic links omitted under SymlinkPolicy::skip, relative to
    /// the cartridge root, in the order encountered.
    ///
    /// Exists so that skipping is never silent. A caller that ignores this is
    /// making that choice explicitly; validate() surfaces the same list as
    /// warnings for callers that do not.
    [[nodiscard]] std::span<const EntryPath> skipped_symlinks() const noexcept;
    [[nodiscard]] bool contains(std::string_view path) const noexcept;
    /// Locate an entry by exact path. Empty when absent. The returned
    /// reference is valid for the lifetime of the Cartridge.
    [[nodiscard]] std::optional<std::reference_wrapper<const EntryInfo>>
    find(std::string_view path) const noexcept;

    /// Read an entry in full. Fails rather than truncating if the entry
    /// exceeds the configured limits.
    [[nodiscard]] Result<std::vector<std::byte>> read(std::string_view path) const;

    /// Read into caller-provided storage. Returns bytes written. Fails if the
    /// buffer is too small; never writes a partial result.
    [[nodiscard]] Result<std::size_t> read_into(std::string_view path,
                                                std::span<std::byte> out) const;

    /// Content digest per format spec §9.2: SHA-256 over sorted
    /// (path, entry-digest) lines, excluding hashes.json and signatures/.
    /// Stable across compressors, so two independently built writers agree.
    [[nodiscard]] Result<ContentDigest> content_digest() const;

    /// Per-entry digests from SQ-INF/hashes.json, if present.
    [[nodiscard]] Result<EntryDigestMap> declared_digests() const;

    /// Recompute digests and compare against declared_digests().
    [[nodiscard]] Result<void> verify_integrity() const;

    /// Materialise the payload into a destination directory. Every path is
    /// re-validated at write time; the function refuses to write outside dest
    /// under any circumstance. Does not overwrite existing files unless
    /// overwrite is true.
    [[nodiscard]] Result<void> extract_to(const std::filesystem::path& dest,
                                          bool overwrite = false) const;

    Cartridge(Cartridge&&) noexcept;
    Cartridge& operator=(Cartridge&&) noexcept;
    ~Cartridge();

    Cartridge(const Cartridge&)            = delete;
    Cartridge& operator=(const Cartridge&) = delete;

private:
    Cartridge();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

enum class Severity : std::uint8_t { info, warning, error };

struct Diagnostic {
    Severity                 severity{Severity::info};
    std::string              message;
    std::optional<EntryPath> entry;
};

struct ValidationReport {
    bool                    conforming{false};
    std::vector<Diagnostic> diagnostics;
};

/// Run the full §12.1 pipeline. Stages 1-8 only; compatibility resolution
/// (stage 9) needs a resolution context and belongs to the engine services,
/// not here.
[[nodiscard]] ValidationReport validate(const Cartridge& cartridge);

/// Validate a single entry path against format spec §3.3, independently of
/// any container. Exposed because callers building cartridges want the check
/// before they have one.
[[nodiscard]] Result<EntryPath> validate_entry_path(std::string_view path,
                                                    const Limits& limits = {});

// ---------------------------------------------------------------------------
// Writer  (SQCART_ENABLE_WRITER)
//
// Absent from runtime builds by design: a launcher or shipped game carries no
// compressor and no manifest serialiser (component spec §4.1).
// ---------------------------------------------------------------------------

#ifdef SQCART_ENABLE_WRITER

struct WriteOptions {
    /// Canonical output per format spec §9.3: sorted entries, manifest first
    /// and Stored, fixed timestamps, no directory entries. On by default;
    /// determinism should not require remembering a flag.
    bool          canonical    = true;
    /// Fixed entry timestamp as a Unix epoch. Default is the ZIP epoch.
    std::int64_t  timestamp    = 315'532'800;   ///< 1980-01-01T00:00:00Z
    /// 0 = store everything, 1-9 = deflate level.
    int           compression  = 6;
    /// Emit SQ-INF/hashes.json.
    bool          write_hashes = true;
    /// Applies to add_tree(). Same reasoning as OpenOptions::symlinks.
    SymlinkPolicy symlinks     = SymlinkPolicy::skip;
    Limits        limits       = {};
};

class Writer {
public:
    [[nodiscard]] static Result<Writer> create(const std::filesystem::path& out,
                                               const WriteOptions& options = {});

    /// Set the manifest. Required before finish(); validated on set.
    [[nodiscard]] Result<void> set_manifest(std::string_view json);

    [[nodiscard]] Result<void> add(std::string_view path,
                                   std::span<const std::byte> content);
    [[nodiscard]] Result<void> add_file(std::string_view path,
                                        const std::filesystem::path& source);

    /// Add every payload entry from an exploded directory.
    [[nodiscard]] Result<void> add_tree(const std::filesystem::path& root);

    /// Links add_tree() omitted under SymlinkPolicy::skip, relative to the
    /// tree root. Check this after add_tree(): a caller that ignores it is
    /// choosing to ship a cartridge missing files the source directory had.
    [[nodiscard]] std::span<const EntryPath> skipped_symlinks() const noexcept;

    /// Write the archive. Returns the content digest of the result. The
    /// Writer is spent afterwards.
    [[nodiscard]] Result<ContentDigest> finish() &&;

    Writer(Writer&&) noexcept;
    Writer& operator=(Writer&&) noexcept;
    ~Writer();

    Writer(const Writer&)            = delete;
    Writer& operator=(const Writer&) = delete;

private:
    Writer();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#endif  // SQCART_ENABLE_WRITER

}  // namespace sqcart

#endif  // SQCART_SQCART_HPP
