// SPDX-License-Identifier: MIT
//
// `sqcart create` — the jar `cf` equivalent.
//
// jar's defining convenience is that `jar cf out.jar -C dir .` works on a
// directory that knows nothing about jar: the tool synthesises a manifest.
// `pack` deliberately refuses a directory without SQ-INF/manifest.json,
// because packing an arbitrary tree produces a .sq that fails to open. This
// command is the other half: it writes the manifest first, then packs.
//
// The split matters. `pack` is for cartridges that already exist and must
// round-trip byte-for-byte. `create` is for turning a plain folder into one,
// and it is the only command in the tool that authors content rather than
// reading or copying it.

#include "commands.hpp"
#include "seeds.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <ostream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace sqcart::app {

namespace {

/// Derive a spec-legal identifier segment from arbitrary text.
///
/// Format spec §5.2 requires each segment to match [a-z][a-z0-9_]*. A folder
/// called "My Assets (v2)" must become something legal or the manifest we
/// write will not open, which would be a worse failure than refusing.
std::string sanitise_segment(std::string_view raw)
{
    std::string out;
    out.reserve(raw.size());
    for (char c : raw) {
        const auto u = static_cast<unsigned char>(c);
        if (u >= 'A' && u <= 'Z') {
            out.push_back(static_cast<char>(u - 'A' + 'a'));
        } else if ((u >= 'a' && u <= 'z') || (u >= '0' && u <= '9')) {
            out.push_back(static_cast<char>(u));
        } else if (!out.empty() && out.back() != '_') {
            out.push_back('_');
        }
    }
    while (!out.empty() && out.back() == '_') {
        out.pop_back();
    }
    // A segment must start with a letter; a folder named "2024" cannot lead.
    if (out.empty() || !(out.front() >= 'a' && out.front() <= 'z')) {
        out.insert(out.begin(), 'x');
    }
    return out;
}

std::string replace_all(std::string s, std::string_view from, std::string_view to)
{
    std::size_t at = 0;
    while ((at = s.find(from, at)) != std::string::npos) {
        s.replace(at, from.size(), to);
        at += to.size();
    }
    return s;
}

/// Escape a string for embedding in JSON. Only what can appear in a path.
std::string json_escape(std::string_view s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out.push_back(c);
        }
    }
    return out;
}

