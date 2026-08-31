// SPDX-License-Identifier: MIT
//
// The validation pipeline, format spec §12.1 stages 1-8.
//
// Stage 9 (compatibility resolution) is deliberately absent and must stay
// absent: it needs a resolution context sqcart does not have, and FR-VAL-5
// forbids ever giving validate() a multi-cartridge parameter. Whether two
// different kits' integration_areas collide is an engine question.

#include "glob.hpp"
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

/// FR-VAL-4: every materialisable template path must match exactly one
/// ownership classification. Zero matches or two is an error, because a file
/// whose owner is ambiguous cannot be safely regenerated (§2.7.10-11).
void check_template_ownership(const Cartridge& c, const TemplateBody& body, ValidationReport& r)
{
    const std::string prefix = body.tree.empty() ? std::string("tree/") : body.tree;

    for (const auto& e : c.entries()) {
        if (!e.path.starts_with(prefix)) {
            continue;
        }
        const std::string rel = e.path.substr(prefix.size());
        if (rel.empty()) {
            continue;
        }

        int         matches = 0;
        std::string which;
        auto        test = [&](const std::vector<std::string>& pats, const char* label) {
            for (const auto& p : pats) {
                if (detail::glob_match(rel, p)) {
                    ++matches;
                    if (!which.empty()) {
                        which += ", ";
                    }
                    which += label;
                    return;
                }
            }
        };
        test(body.ownership.generated, "generated");
        test(body.ownership.user, "user");
        test(body.ownership.shared, "shared");

        if (matches == 0) {
            add(r, Severity::error, "template path matches no ownership classification", e.path);
        } else if (matches > 1) {
            add(r, Severity::error,
                "template path matches multiple ownership classifications: " + which, e.path);
        }
    }
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
    if (m.format_version() != kFormatVersion) {
        add(r, Severity::error,
            "unsupported format_version " + std::to_string(m.format_version()));
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

    auto require_entry = [&](const EntryPath& p, const char* what) {
        if (!p.empty() && !c.contains(p)) {
            add(r, Severity::error, std::string("manifest references a missing ") + what, p);
        }
    };

    if (auto body = m.as_cartridge()) {
        require_entry(body->get().entry.module, "entry module");
        for (const auto& [target, path] : body->get().entry.bytecode_cache) {
            require_entry(path, "bytecode cache entry");
        }
        // Format spec §7 rule 1: source is the normative form, bytecode only
        // ever an accelerator. A cartridge whose only entry point is compiled
        // is not portable across Lua builds.
        if (body->get().entry.type == "lua-bytecode") {
            add(r, Severity::warning,
                "entry point is bytecode; source form is the normative representation",
                body->get().entry.module);
        }
    }
    if (auto body = m.as_assets()) {
        for (const auto& a : body->get().entries) {
            require_entry(a.path, "asset");
        }
    }
    if (auto body = m.as_plugin()) {
        require_entry(body->get().entry, "plugin entry");
    }
    if (auto body = m.as_template()) {
        check_template_ownership(c, body->get(), r);
    }

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
