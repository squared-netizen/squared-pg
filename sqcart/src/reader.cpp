// SPDX-License-Identifier: MIT
//
// Cartridge open/read facade, format spec §3.1-§3.5.
//
// Both container forms — archived (.sq ZIP) and exploded (directory tree) —
// build the same entry index and share every layer above read_entry(). That
// is the point of format spec §3.5: the forms are semantically equivalent,
// so exactly one code path may know which is which.
//
// Raw-pointer policy: miniz's C API is called at this boundary and its
// mz_zip_archive is a value member. sqcart code never stores or returns a raw
// owning pointer; reads target caller-owned buffers via
// mz_zip_reader_extract_to_mem rather than the void*-returning heap helper.

#include "impl.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <miniz.h>

namespace sqcart {

using detail::CartridgeState;
using detail::Form;

namespace {

/// Whether this open was asked to refuse native code. Off by default.
bool options_reject_native(const CartridgeState& st)
{
    return st.reject_native_code;
}

bool zip_ok(mz_bool ok)
{
    return ok != 0;
}

Error make_error(ErrorCode code, std::string msg, std::optional<EntryPath> entry = std::nullopt,
                 bool recoverable = false)
{
    return Error{code, std::move(msg), std::move(entry), std::nullopt, recoverable};
}

// --------------------------------------------------------------------------
// Index construction, shared by all three forms
// --------------------------------------------------------------------------

/// Apply the checks every entry must survive regardless of container form:
/// path safety (§3.3), collision detection (FR-PATH-2), native-code
/// prohibition (§8, strict only), and the entry-count limit (§3.4).
///
/// The original draft duplicated ~40 lines of this between the file and
/// memory open paths, which is how the two drifted; there is now one copy.
Result<void> finalise_index(CartridgeState& st)
{
    if (st.entries.size() > st.limits.max_entries) {
        return unexpected(make_error(ErrorCode::limit_exceeded, "cartridge exceeds max_entries"));
    }

    // FR-PATH-2: reject case-only and normalisation-only collisions, so a
    // case-sensitive logical model materialises safely onto a
    // case-insensitive host (Android, macOS).
    std::vector<std::pair<std::string, const EntryInfo*>> keys;
    keys.reserve(st.entries.size());
    for (const auto& e : st.entries) {
        keys.emplace_back(collision_key(e.path), &e);
    }
    std::sort(keys.begin(), keys.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    for (std::size_t i = 1; i < keys.size(); ++i) {
        if (keys[i - 1].first == keys[i].first) {
            return unexpected(make_error(ErrorCode::path_unsafe,
                                         "entry paths collide under case or normalisation folding",
                                         keys[i].second->path));
        }
    }

    // FR-LIM-3: evaluate the whole-archive ratio from declared sizes, before
    // any decompression is attempted.
    std::uint64_t total_uncomp = 0;
    std::uint64_t total_comp   = 0;
    for (const auto& e : st.entries) {
        total_uncomp += e.uncompressed_size;
        total_comp += e.compressed_size;
        if (total_uncomp > st.limits.max_total_uncompressed) {
            return unexpected(make_error(ErrorCode::limit_exceeded,
                                         "cartridge exceeds max_total_uncompressed", e.path));
        }
    }
    if (total_comp > 0 && st.limits.max_archive_ratio > 0) {
        const std::uint64_t ceiling = total_comp * st.limits.max_archive_ratio;
        if (total_uncomp > ceiling) {
            return unexpected(make_error(ErrorCode::limit_exceeded,
                                         "cartridge exceeds max_archive_ratio"));
        }
    }

    const bool has_manifest =
        std::any_of(st.entries.begin(), st.entries.end(),
                    [](const EntryInfo& e) { return e.path == kManifestPath; });
    if (!has_manifest) {
        return unexpected(
            make_error(ErrorCode::manifest_missing, "no SQ-INF/manifest.json entry"));
    }

    // Native code is CONTAINED freely and LOADED never. Those are different
    // rules and the first draft conflated them.
    //
    // A cartridge is the distribution unit for the whole ecosystem: kits ship
    // bridge code, packages ship libraries, an asset bundle made from a build
    // directory picks up object files. Refusing to open any of those defeats
    // the format's purpose. Containment is a property of an archive; loading
    // is a decision a runtime makes, and belongs to the runtime plus the
    // signature model (§10), not to the container reader.
    //
    // The check survives as an opt-in for callers with a genuine
    // no-native-code policy, and as a validate() warning so it is visible.
    if (options_reject_native(st)) {
        for (const auto& e : st.entries) {
            if (is_native_code_path(e.path)) {
                return unexpected(make_error(ErrorCode::native_code_prohibited,
                                             "cartridge contains native code and the caller "
                                             "requested rejection", e.path));
            }
        }
    }
    return {};
}

/// Build the entry index for an already-initialised miniz archive.
Result<void> index_archive(CartridgeState& st)
{
    const mz_uint n = mz_zip_reader_get_num_files(&st.archive);
    if (n > st.limits.max_entries) {
        return unexpected(make_error(ErrorCode::limit_exceeded, "archive exceeds max_entries"));
    }

    st.entries.reserve(n);
    mz_zip_archive_file_stat fst;
    bool first_payload_seen = false;

    for (mz_uint i = 0; i < n; ++i) {
        if (!zip_ok(mz_zip_reader_file_stat(&st.archive, i, &fst))) {
            return unexpected(
                make_error(ErrorCode::container_invalid, "archive entry stat failed"));
        }
        if (fst.m_is_directory) {
            continue;  // Directory entries are not payload (§9.3).
        }

        // §3.1: only Stored (0) and Deflate (8) are permitted.
        if (fst.m_method != 0 && fst.m_method != 8) {
            return unexpected(make_error(ErrorCode::container_invalid,
                                         "entry uses a compression method outside the v1 profile",
                                         std::string(fst.m_filename)));
        }

        EntryInfo e;
        e.path              = std::string(fst.m_filename);
        e.uncompressed_size = static_cast<std::uint64_t>(fst.m_uncomp_size);
        e.compressed_size   = static_cast<std::uint64_t>(fst.m_comp_size);
        e.crc32             = static_cast<std::uint32_t>(fst.m_crc32);
        e.stored            = (fst.m_method == 0);

        auto vpath = validate_entry_path_impl(e.path, st.limits);
        if (!vpath) {
            auto err  = vpath.error();
            err.entry = e.path;
            return unexpected(err);
        }
        e.path = std::move(*vpath);

        // FR-OPEN-3: the manifest must be the first entry, Stored, so a
        // reader can parse it from the head of the stream (§3.2).
        if (!first_payload_seen) {
            first_payload_seen = true;
            if (st.conformance == Conformance::strict) {
                if (e.path != kManifestPath) {
                    return unexpected(make_error(ErrorCode::container_invalid,
                                                 "SQ-INF/manifest.json is not the first entry",
                                                 e.path));
                }
                if (!e.stored) {
                    return unexpected(make_error(ErrorCode::container_invalid,
                                                 "SQ-INF/manifest.json is not stored uncompressed",
                                                 e.path));
                }
            }
        }

        st.entries.push_back(std::move(e));
    }

    return finalise_index(st);
}

/// Build the entry index for an exploded cartridge by walking the tree.
///
/// FR-ENTRY-1 fixes the order here as lexicographic, which differs from an
/// archive's central-directory order for the same logical cartridge — a
/// divergence recorded as open issue 2 in the functional spec.
Result<void> index_exploded(CartridgeState& st)
{
    std::error_code ec;
    std::vector<std::string> rel_paths;

    std::filesystem::recursive_directory_iterator it(
        st.root, std::filesystem::directory_options::none, ec);
    if (ec) {
        return unexpected(make_error(ErrorCode::io_failed, "cannot walk exploded cartridge root"));
    }
    const std::filesystem::recursive_directory_iterator end;

    for (; it != end; it.increment(ec)) {
        if (ec) {
            return unexpected(make_error(ErrorCode::io_failed, "directory walk failed"));
        }
        // Symbolic links: classified, never silently dropped. See
        // src/symlink.cpp for why refusing outright was wrong and why a link
        // escaping the root is refused regardless of policy.
        if (it->is_symlink()) {
            std::string why;
            const auto  verdict = classify_symlink(st.root, it->path(), st.symlinks, why);
            // lexically_relative, not relative(): the latter canonicalises
            // and would resolve the link, naming its target instead of itself.
            const std::string rel_str =
                it->path().lexically_relative(st.root).generic_string();

            if (verdict == LinkVerdict::error) {
                return unexpected(make_error(ErrorCode::path_unsafe, why, rel_str));
            }
            if (verdict == LinkVerdict::skip) {
                st.skipped_symlinks.push_back(rel_str);
                continue;
            }
            // use_target: fall through and index it as an ordinary file.
        }
        if (!it->is_regular_file(ec)) {
            continue;
        }
        // lexically_relative, not relative(): relative() canonicalises, so a
        // followed symbolic link would be indexed under its target's name --
        // producing a duplicate entry when the target is itself in the tree.
        // A followed link is an entry at its own path whose bytes come from
        // the target, which is what reading through the link already does.
        auto rel = it->path().lexically_relative(st.root);
        if (rel.empty()) {
            return unexpected(make_error(ErrorCode::io_failed, "cannot relativise entry path"));
        }
        rel_paths.push_back(rel.generic_string());
        if (rel_paths.size() > st.limits.max_entries) {
            return unexpected(
                make_error(ErrorCode::limit_exceeded, "exploded tree exceeds max_entries"));
        }
    }

    std::sort(rel_paths.begin(), rel_paths.end());

    st.entries.reserve(rel_paths.size());
    for (auto& rel : rel_paths) {
        auto vpath = validate_entry_path_impl(rel, st.limits);
        if (!vpath) {
            auto err  = vpath.error();
            err.entry = rel;
            return unexpected(err);
        }

        EntryInfo e;
        e.path              = std::move(*vpath);
        // file_size follows links, which is what we want: a followed link's
        // size is its target's. Classification already rejected dangling ones.
        const auto sz = std::filesystem::file_size(st.root / e.path, ec);
        if (ec) {
            return unexpected(make_error(ErrorCode::io_failed, "cannot stat entry", e.path));
        }
        e.uncompressed_size = static_cast<std::uint64_t>(sz);
        e.compressed_size   = e.uncompressed_size;  // Nothing is compressed on disk.
        e.crc32             = 0;                    // No stored CRC in exploded form.
        e.stored            = true;
        st.entries.push_back(std::move(e));
    }

    return finalise_index(st);
}

Result<void> check_features(const CartridgeState& st)
{
    const auto& features = st.manifest->requires_features();
    for (const auto& token : features) {
        const bool supported =
            std::find(st.supported_features.begin(), st.supported_features.end(), token) !=
            st.supported_features.end();
        if (!supported) {
            // FR-MAN-8: a manifest whose critical requirements are not fully
            // met is not partially honoured.
            return unexpected(Error{ErrorCode::feature_unsupported,
                                    "cartridge requires unimplemented feature: " + token,
                                    std::nullopt, st.manifest->id(), true});
        }
    }
    return {};
}

/// Stage 7 of §12.1: every path the manifest names must exist.
Result<void> check_declared_payload(const CartridgeState& st)
{
    const Manifest& m = *st.manifest;

    auto require = [&](const EntryPath& p, const char* what) -> Result<void> {
        if (p.empty() || find_entry(st, p)) {
            return {};
        }
        return unexpected(Error{ErrorCode::payload_missing,
                                std::string("manifest references a missing ") + what, p, m.id(),
                                false});
    };

    if (auto body = m.as_cartridge()) {
        if (auto r = require(body->get().entry.module, "entry module"); !r) {
            return r;
        }
        for (const auto& [target, path] : body->get().entry.bytecode_cache) {
            if (auto r = require(path, "bytecode cache entry"); !r) {
                return r;
            }
        }
    }
    if (auto body = m.as_assets()) {
        for (const auto& asset : body->get().entries) {
            if (auto r = require(asset.path, "asset"); !r) {
                return r;
            }
        }
    }
    if (auto body = m.as_plugin()) {
        if (auto r = require(body->get().entry, "plugin entry"); !r) {
            return r;
        }
    }
    return {};
}

/// Stages 4-8 of §12.1, common to every form once the index exists.
Result<void> finish_open(CartridgeState& st)
{
    auto m = parse_manifest_entry(st, st.limits);
    if (!m) {
        return unexpected(m.error());
    }
    st.manifest.emplace(std::move(*m));

    // Lenient conformance stops after the manifest is known to be
    // well-formed (FR-OPEN-7); later violations surface via validate().
    if (st.conformance == Conformance::lenient) {
        return {};
    }

    if (auto r = check_features(st); !r) {
        return r;
    }
    if (auto r = check_declared_payload(st); !r) {
        return r;
    }

    if (st.limits.max_meta_file_bytes > 0) {
        for (const auto& e : st.entries) {
            if (e.path.starts_with(kMetaDir) && e.uncompressed_size > st.limits.max_meta_file_bytes) {
                return unexpected(Error{ErrorCode::limit_exceeded,
                                        "SQ-INF file exceeds max_meta_file_bytes", e.path,
                                        st.manifest->id(), false});
            }
        }
    }
    return {};
}

}  // namespace

// ---------------------------------------------------------------------------
// detail::CartridgeState
// ---------------------------------------------------------------------------

detail::CartridgeState::~CartridgeState()
{
    if (archive_inited) {
        mz_zip_reader_end(&archive);
    }
}

Cartridge::Impl::~Impl() = default;

// ---------------------------------------------------------------------------
// Internal helpers used by the other translation units
// ---------------------------------------------------------------------------

std::optional<std::reference_wrapper<const EntryInfo>>
find_entry(const CartridgeState& st, std::string_view path) noexcept
{
    auto it = std::find_if(st.entries.begin(), st.entries.end(),
                           [&](const EntryInfo& e) { return e.path == path; });
    if (it == st.entries.end()) {
        return std::nullopt;
    }
    return std::cref(*it);
}

Result<std::vector<std::byte>> read_entry(const CartridgeState& st, const EntryInfo& info)
{
    // FR-ENTRY-3 / FR-LIM-3: the size and ratio checks precede any allocation
    // proportional to attacker-controlled input.
    if (info.uncompressed_size > st.limits.max_total_uncompressed) {
        return unexpected(make_error(ErrorCode::limit_exceeded,
                                     "entry exceeds max_total_uncompressed", info.path));
    }
    if (st.limits.max_entry_ratio > 0 && info.compressed_size > 0) {
        const std::uint64_t ceiling = info.compressed_size * st.limits.max_entry_ratio;
        if (info.uncompressed_size > ceiling) {
            return unexpected(make_error(ErrorCode::limit_exceeded,
                                         "entry exceeds max_entry_ratio", info.path));
        }
    }

    std::vector<std::byte> buf(static_cast<std::size_t>(info.uncompressed_size));

    if (st.is_archive()) {
        // miniz's reader takes a non-const handle even though reading is
        // semantically const (it tracks internal offsets). The const_cast is
        // confined to this boundary.
        auto* archive = const_cast<mz_zip_archive*>(&st.archive);

        const int index = mz_zip_reader_locate_file(archive, info.path.c_str(), nullptr, 0);
        if (index < 0) {
            return unexpected(make_error(ErrorCode::internal, "entry missing from archive index",
                                         info.path));
        }
        if (!buf.empty() &&
            !zip_ok(mz_zip_reader_extract_to_mem(archive, static_cast<mz_uint>(index), buf.data(),
                                                 buf.size(), 0))) {
            return unexpected(make_error(ErrorCode::container_invalid,
                                         "entry extraction failed or CRC mismatch", info.path));
        }
        return buf;
    }

    // Exploded form.
    const auto full = st.root / info.path;
    std::ifstream in(full, std::ios::binary);
    if (!in) {
        return unexpected(make_error(ErrorCode::io_failed, "cannot open entry file", info.path));
    }
    if (!buf.empty()) {
        in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
        if (in.gcount() != static_cast<std::streamsize>(buf.size())) {
            // FR-ENTRY-3: short read is a failure, never a truncated buffer.
            return unexpected(
                make_error(ErrorCode::io_failed, "short read on entry file", info.path));
        }
    }
    return buf;
}

Result<Manifest> parse_manifest_entry(const CartridgeState& st, const Limits& limits)
{
    auto found = find_entry(st, kManifestPath);
    if (!found) {
        return unexpected(make_error(ErrorCode::manifest_missing, "no manifest entry"));
    }
    const EntryInfo& info = found->get();

    if (info.uncompressed_size > limits.max_manifest_bytes) {
        return unexpected(make_error(ErrorCode::limit_exceeded,
                                     "manifest exceeds max_manifest_bytes", info.path));
    }

    auto data = read_entry(st, info);
    if (!data) {
        return unexpected(data.error());
    }
    const std::string_view json(reinterpret_cast<const char*>(data->data()), data->size());
    return Manifest::parse(json, limits);
}

// ---------------------------------------------------------------------------
// Cartridge
// ---------------------------------------------------------------------------

Cartridge::Cartridge() : impl_(std::make_unique<Impl>()) {}

Cartridge::Cartridge(Cartridge&&) noexcept            = default;
Cartridge& Cartridge::operator=(Cartridge&&) noexcept = default;
Cartridge::~Cartridge()                               = default;

Result<Cartridge> Cartridge::open(const std::filesystem::path& path, const OpenOptions& options)
{
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return unexpected(make_error(ErrorCode::io_failed, "cartridge path does not exist"));
    }

