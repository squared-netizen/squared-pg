// SPDX-License-Identifier: MIT
//
// The validation pipeline, format spec §12.1 stages 1-8.
//
// Stage 9 (compatibility resolution) is deliberately absent and must stay
// absent: it needs a resolution context sqcart does not have, and FR-VAL-5
// forbids ever giving validate() a multi-cartridge parameter. Whether two
// different kits' integration_areas collide is an engine question.

#include "impl.hpp"

#include <algorithm>
#include <string>
#include <string_view>

namespace sqcart {

namespace {

void add(ValidationReport& r, Severity sev, std::string msg,
         std::optional<EntryPath> entry = std::nullopt)
{
    r.diagnostics.push_back(Diagnostic{sev, std::move(msg), std::move(entry)});
}

/// FR-VAL-3: unrecognised SQ-INF/ files are informational, never errors.
bool is_reserved_meta_name(std::string_view path)
{
    static constexpr std::string_view kKnown[] = {
        "SQ-INF/manifest.json",
        "SQ-INF/hashes.json",
        "SQ-INF/thumbnail.png",
        "SQ-INF/icon.png",
    };
    if (std::find(std::begin(kKnown), std::end(kKnown), path) != std::end(kKnown)) {
        return true;
    }
    return path.starts_with("SQ-INF/licenses/") || path.starts_with("SQ-INF/signatures/") ||
           path.starts_with("SQ-INF/targets/");
}

}  // namespace

ValidationReport validate(const Cartridge& c)
{
    ValidationReport r;

    // Stages 1-3 (container, paths, limits) were enforced by open(); a
    // Cartridge cannot exist having failed them. What remains re-checkable
    // here is everything a lenient open() skipped, plus the payload-level
    // rules that need the manifest and the entry list together.

    const Manifest& m = c.manifest();

    // Stage 4: format version.
    //
    // Unreachable in practice as of format 2 -- parse_root refuses a foreign
    // format_version, so a Cartridge cannot exist carrying one, and the only
    // route to a Manifest is through that parser. Kept because it costs one
    // comparison and the alternative is a validator that silently trusts an
    // invariant enforced somewhere else; if the parse-time check is ever
    // relaxed, this is the net underneath it.
    if (m.format_version() != kFormatVersion) {
        add(r, Severity::error,
            "unsupported format_version " + std::to_string(m.format_version()));
    }

    // Stage 4b: the declared payload root should contain something.
    //
    // Informational, not an error. A manifest-only cartridge is legal -- an
    // asset bundle can legitimately declare entries it has yet to be filled
    // with -- but a `tree` pointing at a directory the cartridge does not
    // have is far more often a typo than an intention, and it fails later in
    // a place that does not mention the manifest.
    if (m.tree() != ".") {
        const bool populated = std::any_of(
            c.entries().begin(), c.entries().end(),
            [&](const EntryInfo& e) { return e.path.starts_with(m.tree()); });
        if (!populated) {
            add(r, Severity::info,
                "declared payload root '" + m.tree() + "' contains no entries");
        }
    }

    // Stage 7: prohibited entries (§8) and manifest-referenced payload.
    for (const auto& e : c.entries()) {
        if (is_native_code_path(e.path)) {
            // Warning, not error: containing native code is legitimate and
            // expected for kits and packages. Loading it is a separate
            // decision made by the runtime, under the trust model of §10.
            add(r, Severity::warning, "contains native code; a runtime must not load it "
                                      "from an unverified cartridge", e.path);
        }
        if (e.path.starts_with(kMetaDir) && !is_reserved_meta_name(e.path)) {
            add(r, Severity::info, "unrecognised file under SQ-INF/", e.path);
        }
    }

    // Stage 7 used to continue here with payload-reference checks driven by
    // the kind body: a cartridge's entry module exists, an asset bundle's
    // declared paths exist, a template's ownership globs classify every file
    // exactly once.
    //
    // All of them are gone with the kind bodies, and it is worth being plain
    // that this is a real loss rather than a tidy-up. They were genuinely
    // useful checks. They were also, every one of them, checks against a
    // schema this library is no longer permitted to know (§0.1): you cannot
    // verify that `entry.module` names a present entry without knowing that
    // `entry.module` is a path, and knowing that is knowing the consumer's
    // vocabulary.
    //
    // The checks belong to the consumer now, against its own section, where
    // it can give a better error than "manifest references a missing asset"
    // because it knows what the reference was for. What this library can
    // still say -- and does, above -- is whether the declared payload root is
    // populated, which is decidable without knowing anything about anyone.

    // Symbolic links omitted during the walk. Warnings, not errors: the
    // cartridge is structurally fine, but a human should know that files
    // present in the source directory are absent from the cartridge.
    for (const auto& link : c.skipped_symlinks()) {
        add(r, Severity::warning, "symbolic link omitted from the cartridge", link);
    }

    // Stage 8: integrity, when declared.
    if (auto declared = c.declared_digests()) {
        if (!declared->empty()) {
            if (auto ok = c.verify_integrity(); !ok) {
                add(r, Severity::error, ok.error().message, ok.error().entry);
            }
        }
    } else {
        add(r, Severity::error, declared.error().message, declared.error().entry);
    }

    // FR-VAL-2: conforming iff no error-severity diagnostic.
    r.conforming = std::none_of(r.diagnostics.begin(), r.diagnostics.end(),
                                [](const Diagnostic& d) { return d.severity == Severity::error; });
    return r;
}

}  // namespace sqcart
