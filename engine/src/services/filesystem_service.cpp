// SPDX-License-Identifier: MIT

#include "services/services.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <system_error>

#if defined(__unix__) || defined(__APPLE__) || defined(__ANDROID__)
#  include <fcntl.h>
#  include <unistd.h>
#  define SQUARED_PG_HAVE_POSIX_SYNC 1
#else
#  define SQUARED_PG_HAVE_POSIX_SYNC 0
#endif

namespace squared::pg {

// ---------------------------------------------------------------------------
// ServiceLocator
// ---------------------------------------------------------------------------

void ServiceLocator::register_service(Service& service) {
    const std::string name{service.service_name()};
    for (auto& entry : services_) {
        if (entry.first == name) {
            // §2.14.2: an extension adds, it never substitutes. Re-registering
            // a core service would make the engine's contract unverifiable, so
            // the last registration is ignored rather than honoured.
            return;
        }
    }
    services_.emplace_back(name, &service);
}

Service* ServiceLocator::find(std::string_view name) const {
    for (const auto& [registered, service] : services_) {
        if (registered == name) return service;
    }
    return nullptr;
}

std::vector<std::string> ServiceLocator::names() const {
    std::vector<std::string> out;
    out.reserve(services_.size());
    for (const auto& [name, service] : services_) out.push_back(name);
    return out;
}

// ---------------------------------------------------------------------------
// FilesystemService
// ---------------------------------------------------------------------------

namespace {

[[nodiscard]] EngineError filesystem_error(std::string code, std::string message,
                                           const std::filesystem::path& path,
                                           const std::error_code& ec = {}) {
    EngineError error = make_error(ErrorCategory::filesystem, std::move(code), std::move(message));
    error.path        = path.string();
    if (ec) {
        Value detail = Value::object();
        detail.set("system_error", ec.message());
        detail.set("errno", static_cast<std::int64_t>(ec.value()));
        error.diagnostics = std::move(detail);
    }
    return error;
}

}  // namespace

Result<void> FilesystemService::create_directories(const std::filesystem::path& path) const {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec && !std::filesystem::is_directory(path)) {
        return Unexpected{filesystem_error("filesystem.directory.create",
                                           "could not create directory", path, ec)};
    }
    return {};
}

Result<void> FilesystemService::write_file(const std::filesystem::path& path, std::string_view content,
                                           bool executable) const {
    if (path.has_parent_path()) {
        if (auto created = create_directories(path.parent_path()); !created) {
            return created;
        }
    }

    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) {
            return Unexpected{filesystem_error("filesystem.permission", "could not open file for writing", path)};
        }
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!out) {
            // A short write here is almost always a full filesystem, which
            // §2.15.3 gives its own code because the user's remedy differs.
            return Unexpected{filesystem_error("filesystem.space", "write failed before completion", path)};
        }
    }

    if (executable) {
        std::error_code ec;
        std::filesystem::permissions(path,
                                     std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec |
                                         std::filesystem::perms::others_exec,
                                     std::filesystem::perm_options::add, ec);
        if (ec) {
            return Unexpected{filesystem_error("filesystem.permission",
                                               "could not set the executable bit", path, ec)};
        }
    }
    return {};
}

Result<std::string> FilesystemService::read_file(const std::filesystem::path& path) const {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return Unexpected{filesystem_error("filesystem.read", "could not open file for reading", path)};
    }
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (in.bad()) {
        return Unexpected{filesystem_error("filesystem.read", "read failed", path)};
    }
    return content;
}

Result<void> FilesystemService::remove_all(const std::filesystem::path& path) const {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    if (ec) {
        return Unexpected{filesystem_error("filesystem.remove", "could not remove path", path, ec)};
    }
    return {};
}