    Cartridge c;
    auto&     st = c.impl_->st;

    st.conformance = options.conformance;
    st.limits      = options.limits;
    st.supported_features.assign(options.supported_features.begin(),
                                 options.supported_features.end());
    st.symlinks            = options.symlinks;
    st.reject_native_code  = options.reject_native_code;

    if (std::filesystem::is_directory(path, ec)) {
        // FR-OPEN-1: a directory is an exploded cartridge iff it holds a
        // readable SQ-INF/manifest.json (§3.5).
        st.form = Form::exploded;
        st.root = path;
        if (!std::filesystem::is_regular_file(path / kManifestPath, ec)) {
            return unexpected(make_error(ErrorCode::manifest_missing,
                                         "directory has no SQ-INF/manifest.json"));
        }
        if (auto r = index_exploded(st); !r) {
            return unexpected(r.error());
        }
    } else {
        st.form = Form::archive_file;
        if (!zip_ok(mz_zip_reader_init_file(&st.archive, path.string().c_str(), 0))) {
            return unexpected(
                make_error(ErrorCode::container_invalid, "not a readable ZIP container"));
        }
        st.archive_inited = true;
        if (auto r = index_archive(st); !r) {
            return unexpected(r.error());
        }
    }

    if (auto r = finish_open(st); !r) {
        return unexpected(r.error());
    }
    return c;
}

Result<Cartridge> Cartridge::open_memory(std::span<const std::byte> bytes,
                                         const OpenOptions&         options)
{
    Cartridge c;
    auto&     st = c.impl_->st;

    st.form        = Form::archive_memory;
    st.conformance = options.conformance;
    st.limits      = options.limits;
    st.supported_features.assign(options.supported_features.begin(),
                                 options.supported_features.end());
    st.memory              = bytes;
    st.reject_native_code  = options.reject_native_code;

    if (!zip_ok(mz_zip_reader_init_mem(&st.archive, bytes.data(), bytes.size(), 0))) {
        return unexpected(
            make_error(ErrorCode::container_invalid, "not a readable ZIP memory image"));
    }
    st.archive_inited = true;

    if (auto r = index_archive(st); !r) {
        return unexpected(r.error());
    }
    if (auto r = finish_open(st); !r) {
        return unexpected(r.error());
    }
    return c;
}

const Manifest& Cartridge::manifest() const noexcept
{
    return *impl_->st.manifest;
}

bool Cartridge::exploded() const noexcept
{
    return impl_->st.form == Form::exploded;
}

std::span<const EntryInfo> Cartridge::entries() const noexcept
{
    return impl_->st.entries;
}

std::span<const EntryPath> Cartridge::skipped_symlinks() const noexcept
{
    return impl_->st.skipped_symlinks;
}

bool Cartridge::contains(std::string_view path) const noexcept
{
    return find_entry(impl_->st, path).has_value();
}

std::optional<std::reference_wrapper<const EntryInfo>>
Cartridge::find(std::string_view path) const noexcept
{
    return find_entry(impl_->st, path);
}

Result<std::vector<std::byte>> Cartridge::read(std::string_view path) const
{
    auto found = find_entry(impl_->st, path);
    if (!found) {
        return unexpected(
            make_error(ErrorCode::payload_missing, "no such entry", std::string(path)));
    }
    return read_entry(impl_->st, found->get());
}

Result<std::size_t> Cartridge::read_into(std::string_view path, std::span<std::byte> out) const
{
    auto found = find_entry(impl_->st, path);
    if (!found) {
        return unexpected(
            make_error(ErrorCode::payload_missing, "no such entry", std::string(path)));
    }
    const EntryInfo& info = found->get();

    // FR-ENTRY-4: too-small buffer fails without writing any bytes.
    if (out.size() < info.uncompressed_size) {
        return unexpected(make_error(ErrorCode::limit_exceeded,
                                     "destination buffer smaller than entry", info.path));
    }

    auto data = read_entry(impl_->st, info);
    if (!data) {
        return unexpected(data.error());
    }
    std::memcpy(out.data(), data->data(), data->size());
    return data->size();
}

}  // namespace sqcart
