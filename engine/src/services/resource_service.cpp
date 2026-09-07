// SPDX-License-Identifier: MIT

#include "services/services.hpp"

#include <algorithm>
#include <system_error>

namespace squared::pg {
namespace {

/// Manifests are read through sqcart rather than by a second parser here.
/// sqcart already enforces the container profile, the path rules, the
/// resource limits and the envelope, and it is linked from vendored in-tree
/// source regardless. A second manifest reader would be a second chance to
/// disagree about what a manifest means.
[[nodiscard]] sqcart::OpenOptions index_open_options() {
    sqcart::OpenOptions options;
    // Envelope validation only at index time (D-012); the typed services
    // check the body when they resolve. Integrity verification costs a full
    // read of every entry, so it stays off until a resource is actually used.
    options.conformance      = sqcart::Conformance::strict;
    options.verify_integrity = false;
    // Kits and packages legitimately ship compiled artefacts; containment is
    // an archive property and loading is a runtime decision.
    options.reject_native_code = false;
    return options;
}

[[nodiscard]] bool is_resource_directory(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::is_directory(path, ec)) return false;
    return std::filesystem::exists(path / "SQ-INF" / "manifest.json", ec);
}

[[nodiscard]] bool is_archive(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec) && path.extension() == ".sq";
}

/// Collect candidate resource locations under one root, sorted.
///
/// §2.8.4 requires deterministic discovery: roots in declared order, entries
/// within a root sorted. Filesystem enumeration order is not stable across
/// hosts, so the sort is what makes a duplicate-identity conflict report the
/// same two paths on every machine.
void collect(const std::filesystem::path& root, std::vector<std::filesystem::path>& out) {
    std::error_code ec;
    std::vector<std::filesystem::path> entries;
    for (std::filesystem::directory_iterator it(root, ec), end; it != end; it.increment(ec)) {
        if (ec) break;
        entries.push_back(it->path());
    }
    std::sort(entries.begin(), entries.end());

    for (const std::filesystem::path& entry : entries) {
        if (is_archive(entry) || is_resource_directory(entry)) {
            out.push_back(entry);
            continue;
        }
        // A namespace directory (resources/templates/) holds resources; a
        // resource directory holds a manifest. Recursing only into the former
        // keeps a template's own subdirectories from being mistaken for
        // resources.
        std::error_code dir_ec;
        if (std::filesystem::is_directory(entry, dir_ec)) collect(entry, out);
    }
}

[[nodiscard]] std::string manifest_string(std::optional<std::string_view> field) {
    return field ? std::string{*field} : std::string{};
}

}  // namespace