/// Collect payload-relative paths, excluding anything under SQ-INF/.
///
/// Sorted, so a created manifest is a deterministic function of the tree:
/// running create twice on the same folder must produce identical bytes.
Result<std::vector<std::string>> collect_payload(const std::filesystem::path& root,
                                                 SymlinkPolicy             policy,
                                                 std::vector<std::string>& skipped)
{
    std::error_code ec;
    std::vector<std::string> paths;

    std::filesystem::recursive_directory_iterator it(
        root, std::filesystem::directory_options::none, ec);
    if (ec) {
        return unexpected(std::string("cannot walk ") + root.string());
    }
    const std::filesystem::recursive_directory_iterator end;

    for (; it != end; it.increment(ec)) {
        if (ec) {
            return unexpected(std::string("directory walk failed"));
        }
        if (it->is_symlink()) {
            std::string why;
            const auto  verdict = classify_symlink(root, it->path(), policy, why);
            // lexically_relative: relative() would resolve the link and
            // report its target rather than the link itself.
            const std::string rel_s = it->path().lexically_relative(root).generic_string();
            if (verdict == LinkVerdict::error) {
                return unexpected(rel_s + ": " + why);
            }
            if (verdict == LinkVerdict::skip) {
                skipped.push_back(rel_s + "  (" + why + ")");
                continue;
            }
        }
        if (!it->is_regular_file(ec)) {
            continue;
        }
        auto rel = std::filesystem::relative(it->path(), root, ec);
        if (ec) {
            return unexpected("cannot relativise " + it->path().string());
        }
        auto s = rel.generic_string();
        if (s.starts_with("SQ-INF/")) {
            continue;
        }
        paths.push_back(std::move(s));
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

}  // namespace

ExitCode cmd_create(const Invocation& inv, Environment& env)
{
    if (inv.positional.empty()) {
        env.err << "usage: sqcart create <directory> --kind K [-o out.sq] [--id ID]\n"
                   "       --kind is any well-formed token; sqcart does not\n"
                   "       enumerate roles (format spec 5.2)\n"
                   "       --in-place writes SQ-INF/manifest.json and packs nothing\n";
        return ExitCode::usage;
    }

    const auto src = resolve(env, inv.positional[0]);
    std::error_code ec;
    if (!std::filesystem::is_directory(src, ec)) {
        env.err << "sqcart: " << src.string() << " is not a directory\n";
        return ExitCode::failure;
    }

    const auto manifest_path = src / kManifestPath;
    const bool already       = std::filesystem::is_regular_file(manifest_path, ec);
    if (already && !inv.has_flag("force")) {
        // Refusing protects a hand-written manifest from being clobbered by a
        // seeded one. `pack` is the right command once a manifest exists.
        env.err << "sqcart: " << src.string() << " already has " << kManifestPath << "\n"
                << "        use 'sqcart pack' to package it, or --force to reseed\n";
        return ExitCode::failure;
    }

    std::vector<std::string> skipped_links;
    auto payload = collect_payload(src, symlink_policy_from(inv), skipped_links);
    if (!payload) {
        env.err << "sqcart: " << payload.error() << "\n";
        return ExitCode::failure;
    }
    // Never silent: every omitted link is named, with the reason.
    for (const auto& s : skipped_links) {
        env.err << "sqcart: skipped symlink " << s << "\n";
    }
    if (!skipped_links.empty()) {
        env.err << "sqcart: " << skipped_links.size()
                << " symbolic link(s) omitted; --symlinks=follow packs internal targets, "
                   "--symlinks=reject fails instead\n";
    }
    if (payload->empty()) {
        env.err << "sqcart: " << src.string() << " has no files to package\n";
        return ExitCode::failure;
    }

    // Kind. Required, with no default and no detection.
    //
    // Format 1 guessed: asset-bundle normally, cartridge if a file named
    // main.lua turned up. Both branches required knowing what those roles
    // mean and what their bodies must contain, which is the coupling format 2
    // removed. A tool that does not know the ecosystem's roles cannot pick
    // one for you, and pretending otherwise would put a wrong `kind` in a
    // manifest a consumer then has to reject.
    const std::string kind = inv.option("kind").value_or("");
    if (kind.empty()) {
        env.err << "sqcart: create needs --kind\n"
                << "        any token matching [a-z][a-z0-9_]* is accepted; what it\n"
                << "        means is the consuming tool's question, not sqcart's\n";
        return ExitCode::usage;
    }
    if (!valid_kind_token(kind)) {
        env.err << "sqcart: '" << kind << "' is not a well-formed kind token\n"
                << "        expected [a-z][a-z0-9_]*, one segment\n";
        return ExitCode::usage;
    }

    auto seed = seed_for(kind);
    if (!seed) {
        env.err << "sqcart: internal error: no envelope seed\n";
        return ExitCode::failure;
    }

    // Identity. A derived id is a starting point, not an assertion of
    // ownership. No kind-derived prefix: which prefixes belong to which roles
    // is ecosystem policy, and this tool has no standing to enforce it.
    const std::string folder = src.filename().empty()
                                   ? std::string("cartridge")
                                   : src.filename().string();
    std::string id = inv.option("id").value_or("");
    if (id.empty()) {
        id = "local." + sanitise_segment(folder);
    }

    const std::string version = inv.option("version").value_or("0.1.0");
    const std::string title   = inv.option("title").value_or(folder);

    std::string manifest(*seed);
    manifest = replace_all(std::move(manifest), "{{id}}", json_escape(id));
    manifest = replace_all(std::move(manifest), "{{version}}", json_escape(version));
    manifest = replace_all(std::move(manifest), "{{title}}", json_escape(title));
    manifest = replace_all(std::move(manifest), "{{kind}}", json_escape(kind));

    // Validate before writing. A seeded manifest that does not parse is a
    // defect in the seed, and the user should hear about it here rather than
    // when they later try to open what we wrote.
    if (auto parsed = Manifest::parse(manifest); !parsed) {
        env.err << "sqcart: generated manifest is invalid: " << parsed.error().message << "\n"
                << "        this is a defect in the envelope seed\n";
        if (inv.has_flag("verbose")) {
            env.err << manifest << '\n';
        }
        return ExitCode::failure;
    }

    // Write it into the folder. `create` is the one command that authors
    // content, so it says exactly what it wrote.
    std::filesystem::create_directories(src / kMetaDir, ec);
    if (ec) {
        env.err << "sqcart: cannot create " << kMetaDir << " in " << src.string() << "\n";
        return ExitCode::failure;
    }
    {
        std::ofstream out(manifest_path, std::ios::binary | std::ios::trunc);
        if (!out) {
            env.err << "sqcart: cannot write " << manifest_path.string() << "\n";
            return ExitCode::failure;
        }
        out << manifest;
        out.close();
        if (!out) {
            env.err << "sqcart: write failed for " << manifest_path.string() << "\n";
            return ExitCode::failure;
        }
    }
    env.err << "sqcart: wrote " << kManifestPath << " (kind " << kind << ", id " << id << ")\n";

    if (inv.has_flag("in-place") || !inv.option("output")) {
        // No output requested: the folder is now an exploded cartridge, which
        // is a complete and valid result (§3.5). Nothing more to do.
        if (!inv.option("output")) {
            env.err << "sqcart: " << src.string()
                    << " is now an exploded cartridge; pass -o to also pack it\n";
        }
        return ExitCode::ok;
    }

    // Hand off to pack rather than duplicating the writer wiring. Same
    // arguments, one implementation.
    Invocation packing;
    packing.command    = "pack";
    packing.positional = {src.string()};
    packing.options    = inv.options;
    packing.flags      = inv.flags;
    return cmd_pack(packing, env);
}

}  // namespace sqcart::app
