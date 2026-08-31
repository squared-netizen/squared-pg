// SPDX-License-Identifier: MIT
//
// Cartridge writer (format spec §9.3). Only compiled when
// SQCART_ENABLE_WRITER is set; a launcher or shipped game carries no
// compressor and no manifest serialiser (component spec §4.1).
//
// Producer of the deterministic test artifact: it packs an exploded tree
// (tests/testsqcart) into an archived test.sq. The reader then opens what
// this produced, so writer and reader exercise each other.
//
// Backend boundary: miniz's mz_zip_archive is embedded as a value member and
// its C API is called directly. miniz owns its own pointers; sqcart-authored
// code on both sides of this boundary is pointer-free. No mz_zip_* type
// appears in any public header (FR-CON-3).

#include "impl.hpp"

#ifdef SQCART_ENABLE_WRITER

    #include <algorithm>
    #include <cstddef>
    #include <filesystem>
    #include <fstream>
    #include <limits>
    #include <string>
    #include <utility>
    #include <vector>

    #include <miniz.h>

namespace sqcart {
namespace {

bool zip_ok(mz_bool ok)
{
    return ok != 0;
}

// ASCII-only lowercasing used for canonical path ordering (§9.3). Entry
// names are UTF-8; the canonical order is defined over the byte sequence,
// so a plain byte-wise comparison is correct and deterministic.
bool entry_before(std::string_view a, std::string_view b)
{
    return a < b;
}

} // namespace

// ---------------------------------------------------------------------------
// Writer::Impl
// ---------------------------------------------------------------------------

struct Writer::Impl {
    std::filesystem::path out;
    WriteOptions options;

    /// Links omitted by add_tree under SymlinkPolicy::skip. Reported by the
    /// CLI so packing is never quietly lossy.
    std::vector<EntryPath> skipped_symlinks;

    // Pending entries. add()/add_file()/add_tree() buffer here; finish()
    // sorts canonically and streams to miniz. Buffering is deliberate for
    // the minimal pass: canonical ordering (sorted, manifest first) is
    // impossible to emit correctly without knowing the whole set first.
    struct Pending {
        EntryPath path;
        std::vector<std::byte> data;
        std::size_t size{0};
        std::uint32_t crc32{0};
    };

    std::vector<Pending> pending;

    std::string manifest; ///< Validated manifest JSON
    bool have_manifest{false};

    mz_zip_archive archive{};
    bool active{false};

    ~Impl()
    {
        if (active) {
            mz_zip_writer_end(&archive);
        }
    }

    static Result<Writer> create(std::filesystem::path out, const WriteOptions& options)
    {
        auto w = Writer();
        w.impl_->out = std::move(out);
        w.impl_->options = options;

        if (!zip_ok(mz_zip_writer_init_file(&w.impl_->archive, w.impl_->out.c_str(), 0))) {
            return unexpected(Error{ErrorCode::io_failed, "failed to open output file for writing",
                                    std::nullopt, std::nullopt, false});
        }
        w.impl_->active = true;
        return w;
    }
};

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

Writer::Writer() : impl_(std::make_unique<Impl>()) {}

Writer::Writer(Writer&&) noexcept = default;
Writer& Writer::operator=(Writer&&) noexcept = default;
Writer::~Writer() = default;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

Result<Writer> Writer::create(const std::filesystem::path& out, const WriteOptions& options)
{
    return Impl::create(out, options);
}

Result<void> Writer::set_manifest(std::string_view json)
{
    // Validate the manifest before accepting it; a non-parsing manifest
    // cannot be the envelope of a conforming cartridge.
    auto parsed = Manifest::parse(json, impl_->options.limits);
    if (!parsed) {
        return unexpected(parsed.error());
    }
    impl_->manifest = std::string(json);
    impl_->have_manifest = true;
    return {};
}

Result<void> Writer::add(std::string_view path, std::span<const std::byte> content)
{
    auto entry = validate_entry_path(path, impl_->options.limits);
    if (!entry) {
        return unexpected(entry.error());
    }
    if (*entry == kManifestPath) {
        return unexpected(Error{ErrorCode::path_unsafe,
                                "the manifest is set via set_manifest, not add()", *entry,
                                std::nullopt, false});
    }

    // Duplicate paths are a payload_missing-adjacent authoring error; the
    // canonical archive cannot contain two entries with one name.
    for (const auto& p : impl_->pending) {
        if (p.path == *entry) {
            return unexpected(Error{ErrorCode::path_unsafe, "duplicate entry path in writer input",
                                    *entry, std::nullopt, false});
        }
    }

    Impl::Pending p;
    p.path = std::move(*entry);
    p.data.assign(content.begin(), content.end());
    p.size = p.data.size();
    if (p.size > std::numeric_limits<mz_uint32>::max()) {
        return unexpected(Error{ErrorCode::limit_exceeded, "entry exceeds ZIP 32-bit size limit",
                                p.path, std::nullopt, false});
    }
    p.crc32 = static_cast<std::uint32_t>(
        mz_crc32(0, reinterpret_cast<const unsigned char*>(p.data.data()), p.data.size()));

    impl_->pending.push_back(std::move(p));
    return {};
}

Result<void> Writer::add_file(std::string_view path, const std::filesystem::path& source)
{
    std::error_code ec;
    const auto size = std::filesystem::file_size(source, ec);
    if (ec) {
        return unexpected(Error{ErrorCode::io_failed, "cannot stat source file for add_file",
                                std::nullopt, std::nullopt, false});
    }
    std::ifstream in(source, std::ios::binary);
    if (!in) {
        return unexpected(Error{ErrorCode::io_failed, "cannot open source file for add_file",
                                std::nullopt, std::nullopt, false});
    }
    std::vector<std::byte> buf(static_cast<std::size_t>(size));
    in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(size));
    if (in.gcount() != static_cast<std::streamsize>(size)) {
        return unexpected(Error{ErrorCode::io_failed, "short read while reading source file",
                                std::nullopt, std::nullopt, false});
    }
    return add(path, buf);
}