Result<void> ResourceService::build_index(const std::vector<std::filesystem::path>& roots,
                                          const Version& engine_version, bool strict_roots,
                                          std::vector<Diagnostic>& diagnostics) {
    std::vector<std::filesystem::path> candidates;

    for (const std::filesystem::path& root : roots) {
        std::error_code ec;
        if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
            if (strict_roots) {
                EngineError error = make_error(ErrorCategory::resource, "resource.root.missing",
                                               "resource root does not exist: " + root.string());
                error.path        = root.string();
                return Unexpected{std::move(error)};
            }
            diagnostics.push_back(Diagnostic{Severity::info,
                                             "resource root absent, skipped: " + root.string(), {}, root.string()});
            continue;
        }
        collect(root, candidates);
    }

    for (const std::filesystem::path& location : candidates) {
        auto opened = sqcart::Cartridge::open(location, index_open_options());
        if (!opened) {
            // A malformed resource is reported and skipped. Failing the whole
            // engine because one third-party kit is broken would make the
            // generator unusable for a reason the user did not cause; a
            // *duplicate identity* is different and does fail (§2.8.1).
            diagnostics.push_back(Diagnostic{Severity::warning,
                                             "skipping unreadable resource: " + opened.error().message,
                                             {},
                                             location.string()});
            continue;
        }

        const sqcart::Manifest& manifest = opened->manifest();

        // `kind` is an opaque token as of cartridge format 2, so mapping it
        // onto a ResourceKind is this engine's job and an unrecognised token
        // is an ordinary outcome rather than a defect. The index skips it and
        // says so: a sysroot may hold cartridges for tools that are not this
        // one, and refusing to scan past them would make the index brittle.
        auto kind = resource_kind_from_string(manifest.kind());
        if (!kind) {
            diagnostics.push_back(Diagnostic{Severity::info,
                                             "cartridge kind '" + std::string{manifest.kind()} +
                                                 "' is not a squared-pg resource kind, skipped",
                                             {}, location.string()});
            continue;
        }
        // A cartridge is a runnable application, not a generation input. It
        // may legitimately sit in a resource root; indexing it as a resource
        // would let a workflow ask for one as a template.
        if (*kind == ResourceKind::cartridge) {
            diagnostics.push_back(Diagnostic{Severity::info,
                                             "cartridge '" + manifest.id() + "' is not a generation resource",
                                             manifest.id(), location.string()});
            continue;
        }

        auto id = ResourceId::parse(manifest.id(), *kind);
        if (!id) {
            diagnostics.push_back(
                Diagnostic{Severity::warning,
                           "identity '" + manifest.id() + "' violates the identifier grammar, skipped",
                           manifest.id(), location.string()});
            continue;
        }

        auto version = Version::parse(manifest.version());
        if (!version) {
            diagnostics.push_back(Diagnostic{Severity::warning,
                                             "version '" + manifest.version() + "' is not valid SemVer, skipped",
                                             manifest.id(), location.string()});
            continue;
        }

        ResourceRecord record;
        record.id          = *id;
        record.kind        = *kind;
        record.version     = *version;
        record.form        = opened->exploded() ? ResourceForm::exploded : ResourceForm::archive;
        record.location    = location;
        record.title       = manifest_string(manifest.title());
        record.description = manifest_string(manifest.description());
        // `engine` moved out of the cartridge envelope and into this
        // engine's own section: the field's `id` named the consumer it was
        // addressed to, which is what the section key already says.
        if (const Value range = detail::manifest_extension(manifest, "requires.engine");
            range.as_string()) {
            record.engine_range = std::string{*range.as_string()};
        }
        for (const std::string& feature : manifest.requires_features()) {
            record.required_features.push_back(feature);
        }

        // §2.14.1: an unsatisfied requirement fails at resolution, never at
        // materialization. Indexing it and refusing later gives the workflow
        // an actionable error naming the resource; refusing to index would
        // give it "not found", which is a different and misleading claim.
        if (!record.engine_range.empty()) {
            auto range = VersionRange::parse(record.engine_range);
            if (range && !range->satisfied_by(engine_version)) {
                diagnostics.push_back(
                    Diagnostic{Severity::warning,
                               "resource requires engine " + record.engine_range + ", this engine is " +
                                   engine_version.to_string(),
                               record.id.str(), location.string()});
            }
        }

        if (is_reserved_namespace(record.id) &&
            location.string().find("resources/generator") == std::string::npos) {
            diagnostics.push_back(Diagnostic{Severity::warning,
                                             "identity uses the reserved squared_pg namespace",
                                             record.id.str(), location.string()});
        }

        if (auto inserted = index_.insert(std::move(record)); !inserted) {
            return inserted;
        }
    }

    return {};
}

Result<std::shared_ptr<sqcart::Cartridge>> ResourceService::open(const ResourceRecord& record) {
    const std::string key = record.id.str() + '@' + record.version.to_string();
    if (auto found = cache_.find(key); found != cache_.end()) return found->second;

    sqcart::OpenOptions options = index_open_options();
    // Payloads are about to be read into a workspace, so integrity is checked
    // now rather than at index time (§2.10.7): the cost is paid once, for the
    // resources actually used.
    options.verify_integrity = true;

    auto opened = sqcart::Cartridge::open(record.location, options);
    if (!opened) {
        EngineError error = make_error(ErrorCategory::resource, "resource.payload.unreadable",
                                       "could not open resource payload: " + opened.error().message);
        error.resource    = record.id.str();
        error.path        = record.location.string();
        return Unexpected{std::move(error)};
    }

    auto shared = std::make_shared<sqcart::Cartridge>(std::move(*opened));
    cache_.emplace(key, shared);
    return shared;
}

}  // namespace squared::pg
