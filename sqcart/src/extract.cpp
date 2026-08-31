// SPDX-License-Identifier: MIT
//
// Payload materialisation, format spec §3.1 and FR-EXT-*.
//
// extract_to() is deliberately NOT transactional across entries (FR-EXT-4).
// The engine's Transaction Service already owns staging and rollback
// (§2.3.13); duplicating it here would give two components a claim on undo.
// A caller needing all-or-nothing extracts to a staging directory and renames.

#include "impl.hpp"

#include <algorithm>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace sqcart {

namespace {

Error fs_error(std::string msg, std::optional<EntryPath> entry = std::nullopt)
{
    return Error{ErrorCode::io_failed, std::move(msg), std::move(entry), std::nullopt, true};
}

/// Split a path into its non-empty components.
///
/// lexically_normal() leaves a trailing EMPTY component on any path ending in
/// '.' or '/': "/a/b/." normalises to "/a/b/" whose components are
/// {"/", "a", "b", ""}. A component-wise prefix comparison that does not drop
/// it can never match, so `unpack cart.sq` with the default destination "."
/// -- the commonest invocation there is -- reported that every entry would
/// escape the destination. `-d ./test` worked and `-d test/` did not, which
/// is exactly the shape of that bug.
std::vector<std::filesystem::path::string_type> components(const std::filesystem::path& p)
{
    std::vector<std::filesystem::path::string_type> out;
    for (const auto& part : p.lexically_normal()) {
        if (!part.empty()) {
            out.push_back(part.native());
        }
    }
    return out;
}

/// Confirm `candidate` lies inside `root`. Belt to FR-EXT-1's braces: even
/// with a validated relative path, the concatenation is re-checked before any
/// write.
bool within(const std::filesystem::path& root, const std::filesystem::path& candidate)
{
    const auto r = components(root);
    const auto c = components(candidate);
    if (c.size() < r.size()) {
        return false;
    }
    return std::equal(r.begin(), r.end(), c.begin());
}

}  // namespace

Result<void> Cartridge::extract_to(const std::filesystem::path& dest, bool overwrite) const
{
    const auto& st = impl_->st;
    std::error_code ec;

    const auto dest_norm = dest.lexically_normal();

    // FR-EXT-1: re-validate every path before writing anything, independently
    // of what open() already checked. This matters for cartridges opened
    // under lenient conformance, which skipped stages 5-8.
    std::vector<std::pair<const EntryInfo*, std::filesystem::path>> plan;
    plan.reserve(st.entries.size());

    for (const auto& e : st.entries) {
        auto vpath = validate_entry_path_impl(e.path, st.limits);
        if (!vpath) {
            auto err  = vpath.error();
            err.entry = e.path;
            return unexpected(err);
        }
        auto target = (dest_norm / *vpath).lexically_normal();

        // FR-EXT-2 / FR-SEC-1: nothing is written outside dest, under any input.
        if (!within(dest_norm, target)) {
            return unexpected(Error{ErrorCode::path_unsafe,
                                    "entry would materialise outside the destination", e.path,
                                    std::nullopt, false});
        }
        plan.emplace_back(&e, std::move(target));
    }

    // FR-EXT-3: with overwrite=false the existence check precedes all writes,
    // rather than interleaving with them.
    if (!overwrite) {
        for (const auto& [info, target] : plan) {
            if (std::filesystem::exists(target, ec)) {
                return unexpected(Error{ErrorCode::io_failed,
                                        "destination path already exists", info->path,
                                        std::nullopt, false});
            }
        }
    }

    std::filesystem::create_directories(dest_norm, ec);
    if (ec) {
        return unexpected(fs_error("cannot create destination directory"));
    }

    for (const auto& [info, target] : plan) {
        if (target.has_parent_path()) {
            std::filesystem::create_directories(target.parent_path(), ec);
            if (ec) {
                return unexpected(fs_error("cannot create entry directory", info->path));
            }
        }

        auto data = read_entry(st, *info);
        if (!data) {
            return unexpected(data.error());
        }

        std::ofstream out(target, std::ios::binary | std::ios::trunc);
        if (!out) {
            return unexpected(fs_error("cannot open destination file", info->path));
        }
        if (!data->empty()) {
            out.write(reinterpret_cast<const char*>(data->data()),
                      static_cast<std::streamsize>(data->size()));
        }
        out.close();
        if (!out) {
            return unexpected(fs_error("write failed", info->path));
        }

        // FR-EXT-5: permission bits, symlinks and other external attributes
        // carry no meaning in v1 and are not restored. Files land with the
        // process umask, deliberately.
    }
    return {};
}

}  // namespace sqcart