Result<void> Writer::add_tree(const std::filesystem::path& root)
{
    std::error_code ec;
    std::filesystem::recursive_directory_iterator it(root, ec), end;
    if (ec) {
        return unexpected(Error{ErrorCode::io_failed, "cannot enumerate directory for add_tree",
                                std::nullopt, std::nullopt, false});
    }
    for (; it != end; it.increment(ec)) {
        if (ec) {
            return unexpected(Error{ErrorCode::io_failed, "directory iteration error in add_tree",
                                    std::nullopt, std::nullopt, false});
        }
        const auto& p = it->path();
        if (it->is_directory(ec)) {
            continue;
        }
        if (it->is_symlink(ec)) {
            // Same policy as the reader (src/symlink.cpp). Refusing outright
            // made the writer unusable on a normal workspace; a link escaping
            // the root is still never packed.
            std::string why;
            const auto  verdict = classify_symlink(root, p, impl_->options.symlinks, why);
            if (verdict == LinkVerdict::error) {
                return unexpected(Error{ErrorCode::io_failed, why,
                                        p.lexically_relative(root).generic_string(),
                                        std::nullopt, false});
            }
            if (verdict == LinkVerdict::skip) {
                impl_->skipped_symlinks.push_back(
                    p.lexically_relative(root).generic_string());
                continue;
            }
        }
        // Relative, forward-slash entry path rooted at `root`.
        auto rel = std::filesystem::relative(p, root, ec);
        if (ec) {
            return unexpected(Error{ErrorCode::io_failed, "cannot relativise path in add_tree",
                                    std::nullopt, std::nullopt, false});
        }
        std::string name = rel.generic_string();
        if (name == kManifestPath) {
            continue; // supplied via set_manifest()
        }
        auto r = add_file(name, p);
        if (!r) {
            return unexpected(r.error());
        }
    }
    return {};
}

std::span<const EntryPath> Writer::skipped_symlinks() const noexcept
{
    return impl_->skipped_symlinks;
}

