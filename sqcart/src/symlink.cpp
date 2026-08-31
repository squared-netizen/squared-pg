// SPDX-License-Identifier: MIT
//
// Symbolic link policy for exploded cartridges.
//
// Terminal workspaces are full of symlinks: a built binary linked back into
// the source tree, a shared asset directory linked into several projects.
// The first implementation refused outright on encountering one, which makes
// the tool unusable in exactly the environment it targets.
//
// The rule that does not bend, regardless of policy: a link whose target
// resolves outside the cartridge root is never packed. Following one would
// copy bytes the author never placed in the directory -- a link to
// ~/.ssh/id_rsa would put that key in a distributable cartridge. Format spec
// §3.5 states this as a MUST NOT; the SHOULD-reject part is what the policy
// relaxes.

#include "impl.hpp"

#include <algorithm>
#include <filesystem>
#include <vector>
#include <string>
#include <system_error>

namespace sqcart {

namespace {

/// True when `candidate` lies inside `root` after both are made canonical.
/// Canonical, not lexical: a lexical check is defeated by an intermediate
/// symlink in the path.
/// Component-wise containment, ignoring empty components.
///
/// See the note in extract.cpp: a normalised path ending in '/' or '.' keeps
/// a trailing empty component that no comparison will ever match.
bool inside(const std::filesystem::path& root, const std::filesystem::path& candidate)
{
    std::vector<std::filesystem::path::string_type> r, c;
    for (const auto& part : root.lexically_normal()) {
        if (!part.empty()) { r.push_back(part.native()); }
    }
    for (const auto& part : candidate.lexically_normal()) {
        if (!part.empty()) { c.push_back(part.native()); }
    }
    if (c.size() < r.size()) {
        return false;
    }
    return std::equal(r.begin(), r.end(), c.begin());
}

}  // namespace

LinkVerdict classify_symlink(const std::filesystem::path& root,
                             const std::filesystem::path& link, SymlinkPolicy policy,
                             std::string& why)
{
    if (policy == SymlinkPolicy::reject) {
        why = "symbolic link (policy: reject)";
        return LinkVerdict::error;
    }

    std::error_code ec;

    const auto canon_root = std::filesystem::weakly_canonical(root, ec);
    if (ec) {
        why = "cannot canonicalise cartridge root";
        return LinkVerdict::error;
    }

    const auto target = std::filesystem::weakly_canonical(link, ec);
    if (ec) {
        why = "dangling or unresolvable symbolic link";
        return LinkVerdict::skip;
    }

    if (!std::filesystem::exists(target, ec) || ec) {
        why = "symbolic link target does not exist";
        return LinkVerdict::skip;
    }

    if (!inside(canon_root, target)) {
        // Non-negotiable, whatever the policy. Naming the target in the
        // message is deliberate: the user should see exactly what would have
        // been packed had this not been refused.
        why = "symbolic link escapes the cartridge root -> " + target.string();
        return LinkVerdict::skip;
    }

    if (std::filesystem::is_directory(target, ec)) {
        // A directory link inside the root duplicates entries already indexed
        // by the walk, and can form a cycle. Skipping loses nothing.
        why = "symbolic link to a directory inside the cartridge";
        return LinkVerdict::skip;
    }

    if (!std::filesystem::is_regular_file(target, ec) || ec) {
        why = "symbolic link target is not a regular file";
        return LinkVerdict::skip;
    }

    if (policy == SymlinkPolicy::follow_internal) {
        return LinkVerdict::use_target;
    }

    why = "symbolic link (policy: skip)";
    return LinkVerdict::skip;
}

}  // namespace sqcart