Result<void> FilesystemService::rename(const std::filesystem::path& from,
                                       const std::filesystem::path& to) const {
    std::error_code ec;
    std::filesystem::rename(from, to, ec);
    if (ec) {
        // D-024 exists to make this impossible: staging is always on the
        // target filesystem, so EXDEV should never appear. If it does, the
        // staging location was chosen wrongly and the message should say so
        // rather than blaming the user's disk.
        EngineError error = filesystem_error("transaction.commit_failed",
                                             "could not move the staged workspace into place", to, ec);
        Value detail = error.diagnostics.is_null() ? Value::object() : error.diagnostics;
        detail.set("staged", from.string());
        error.diagnostics = std::move(detail);
        error.category    = ErrorCategory::transaction;
        return Unexpected{std::move(error)};
    }
    return {};
}

bool FilesystemService::exists(const std::filesystem::path& path) const {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

void FilesystemService::sync_path(const std::filesystem::path& path) const {
#if SQUARED_PG_HAVE_POSIX_SYNC
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd >= 0) {
        ::fsync(fd);
        ::close(fd);
    }
#else
    (void)path;
#endif
}

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

namespace detail {

OwnershipClass classify_path(const OwnershipRules& rules, std::string_view path,
                             std::vector<Diagnostic>* diagnostics) {
    struct Candidate {
        OwnershipClass cls{OwnershipClass::seeded};
        int            score{0};
        bool           found{false};
        std::string    pattern;
    };

    Candidate best;
    const auto consider = [&](const std::vector<std::string>& patterns, OwnershipClass cls) {
        for (const std::string& pattern : patterns) {
            if (!support::glob_match(pattern, path)) continue;
            const int score = support::glob_specificity(pattern);
            if (!best.found || score > best.score) {
                best = Candidate{cls, score, true, pattern};
            } else if (score == best.score && best.cls != cls) {
                // §2.8.6: two equally specific patterns disagreeing is a
                // manifest defect. Report it and keep the safer class rather
                // than resolving by declaration order, which would make the
                // outcome depend on how the file happened to be written.
                if (diagnostics != nullptr) {
                    diagnostics->push_back(Diagnostic{
                        Severity::warning,
                        "ownership patterns '" + best.pattern + "' and '" + pattern +
                            "' are equally specific and disagree; using the safer class",
                        {},
                        std::string{path}});
                }
                if (cls == OwnershipClass::seeded || cls == OwnershipClass::user) best.cls = cls;
            }
        }
    };

    consider(rules.generated, OwnershipClass::generated);
    consider(rules.user, OwnershipClass::seeded);
    // The cartridge format's `shared` has no v1 counterpart: it is what §2.7.10
    // reserves as `merged` and explicitly does not implement (D-020). Treating
    // it as seeded means the generator writes it once and never rewrites it,
    // which is the conservative reading.
    consider(rules.shared, OwnershipClass::seeded);

    if (!best.found) return OwnershipClass::seeded;
    return best.cls;
}

OwnershipRules ownership_rules(const sqcart::Manifest& manifest) {
    OwnershipRules out;
    const Value declared = manifest_extension(manifest, "ownership");
    const auto fill = [&](const char* key, std::vector<std::string>& into) {
        if (const Value* v = declared.find(key); v != nullptr) {
            if (const Array* arr = v->as_array(); arr != nullptr) {
                for (const Value& item : *arr) {
                    if (auto s = item.as_string()) into.emplace_back(*s);
                }
            }
        }
    };
    fill("generated", out.generated);
    fill("user", out.user);
    fill("shared", out.shared);
    return out;
}

Value manifest_extension(const sqcart::Manifest& manifest, std::string_view member) {
    // Cartridge format 2 carries this engine's manifest data under
    // `consumers.squared_pg` and never opens it (format spec 5.6). sqcart
    // hands it back as JSON text, which is the whole of the contract: the
    // library guarantees the bytes arrived, and every question about what
    // they mean is answered here.
    //
    // Note this reads the section, not raw_json(). Going through
    // Manifest::consumer() rather than re-parsing the whole document and
    // indexing into it keeps one fact in one place -- if the consumer
    // identity ever changes, it changes in kConsumerId and nowhere else.
    const auto section = manifest.consumer(kConsumerId);
    if (!section) return {};
    auto parsed = support::json_parse(*section);
    if (!parsed) return {};

    // `member` may be dotted: "requires.kits.required" walks three levels.
    //
    // The nesting is not new -- `requires` has always been a block, and
    // sqcart's parse_requires flattened it into KitBody::required_packages
    // and friends. With the typed bodies gone, the flattening has to happen
    // somewhere, and a path walk here is less code than a reader per field.
    const Value* value = &*parsed;
    std::size_t start = 0;
    while (start <= member.size()) {
        const std::size_t dot = member.find('.', start);
        const std::string_view segment = member.substr(start, dot - start);
        value = value->find(segment);
        if (value == nullptr) return {};
        if (dot == std::string_view::npos) break;
        start = dot + 1;
    }
    return *value;
}

bool looks_textual(std::string_view content) {
    const std::size_t window = std::min<std::size_t>(content.size(), 8192);
    return content.find('\0', 0) >= window;
}

Result<std::string> substitute(std::string_view content, const Value& parameters,
                               std::string_view origin_path) {
    std::string out;
    out.reserve(content.size());

    std::size_t i = 0;
    while (i < content.size()) {
        if (content[i] != '{' || i + 1 >= content.size() || content[i + 1] != '{') {
            out.push_back(content[i]);
            ++i;
            continue;
        }

        const std::size_t close = content.find("}}", i + 2);
        if (close == std::string_view::npos) {
            // An unterminated opener is far more likely to be C++ brace
            // initialisation than a broken placeholder, so it is copied
            // through rather than treated as an error.
            out.push_back(content[i]);
            ++i;
            continue;
        }

        std::string_view token = content.substr(i + 2, close - i - 2);
        while (!token.empty() && (token.front() == ' ' || token.front() == '\t')) token.remove_prefix(1);
        while (!token.empty() && (token.back() == ' ' || token.back() == '\t')) token.remove_suffix(1);

        // §2.8.8: `{{"{{"}}` escapes a literal brace pair.
        if (token == "\"{{\"") {
            out += "{{";
            i = close + 2;
            continue;
        }

        if (token.empty() || token.find_first_of(" \t\n{}") != std::string_view::npos) {
            out.push_back(content[i]);
            ++i;
            continue;
        }

        // A placeholder name is an identifier and nothing else. Without this
        // check `std::vector<int> v{{1, 2}}` would be read as a reference to a
        // parameter named "1, 2" and the whole file would fail to substitute.
        // Templates are mostly C++ source, so brace initialisation appearing
        // in one is a matter of time rather than a hypothetical.
        const bool identifier =
            (token.front() == '_' || (token.front() >= 'a' && token.front() <= 'z') ||
             (token.front() >= 'A' && token.front() <= 'Z')) &&
            std::all_of(token.begin(), token.end(), [](char c) {
                return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                       (c >= '0' && c <= '9');
            });
        if (!identifier) {
            out.push_back(content[i]);
            ++i;
            continue;
        }

        const Value* value = parameters.find(token);
        if (value == nullptr || value->is_null()) {
            EngineError error =
                make_error(ErrorCategory::template_, "template.parameter.missing",
                           "no value for parameter '" + std::string{token} + "' referenced by " +
                               std::string{origin_path});
            error.path        = std::string{origin_path};
            error.recoverable = true;
            Value detail      = Value::object();
            detail.set("parameter", std::string{token});
            error.diagnostics = std::move(detail);
            return Unexpected{std::move(error)};
        }

        switch (value->kind()) {
            case ValueKind::string: out += *value->as_string(); break;
            case ValueKind::integer: out += std::to_string(*value->as_int()); break;
            case ValueKind::boolean: out += *value->as_bool() ? "true" : "false"; break;
            case ValueKind::number: {
                char buffer[40];
                std::snprintf(buffer, sizeof buffer, "%g", *value->as_number());
                out += buffer;
                break;
            }
            default: {
                EngineError error = make_error(
                    ErrorCategory::template_, "template.parameter.invalid",
                    "parameter '" + std::string{token} + "' is a " +
                        std::string{to_string(value->kind())} + " and cannot be substituted into text");
                error.path = std::string{origin_path};
                return Unexpected{std::move(error)};
            }
        }
        i = close + 2;
    }

    return out;
}

}  // namespace detail

}  // namespace squared::pg