Result<ContentDigest> Writer::finish() &&
{
    if (!impl_->have_manifest) {
        return unexpected(Error{ErrorCode::manifest_missing,
                                "finish() called before set_manifest()", std::nullopt, std::nullopt,
                                false});
    }
    if (!impl_->active) {
        return unexpected(Error{ErrorCode::internal, "finish() called on spent Writer",
                                std::nullopt, std::nullopt, false});
    }

    // Canonical order: the manifest first (it sorts before any other
    // SQ-INF file) then every payload entry sorted by path.
    std::vector<Impl::Pending*> order;
    order.reserve(impl_->pending.size() + 1);
    // Manifest goes in as the leading entry.
    Impl::Pending manifest;
    manifest.path = std::string(kManifestPath);
    const auto& mdata = impl_->manifest;
    manifest.data.reserve(mdata.size());
    for (char ch : mdata) {
        manifest.data.push_back(static_cast<std::byte>(ch));
    }
    manifest.size = manifest.data.size();
    manifest.crc32 = static_cast<std::uint32_t>(mz_crc32(
        0, reinterpret_cast<const unsigned char*>(manifest.data.data()), manifest.data.size()));

    // Collect payload pointers and sort by path.
    for (auto& p : impl_->pending) {
        order.push_back(&p);
    }
    std::sort(order.begin(), order.end(), [](const Impl::Pending* a, const Impl::Pending* b) {
        return entry_before(a->path, b->path);
    });

    // Write manifest first (Stored), then payloads (compressed).
    auto write_one = [&](const Impl::Pending& p, int level) -> bool {
        const mz_uint level_and_flags = static_cast<mz_uint>(
            level > 0 ? static_cast<mz_uint>(level) : static_cast<mz_uint>(MZ_NO_COMPRESSION));
        return zip_ok(mz_zip_writer_add_mem(&impl_->archive, p.path.c_str(),
                                            p.data.empty() ? nullptr : p.data.data(),
                                            static_cast<size_t>(p.size), level_and_flags));
    };

    if (!write_one(manifest, 0)) {
        return unexpected(Error{ErrorCode::internal, "failed to write manifest entry", std::nullopt,
                                std::nullopt, false});
    }
    const int payload_level = impl_->options.compression > 0 ? impl_->options.compression : 0;
    for (const Impl::Pending* p : order) {
        if (!write_one(*p, payload_level)) {
            return unexpected(Error{ErrorCode::internal, "failed to write payload entry", p->path,
                                    std::nullopt, false});
        }
    }

    // FR-WRITE-7/8: accumulate the digest from the same implementation the
    // reader uses, over the same entry set. Built after the payload is known
    // and before finalisation, so hashes.json can be written into the same
    // archive it describes.
    DigestAccumulator acc;
    acc.add(manifest.path, manifest.data);
    for (const Impl::Pending* p : order) {
        acc.add(p->path, p->data);
    }

    if (impl_->options.write_hashes) {
        // hashes.json is excluded from the content digest by §9.2 (it cannot
        // contain its own hash), so it is emitted after the accumulator has
        // seen everything and is never fed to it.
        std::string json = "{\n  \"algorithm\": \"sha256\",\n  \"entries\": {\n";
        std::vector<std::pair<std::string, ContentDigest>> listed;
        listed.emplace_back(manifest.path, sha256(manifest.data));
        for (const Impl::Pending* p : order) {
            listed.emplace_back(p->path, sha256(p->data));
        }
        std::sort(listed.begin(), listed.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        for (std::size_t i = 0; i < listed.size(); ++i) {
            json += "    \"" + listed[i].first + "\": \"" + to_hex(listed[i].second) + "\"";
            json += (i + 1 < listed.size()) ? ",\n" : "\n";
        }
        json += "  },\n  \"content_digest\": \"" + to_hex(DigestAccumulator(acc).finish()) +
                "\"\n}\n";

        Impl::Pending hashes;
        hashes.path = "SQ-INF/hashes.json";
        hashes.data.reserve(json.size());
        for (char ch : json) {
            hashes.data.push_back(static_cast<std::byte>(ch));
        }
        hashes.size = hashes.data.size();
        if (!write_one(hashes, 0)) {
            return unexpected(Error{ErrorCode::internal, "failed to write hashes.json",
                                    std::string("SQ-INF/hashes.json"), std::nullopt, false});
        }
    }

    if (!zip_ok(mz_zip_writer_finalize_archive(&impl_->archive))) {
        return unexpected(Error{ErrorCode::internal, "failed to finalise archive", std::nullopt,
                                std::nullopt, false});
    }
    if (!zip_ok(mz_zip_writer_end(&impl_->archive))) {
        return unexpected(Error{ErrorCode::internal, "failed to close archive", std::nullopt,
                                std::nullopt, false});
    }
    impl_->active = false;

    return acc.finish();
}

} // namespace sqcart

#endif // SQCART_ENABLE_WRITER
