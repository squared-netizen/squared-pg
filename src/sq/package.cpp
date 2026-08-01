#include <squared/sq/package.hpp>

#include "sha256.hpp"

#define MINIZ_NO_ZLIB_APIS
#include <miniz.h>
#include <yyjson.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace squared::sq {
namespace {

constexpr std::string_view manifest_path = ".squared/manifest.json";
constexpr std::string_view inventory_path = ".squared/files.sha256";
constexpr std::size_t maximum_reported_issues = 128;

struct ArchiveCloser {
    void operator()(mz_zip_archive* archive) const noexcept
    {
        if (archive) {
            mz_zip_reader_end(archive);
            delete archive;
        }
    }
};

using ArchiveHandle = std::unique_ptr<mz_zip_archive, ArchiveCloser>;

struct Entry {
    mz_uint index{0};
    std::string path;
    std::uint64_t compressed{0};
    std::uint64_t expanded{0};
    std::uint32_t external_attributes{0};
    std::uint16_t method{0};
    bool directory{false};
    bool encrypted{false};
    bool supported{false};
};

class StagingCleanup {
public:
    StagingCleanup(
        std::filesystem::path& staging,
        bool preserve
    ) noexcept
        : staging_(staging), preserve_(preserve)
    {
    }

    StagingCleanup(const StagingCleanup&) = delete;
    StagingCleanup& operator=(const StagingCleanup&) = delete;

    ~StagingCleanup()
    {
        if (!committed_ && !preserve_ && !staging_.empty()) {
            std::error_code ignored;
            std::filesystem::remove_all(staging_, ignored);
        }
    }

    void commit() noexcept
    {
        committed_ = true;
    }

private:
    std::filesystem::path& staging_;
    bool preserve_{false};
    bool committed_{false};
};

Error make_error(
    ErrorCode code,
    Phase phase,
    std::string message,
    const std::filesystem::path& filesystem_path = {},
    std::string archive_path = {}
)
{
    Error error;
    error.code = code;
    error.phase = phase;
    error.message = std::move(message);
    error.filesystem_path = filesystem_path;
    error.archive_path = std::move(archive_path);
    return error;
}

Result<ArchiveHandle> open_archive(
    const std::filesystem::path& path,
    Phase phase
)
{
    std::error_code filesystem_error;
    if (!std::filesystem::is_regular_file(path, filesystem_error)) {
        return make_error(
            ErrorCode::FileNotFound,
            phase,
            "SQ archive is not a regular file",
            path
        );
    }

    auto archive = ArchiveHandle(new mz_zip_archive{});
    const std::string native_path = path.string();
    if (!mz_zip_reader_init_file(archive.get(), native_path.c_str(), 0)) {
        const mz_zip_error source = mz_zip_get_last_error(archive.get());
        return make_error(
            ErrorCode::InvalidArchive,
            phase,
            mz_zip_get_error_string(source),
            path
        );
    }
    return archive;
}

Result<std::vector<Entry>> read_entries(
    mz_zip_archive& archive,
    const std::filesystem::path& archive_path,
    Phase phase
)
{
    try {
        const mz_uint count = mz_zip_reader_get_num_files(&archive);
        std::vector<Entry> entries;
        entries.reserve(count);

        for (mz_uint index = 0; index < count; ++index) {
            mz_zip_archive_file_stat stat{};
            if (!mz_zip_reader_file_stat(&archive, index, &stat)) {
                return make_error(
                    ErrorCode::InvalidArchive,
                    phase,
                    "cannot read ZIP entry metadata",
                    archive_path
                );
            }
            Entry entry;
            entry.index = index;
            entry.path = stat.m_filename;
            entry.compressed = stat.m_comp_size;
            entry.expanded = stat.m_uncomp_size;
            entry.external_attributes = stat.m_external_attr;
            entry.method = stat.m_method;
            entry.directory = stat.m_is_directory == MZ_TRUE;
            entry.encrypted = stat.m_is_encrypted == MZ_TRUE;
            entry.supported =
                mz_zip_reader_is_file_supported(&archive, index) == MZ_TRUE;
            entries.push_back(std::move(entry));
        }
        return entries;
    } catch (const std::bad_alloc&) {
        return make_error(
            ErrorCode::ResourceLimit,
            phase,
            "memory allocation failed while reading ZIP directory",
            archive_path
        );
    } catch (...) {
        return make_error(
            ErrorCode::InternalError,
            phase,
            "unexpected failure while reading ZIP directory",
            archive_path
        );
    }
}

struct MemorySink {
    std::string data;
    std::uint64_t maximum{0};
    bool exceeded{false};
};

size_t write_memory(
    void* opaque,
    mz_uint64 offset,
    const void* data,
    size_t size
)
{
    auto& sink = *static_cast<MemorySink*>(opaque);
    if (offset != sink.data.size() ||
        size > sink.maximum - std::min(sink.maximum, offset)) {
        sink.exceeded = true;
        return 0;
    }
    try {
        sink.data.append(static_cast<const char*>(data), size);
        return size;
    } catch (...) {
        return 0;
    }
}

Result<std::string> extract_string(
    mz_zip_archive& archive,
    const Entry& entry,
    std::uint64_t maximum,
    const std::filesystem::path& archive_path,
    Phase phase
)
{
    if (entry.expanded > maximum) {
        return make_error(
            ErrorCode::ResourceLimit,
            phase,
            "archive entry exceeds configured memory limit",
            archive_path,
            entry.path
        );
    }
    MemorySink sink;
    sink.maximum = maximum;
    try {
        sink.data.reserve(static_cast<std::size_t>(entry.expanded));
    } catch (...) {
        return make_error(
            ErrorCode::ResourceLimit,
            phase,
            "memory allocation failed for archive entry",
            archive_path,
            entry.path
        );
    }
    if (!mz_zip_reader_extract_to_callback(
            &archive,
            entry.index,
            write_memory,
            &sink,
            0
        )) {
        return make_error(
            sink.exceeded ? ErrorCode::ResourceLimit
                          : ErrorCode::InvalidArchive,
            phase,
            sink.exceeded
                ? "archive entry exceeded configured output limit"
                : mz_zip_get_error_string(mz_zip_get_last_error(&archive)),
            archive_path,
            entry.path
        );
    }
    return std::move(sink.data);
}

bool duplicate_keys(yyjson_val* value)
{
    if (yyjson_is_obj(value)) {
        std::unordered_set<std::string> keys;
        keys.reserve(yyjson_obj_size(value));
        std::size_t index;
        std::size_t maximum;
        yyjson_val* key;
        yyjson_val* item;
        yyjson_obj_foreach(value, index, maximum, key, item) {
            std::string name{yyjson_get_str(key), yyjson_get_len(key)};
            if (!keys.emplace(std::move(name)).second) return true;
            if (duplicate_keys(item)) return true;
        }
    } else if (yyjson_is_arr(value)) {
        std::size_t index;
        std::size_t maximum;
        yyjson_val* item;
        yyjson_arr_foreach(value, index, maximum, item) {
            if (duplicate_keys(item)) return true;
        }
    }
    return false;
}

bool read_required_string(
    yyjson_val* root,
    const char* name,
    std::string& destination,
    std::string& message
)
{
    yyjson_val* value = yyjson_obj_get(root, name);
    if (!yyjson_is_str(value) || yyjson_get_len(value) == 0) {
        message = std::string("manifest field must be a non-empty string: ") +
                  name;
        return false;
    }
    destination.assign(yyjson_get_str(value), yyjson_get_len(value));
    return true;
}

bool portable_package_id(std::string_view value) noexcept
{
    if (value.empty() || value.size() > 160 ||
        std::isalnum(static_cast<unsigned char>(value.front())) == 0 ||
        std::isalnum(static_cast<unsigned char>(value.back())) == 0 ||
        value.find('.') == std::string_view::npos) {
        return false;
    }
    bool segment_start = true;
    bool segment_last_alnum = false;
    for (const unsigned char character : value) {
        if (character == '.') {
            if (segment_start || !segment_last_alnum) return false;
            segment_start = true;
            segment_last_alnum = false;
            continue;
        }
        if (segment_start && std::isalnum(character) == 0) return false;
        if (std::isalnum(character) == 0 &&
            character != '_' && character != '-') {
            return false;
        }
        segment_start = false;
        segment_last_alnum = std::isalnum(character) != 0;
    }
    return true;
}

bool portable_version(std::string_view value) noexcept
{
    if (value.empty() || value.size() > 80 ||
        std::isalnum(static_cast<unsigned char>(value.front())) == 0 ||
        std::isalnum(static_cast<unsigned char>(value.back())) == 0) {
        return false;
    }
    bool previous_dot = false;
    for (const unsigned char character : value) {
        const bool dot = character == '.';
        if (dot && previous_dot) return false;
        if (std::isalnum(character) == 0 &&
            character != '.' && character != '_' &&
            character != '-' && character != '+') {
            return false;
        }
        previous_dot = dot;
    }
    return true;
}

bool portable_cmake_target(std::string_view value) noexcept
{
    if (value.empty() || value.size() > 100 ||
        (std::isalpha(static_cast<unsigned char>(value.front())) == 0 &&
         value.front() != '_')) {
        return false;
    }
    for (const unsigned char character : value) {
        if (std::isalnum(character) == 0 && character != '_') return false;
    }
    return true;
}

bool portable_module_directory(std::string_view value) noexcept
{
    constexpr std::string_view prefix = "modules/";
    if (!value.starts_with(prefix) || value.size() <= prefix.size() ||
        value.back() == '/' || value.size() > 200) {
        return false;
    }
    for (const unsigned char character : value) {
        if (std::isalnum(character) == 0 &&
            character != '/' && character != '.' &&
            character != '_' && character != '-') {
            return false;
        }
    }
    return value.find("..") == std::string_view::npos &&
           value.find("//") == std::string_view::npos;
}

bool portable_template_directory(std::string_view value) noexcept
{
    if (value.empty() || value.front() == '/' || value.back() == '/' ||
        value.size() > 200) {
        return false;
    }
    std::size_t start = 0;
    while (start < value.size()) {
        const std::size_t slash = value.find('/', start);
        const std::size_t end =
            slash == std::string_view::npos ? value.size() : slash;
        const std::string_view component = value.substr(start, end - start);
        if (component.empty() || component == "." || component == "..") {
            return false;
        }
        for (const unsigned char character : component) {
            if (std::isalnum(character) == 0 &&
                character != '.' && character != '_' && character != '-') {
                return false;
            }
        }
        if (slash == std::string_view::npos) break;
        start = slash + 1;
    }
    return true;
}

bool portable_template_field(std::string_view value) noexcept
{
    if (value.empty() || value.size() > 64 ||
        (value.front() < 'a' || value.front() > 'z')) {
        return false;
    }
    for (const unsigned char character : value) {
        if ((character < 'a' || character > 'z') &&
            (character < '0' || character > '9') &&
            character != '_') {
            return false;
        }
    }
    return true;
}

bool portable_template_variable(std::string_view value) noexcept
{
    if (value.empty() || value.size() > 64 ||
        (value.front() < 'A' || value.front() > 'Z')) {
        return false;
    }
    for (const unsigned char character : value) {
        if ((character < 'A' || character > 'Z') &&
            (character < '0' || character > '9') &&
            character != '_') {
            return false;
        }
    }
    return true;
}

Result<Manifest> parse_manifest(
    std::string json,
    const std::filesystem::path& archive
)
{
    yyjson_read_err read_error{};
    yyjson_doc* document = yyjson_read_opts(
        json.data(),
        json.size(),
        YYJSON_READ_NOFLAG,
        nullptr,
        &read_error
    );
    if (!document) {
        return make_error(
            ErrorCode::InvalidManifest,
            Phase::Inspecting,
            read_error.msg ? read_error.msg : "invalid manifest JSON",
            archive,
            std::string(manifest_path)
        );
    }

    std::unique_ptr<yyjson_doc, decltype(&yyjson_doc_free)> guard(
        document,
        yyjson_doc_free
    );
    yyjson_val* root = yyjson_doc_get_root(document);
    if (!yyjson_is_obj(root)) {
        return make_error(
            ErrorCode::InvalidManifest,
            Phase::Inspecting,
            "manifest root must be an object",
            archive,
            std::string(manifest_path)
        );
    }
    try {
        if (duplicate_keys(root)) {
            return make_error(
                ErrorCode::InvalidManifest,
                Phase::Inspecting,
                "manifest contains duplicate object keys",
                archive,
                std::string(manifest_path)
            );
        }

        std::string format;
        std::string kind;
        std::string message;
        Manifest manifest;
        if (!read_required_string(root, "format", format, message) ||
            !read_required_string(root, "kind", kind, message) ||
            !read_required_string(root, "id", manifest.id, message) ||
            !read_required_string(root, "version", manifest.version, message) ||
            !read_required_string(root, "name", manifest.name, message)) {
            return make_error(
                ErrorCode::InvalidManifest,
                Phase::Inspecting,
                message,
                archive,
                std::string(manifest_path)
            );
        }
        if (format != "dev.squarednetizen.sq") {
            return make_error(
                ErrorCode::UnsupportedFormat,
                Phase::Inspecting,
                "unsupported SQ format identifier",
                archive,
                std::string(manifest_path)
            );
        }
        if (!portable_package_id(manifest.id)) {
            return make_error(
                ErrorCode::InvalidManifest,
                Phase::Inspecting,
                "manifest id is not a portable package identifier",
                archive,
                std::string(manifest_path)
            );
        }
        if (!portable_version(manifest.version)) {
            return make_error(
                ErrorCode::InvalidManifest,
                Phase::Inspecting,
                "manifest version is not a portable version identifier",
                archive,
                std::string(manifest_path)
            );
        }
        yyjson_val* version = yyjson_obj_get(root, "format_version");
        if (!yyjson_is_uint(version) || yyjson_get_uint(version) != 0) {
            return make_error(
                ErrorCode::UnsupportedFormat,
                Phase::Inspecting,
                "only SQ development format version 0 is supported",
                archive,
                std::string(manifest_path)
            );
        }
        manifest.format_version = 0;
        if (kind == "template") {
            manifest.kind = PackageKind::Template;
        } else if (kind == "module") {
            manifest.kind = PackageKind::Module;
        } else if (kind == "kit") {
            manifest.kind = PackageKind::Kit;
        } else if (kind == "cartridge") {
            manifest.kind = PackageKind::Cartridge;
        } else {
            return make_error(
                ErrorCode::UnsupportedKind,
                Phase::Inspecting,
                "unsupported SQ package kind: " + kind,
                archive,
                std::string(manifest_path)
            );
        }
        if (manifest.kind == PackageKind::Module) {
            yyjson_val* module = yyjson_obj_get(root, "module");
            if (!yyjson_is_obj(module)) {
                return make_error(
                    ErrorCode::InvalidManifest,
                    Phase::Inspecting,
                    "module package requires a module object",
                    archive,
                    std::string(manifest_path)
                );
            }
            ModuleContribution contribution;
            if (!read_required_string(
                    module,
                    "directory",
                    contribution.directory,
                    message
                ) ||
                !read_required_string(
                    module,
                    "cmake_target",
                    contribution.cmake_target,
                    message
                )) {
                return make_error(
                    ErrorCode::InvalidManifest,
                    Phase::Inspecting,
                    message,
                    archive,
                    std::string(manifest_path)
                );
            }
            if (!portable_module_directory(contribution.directory) ||
                !portable_cmake_target(contribution.cmake_target)) {
                return make_error(
                    ErrorCode::InvalidManifest,
                    Phase::Inspecting,
                    "module directory or CMake target is not portable",
                    archive,
                    std::string(manifest_path)
                );
            }
            yyjson_val* requirements = yyjson_obj_get(module, "requires");
            if (requirements &&
                (!yyjson_is_arr(requirements) ||
                 yyjson_arr_size(requirements) > 64)) {
                return make_error(
                    ErrorCode::InvalidManifest,
                    Phase::Inspecting,
                    "module requires must be an array with at most 64 entries",
                    archive,
                    std::string(manifest_path)
                );
            }
            std::unordered_set<std::string> requirement_keys;
            if (requirements) {
                std::size_t requirement_index;
                std::size_t requirement_maximum;
                yyjson_val* requirement_value;
                yyjson_arr_foreach(
                    requirements,
                    requirement_index,
                    requirement_maximum,
                    requirement_value
                ) {
                    if (!yyjson_is_obj(requirement_value)) {
                        return make_error(
                            ErrorCode::InvalidManifest,
                            Phase::Inspecting,
                            "module requirement must be an object",
                            archive,
                            std::string(manifest_path)
                        );
                    }
                    std::string requirement_kind;
                    PackageRequirement requirement;
                    if (!read_required_string(
                            requirement_value,
                            "kind",
                            requirement_kind,
                            message
                        ) ||
                        !read_required_string(
                            requirement_value,
                            "id",
                            requirement.id,
                            message
                        ) ||
                        !read_required_string(
                            requirement_value,
                            "version",
                            requirement.version,
                            message
                        )) {
                        return make_error(
                            ErrorCode::InvalidManifest,
                            Phase::Inspecting,
                            message,
                            archive,
                            std::string(manifest_path)
                        );
                    }
                    if (requirement_kind != "module") {
                        return make_error(
                            ErrorCode::UnsupportedKind,
                            Phase::Inspecting,
                            "module version 0 requirements must be modules",
                            archive,
                            std::string(manifest_path)
                        );
                    }
                    requirement.kind = PackageKind::Module;
                    const std::string key =
                        requirement.id + "@" + requirement.version;
                    if (!portable_package_id(requirement.id) ||
                        !portable_version(requirement.version) ||
                        !requirement_keys.emplace(key).second) {
                        return make_error(
                            ErrorCode::InvalidManifest,
                            Phase::Inspecting,
                            "module requirement is invalid or duplicated",
                            archive,
                            std::string(manifest_path)
                        );
                    }
                    contribution.requirements.push_back(
                        std::move(requirement)
                    );
                }
            }
            manifest.module = std::move(contribution);
        } else if (manifest.kind == PackageKind::Template) {
            yyjson_val* template_value = yyjson_obj_get(root, "template");
            if (!yyjson_is_obj(template_value)) {
                return make_error(
                    ErrorCode::InvalidManifest,
                    Phase::Inspecting,
                    "template package requires a template object",
                    archive,
                    std::string(manifest_path)
                );
            }

            TemplateContribution contribution;
            if (!read_required_string(
                    template_value,
                    "profile",
                    contribution.profile,
                    message
                ) ||
                !read_required_string(
                    template_value,
                    "directory",
                    contribution.directory,
                    message
                )) {
                return make_error(
                    ErrorCode::InvalidManifest,
                    Phase::Inspecting,
                    message,
                    archive,
                    std::string(manifest_path)
                );
            }
            if (!portable_template_field(contribution.profile) ||
                !portable_template_directory(contribution.directory)) {
                return make_error(
                    ErrorCode::InvalidManifest,
                    Phase::Inspecting,
                    "template profile or directory is not portable",
                    archive,
                    std::string(manifest_path)
                );
            }

            yyjson_val* hooks =
                yyjson_obj_get(template_value, "executable_hooks");
            if (hooks) {
                if (!yyjson_is_bool(hooks)) {
                    return make_error(
                        ErrorCode::InvalidManifest,
                        Phase::Inspecting,
                        "template executable_hooks must be a boolean",
                        archive,
                        std::string(manifest_path)
                    );
                }
                contribution.executable_hooks = yyjson_get_bool(hooks);
            }
            if (contribution.executable_hooks) {
                return make_error(
                    ErrorCode::UnsupportedFormat,
                    Phase::Inspecting,
                    "SQ development format version 0 rejects template hooks",
                    archive,
                    std::string(manifest_path)
                );
            }

            yyjson_val* fields = yyjson_obj_get(template_value, "fields");
            if (!yyjson_is_arr(fields) ||
                yyjson_arr_size(fields) == 0 ||
                yyjson_arr_size(fields) > 32) {
                return make_error(
                    ErrorCode::InvalidManifest,
                    Phase::Inspecting,
                    "template fields must be an array with 1 to 32 entries",
                    archive,
                    std::string(manifest_path)
                );
            }
            std::unordered_set<std::string> field_names;
            std::unordered_set<std::string> variables;
            std::size_t field_index;
            std::size_t field_maximum;
            yyjson_val* field_value;
            yyjson_arr_foreach(
                fields,
                field_index,
                field_maximum,
                field_value
            ) {
                if (!yyjson_is_obj(field_value)) {
                    return make_error(
                        ErrorCode::InvalidManifest,
                        Phase::Inspecting,
                        "template field must be an object",
                        archive,
                        std::string(manifest_path)
                    );
                }
                TemplateField field;
                if (!read_required_string(
                        field_value,
                        "name",
                        field.name,
                        message
                    ) ||
                    !read_required_string(
                        field_value,
                        "variable",
                        field.variable,
                        message
                    )) {
                    return make_error(
                        ErrorCode::InvalidManifest,
                        Phase::Inspecting,
                        message,
                        archive,
                        std::string(manifest_path)
                    );
                }
                if (!portable_template_field(field.name) ||
                    !portable_template_variable(field.variable) ||
                    !field_names.emplace(field.name).second ||
                    !variables.emplace(field.variable).second) {
                    return make_error(
                        ErrorCode::InvalidManifest,
                        Phase::Inspecting,
                        "template field name or variable is invalid or duplicated",
                        archive,
                        std::string(manifest_path)
                    );
                }
                yyjson_val* required =
                    yyjson_obj_get(field_value, "required");
                if (required) {
                    if (!yyjson_is_bool(required)) {
                        return make_error(
                            ErrorCode::InvalidManifest,
                            Phase::Inspecting,
                            "template field required must be a boolean",
                            archive,
                            std::string(manifest_path)
                        );
                    }
                    field.required = yyjson_get_bool(required);
                }
                yyjson_val* default_value =
                    yyjson_obj_get(field_value, "default");
                if (default_value) {
                    if (!yyjson_is_str(default_value)) {
                        return make_error(
                            ErrorCode::InvalidManifest,
                            Phase::Inspecting,
                            "template field default must be a string",
                            archive,
                            std::string(manifest_path)
                        );
                    }
                    field.default_value.assign(
                        yyjson_get_str(default_value),
                        yyjson_get_len(default_value)
                    );
                }
                contribution.fields.push_back(std::move(field));
            }

            yyjson_val* requirements =
                yyjson_obj_get(template_value, "requires");
            if (!yyjson_is_arr(requirements) ||
                yyjson_arr_size(requirements) > 64) {
                return make_error(
                    ErrorCode::InvalidManifest,
                    Phase::Inspecting,
                    "template requires must be an array with at most 64 entries",
                    archive,
                    std::string(manifest_path)
                );
            }
            std::unordered_set<std::string> requirement_keys;
            std::size_t requirement_index;
            std::size_t requirement_maximum;
            yyjson_val* requirement_value;
            yyjson_arr_foreach(
                requirements,
                requirement_index,
                requirement_maximum,
                requirement_value
            ) {
                if (!yyjson_is_obj(requirement_value)) {
                    return make_error(
                        ErrorCode::InvalidManifest,
                        Phase::Inspecting,
                        "template requirement must be an object",
                        archive,
                        std::string(manifest_path)
                    );
                }
                std::string requirement_kind;
                PackageRequirement requirement;
                if (!read_required_string(
                        requirement_value,
                        "kind",
                        requirement_kind,
                        message
                    ) ||
                    !read_required_string(
                        requirement_value,
                        "id",
                        requirement.id,
                        message
                    ) ||
                    !read_required_string(
                        requirement_value,
                        "version",
                        requirement.version,
                        message
                    )) {
                    return make_error(
                        ErrorCode::InvalidManifest,
                        Phase::Inspecting,
                        message,
                        archive,
                        std::string(manifest_path)
                    );
                }
                if (requirement_kind != "module") {
                    return make_error(
                        ErrorCode::UnsupportedKind,
                        Phase::Inspecting,
                        "template version 0 requirements must be modules",
                        archive,
                        std::string(manifest_path)
                    );
                }
                requirement.kind = PackageKind::Module;
                const std::string key =
                    requirement.id + "@" + requirement.version;
                if (!portable_package_id(requirement.id) ||
                    !portable_version(requirement.version) ||
                    !requirement_keys.emplace(key).second) {
                    return make_error(
                        ErrorCode::InvalidManifest,
                        Phase::Inspecting,
                        "template requirement is invalid or duplicated",
                        archive,
                        std::string(manifest_path)
                    );
                }
                contribution.requirements.push_back(
                    std::move(requirement)
                );
            }
            manifest.project_template = std::move(contribution);
        }
        yyjson_val* description = yyjson_obj_get(root, "description");
        if (description) {
            if (!yyjson_is_str(description)) {
                return make_error(
                    ErrorCode::InvalidManifest,
                    Phase::Inspecting,
                    "manifest description must be a string",
                    archive,
                    std::string(manifest_path)
                );
            }
            manifest.description.assign(
                yyjson_get_str(description),
                yyjson_get_len(description)
            );
        }
        manifest.original_json = std::move(json);
        return manifest;
    } catch (const std::bad_alloc&) {
        return make_error(
            ErrorCode::ResourceLimit,
            Phase::Inspecting,
            "memory allocation failed while parsing manifest",
            archive,
            std::string(manifest_path)
        );
    }
}

bool allowed_path_character(unsigned char character) noexcept
{
    return std::isalnum(character) != 0 ||
           character == '.' || character == '_' || character == '-' ||
           character == '+' || character == '@' ||
           character == '{' || character == '}';
}

bool validate_path(
    std::string_view path,
    const ResourceLimits& limits,
    std::string& message
)
{
    if (path.empty() || path.front() == '/' ||
        path.size() > limits.maximum_path_bytes) {
        message = "path is empty, absolute, or too long";
        return false;
    }
    std::size_t start = 0;
    while (start < path.size()) {
        const std::size_t slash = path.find('/', start);
        const std::size_t end =
            slash == std::string_view::npos ? path.size() : slash;
        const std::string_view component = path.substr(start, end - start);
        if (component.empty() || component == "." || component == ".." ||
            component.size() > limits.maximum_component_bytes) {
            message = "path contains an unsafe or oversized component";
            return false;
        }
        for (const unsigned char character : component) {
            if (!allowed_path_character(character)) {
                message = "path contains a non-portable character";
                return false;
            }
        }
        if (slash == std::string_view::npos) break;
        start = slash + 1;
        if (start == path.size()) break;  // directory marker
    }
    return true;
}

bool is_special_entry(const Entry& entry) noexcept
{
    const std::uint32_t mode = entry.external_attributes >> 16U;
    if (mode == 0) return false;
    const std::uint32_t type = mode & 0170000U;
    return type != 0 && type != 0100000U && type != 0040000U;
}

std::string lowercase(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        }
    );
    return value;
}

bool is_hex_digest(std::string_view value) noexcept
{
    if (value.size() != 64) return false;
    for (const unsigned char character : value) {
        if (!(character >= '0' && character <= '9') &&
            !(character >= 'a' && character <= 'f')) {
            return false;
        }
    }
    return true;
}

Result<std::unordered_map<std::string, std::string>> parse_inventory(
    std::string_view text,
    const ResourceLimits& limits,
    const std::filesystem::path& archive
)
{
    std::unordered_map<std::string, std::string> inventory;
    std::size_t offset = 0;
    try {
        while (offset < text.size()) {
            const std::size_t newline = text.find('\n', offset);
            const std::size_t end =
                newline == std::string_view::npos ? text.size() : newline;
            std::string_view line = text.substr(offset, end - offset);
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
            if (line.size() < 67 || line[64] != ' ' || line[65] != ' ') {
                return make_error(
                    ErrorCode::InvalidManifest,
                    Phase::Validating,
                    "invalid files.sha256 record",
                    archive,
                    std::string(inventory_path)
                );
            }
            const std::string_view digest = line.substr(0, 64);
            const std::string_view path = line.substr(66);
            std::string path_message;
            if (!is_hex_digest(digest) ||
                !validate_path(path, limits, path_message) ||
                path == inventory_path) {
                return make_error(
                    ErrorCode::InvalidManifest,
                    Phase::Validating,
                    "invalid files.sha256 path or digest",
                    archive,
                    std::string(path)
                );
            }
            if (!inventory.emplace(
                    std::string(path),
                    std::string(digest)
                ).second) {
                return make_error(
                    ErrorCode::DuplicatePath,
                    Phase::Validating,
                    "duplicate files.sha256 record",
                    archive,
                    std::string(path)
                );
            }
            offset = newline == std::string_view::npos
                ? text.size()
                : newline + 1;
        }
        return inventory;
    } catch (const std::bad_alloc&) {
        return make_error(
            ErrorCode::ResourceLimit,
            Phase::Validating,
            "memory allocation failed while parsing files.sha256",
            archive,
            std::string(inventory_path)
        );
    }
}

struct HashSink {
    internal::Sha256 hash;
    std::uint64_t written{0};
    std::uint64_t maximum{0};
    bool exceeded{false};
};

size_t write_hash(
    void* opaque,
    mz_uint64 offset,
    const void* data,
    size_t size
)
{
    auto& sink = *static_cast<HashSink*>(opaque);
    if (offset != sink.written ||
        size > sink.maximum - std::min(sink.maximum, sink.written)) {
        sink.exceeded = true;
        return 0;
    }
    sink.hash.update(std::as_bytes(std::span(
        static_cast<const char*>(data),
        size
    )));
    sink.written += size;
    return size;
}

Result<std::string> hash_entry(
    mz_zip_archive& archive,
    const Entry& entry,
    std::uint64_t maximum,
    const std::filesystem::path& archive_path
)
{
    HashSink sink;
    sink.maximum = maximum;
    if (!mz_zip_reader_extract_to_callback(
            &archive,
            entry.index,
            write_hash,
            &sink,
            0
        )) {
        return make_error(
            sink.exceeded ? ErrorCode::ResourceLimit
                          : ErrorCode::InvalidArchive,
            Phase::Validating,
            sink.exceeded
                ? "entry exceeded configured output limit"
                : mz_zip_get_error_string(mz_zip_get_last_error(&archive)),
            archive_path,
            entry.path
        );
    }
    return sink.hash.finish_hex();
}

void add_issue(
    ValidationReport& report,
    ErrorCode code,
    std::string message,
    std::string path
)
{
    if (report.issues.size() >= maximum_reported_issues) return;
    report.issues.push_back(ValidationIssue{
        IssueSeverity::Error,
        code,
        std::move(message),
        std::move(path)
    });
}

std::atomic<std::uint64_t> staging_counter{0};

std::filesystem::path staging_path(
    const std::filesystem::path& destination
)
{
    const auto count = ++staging_counter;
    const auto ticks = std::chrono::steady_clock::now()
                           .time_since_epoch()
                           .count();
    return destination.string() + ".sq-staging-" +
           std::to_string(ticks) + "-" + std::to_string(count);
}

struct FileSink {
    std::ofstream output;
    internal::Sha256 hash;
    std::uint64_t written{0};
    std::uint64_t maximum{0};
    bool failed{false};
};

size_t write_file(
    void* opaque,
    mz_uint64 offset,
    const void* data,
    size_t size
)
{
    auto& sink = *static_cast<FileSink*>(opaque);
    if (offset != sink.written ||
        size > sink.maximum - std::min(sink.maximum, sink.written)) {
        sink.failed = true;
        return 0;
    }
    sink.output.write(static_cast<const char*>(data), size);
    if (!sink.output) {
        sink.failed = true;
        return 0;
    }
    sink.hash.update(std::as_bytes(std::span(
        static_cast<const char*>(data),
        size
    )));
    sink.written += size;
    return size;
}

}  // namespace

class Package::Implementation {
public:
    std::filesystem::path archive;
    Manifest manifest;
};

Package::Package(std::unique_ptr<Implementation> implementation)
    : implementation_(std::move(implementation))
{
}

Package::Package(Package&&) noexcept = default;
Package& Package::operator=(Package&&) noexcept = default;
Package::~Package() = default;

Result<Package> Package::open(
    const std::filesystem::path& archive_path
) noexcept
{
    try {
        auto opened = open_archive(archive_path, Phase::Opening);
        if (!opened) return opened.error();
        ArchiveHandle archive = std::move(opened.value());
        auto listed = read_entries(
            *archive,
            archive_path,
            Phase::Inspecting
        );
        if (!listed) return listed.error();

        const Entry* manifest_entry = nullptr;
        for (const auto& entry : listed.value()) {
            if (entry.path == manifest_path) {
                if (manifest_entry || entry.directory) {
                    return make_error(
                        ErrorCode::DuplicatePath,
                        Phase::Inspecting,
                        "manifest entry is duplicated or not a regular file",
                        archive_path,
                        entry.path
                    );
                }
                manifest_entry = &entry;
            }
        }
        if (!manifest_entry) {
            return make_error(
                ErrorCode::InvalidManifest,
                Phase::Inspecting,
                "SQ archive does not contain .squared/manifest.json",
                archive_path
            );
        }
        auto json = extract_string(
            *archive,
            *manifest_entry,
            1024ULL * 1024ULL,
            archive_path,
            Phase::Inspecting
        );
        if (!json) return json.error();
        auto parsed = parse_manifest(std::move(json.value()), archive_path);
        if (!parsed) return parsed.error();

        auto implementation = std::make_unique<Implementation>();
        implementation->archive = archive_path;
        implementation->manifest = std::move(parsed.value());
        return Package(std::move(implementation));
    } catch (const std::bad_alloc&) {
        return make_error(
            ErrorCode::ResourceLimit,
            Phase::Opening,
            "memory allocation failed while opening SQ package",
            archive_path
        );
    } catch (const std::exception& exception) {
        return make_error(
            ErrorCode::InternalError,
            Phase::Opening,
            exception.what(),
            archive_path
        );
    } catch (...) {
        return make_error(
            ErrorCode::InternalError,
            Phase::Opening,
            "unexpected failure while opening SQ package",
            archive_path
        );
    }
}

const Manifest& Package::manifest() const noexcept
{
    return implementation_->manifest;
}

Result<ValidationReport> Package::validate(
    const ValidationPolicy& policy
) const noexcept
{
    try {
        auto opened = open_archive(
            implementation_->archive,
            Phase::Validating
        );
        if (!opened) return opened.error();
        ArchiveHandle archive = std::move(opened.value());
        auto listed = read_entries(
            *archive,
            implementation_->archive,
            Phase::Validating
        );
        if (!listed) return listed.error();
        const auto& entries = listed.value();

        ValidationReport report;
        report.manifest = implementation_->manifest;
        report.entry_count = entries.size();
        if (policy.expected_kind &&
            *policy.expected_kind != report.manifest.kind) {
            add_issue(
                report,
                ErrorCode::UnsupportedKind,
                "package kind does not match expected kind",
                std::string(manifest_path)
            );
        }
        if (entries.size() > policy.limits.maximum_entries) {
            return make_error(
                ErrorCode::ResourceLimit,
                Phase::Validating,
                "archive entry count exceeds configured limit",
                implementation_->archive
            );
        }

        std::unordered_set<std::string> exact_paths;
        std::unordered_set<std::string> folded_paths;
        const Entry* inventory_entry = nullptr;
        std::uint64_t total = 0;

        for (const auto& entry : entries) {
            std::string message;
            if (!validate_path(entry.path, policy.limits, message)) {
                return make_error(
                    ErrorCode::UnsafePath,
                    Phase::Validating,
                    message,
                    implementation_->archive,
                    entry.path
                );
            }
            if (!entry.path.starts_with(".squared/") &&
                !entry.path.starts_with("content/")) {
                return make_error(
                    ErrorCode::UnsafePath,
                    Phase::Validating,
                    "entry is outside the .squared/ and content/ roots",
                    implementation_->archive,
                    entry.path
                );
            }
            if (!exact_paths.emplace(entry.path).second) {
                return make_error(
                    ErrorCode::DuplicatePath,
                    Phase::Validating,
                    "duplicate archive path",
                    implementation_->archive,
                    entry.path
                );
            }
            if (policy.reject_case_collisions &&
                !folded_paths.emplace(lowercase(entry.path)).second) {
                return make_error(
                    ErrorCode::DuplicatePath,
                    Phase::Validating,
                    "case-insensitive archive path collision",
                    implementation_->archive,
                    entry.path
                );
            }
            if (is_special_entry(entry)) {
                return make_error(
                    ErrorCode::UnsupportedArchive,
                    Phase::Validating,
                    "special filesystem entries are not supported",
                    implementation_->archive,
                    entry.path
                );
            }
            if (entry.encrypted || (!entry.directory && !entry.supported) ||
                (!entry.directory && entry.method != 0 && entry.method != 8)) {
                return make_error(
                    ErrorCode::UnsupportedArchive,
                    Phase::Validating,
                    "entry uses encryption or unsupported compression",
                    implementation_->archive,
                    entry.path
                );
            }
            if (!entry.directory) {
                if (entry.expanded > policy.limits.maximum_file_bytes ||
                    total > policy.limits.maximum_total_bytes -
                                std::min(
                                    policy.limits.maximum_total_bytes,
                                    entry.expanded
                                )) {
                    return make_error(
                        ErrorCode::ResourceLimit,
                        Phase::Validating,
                        "expanded archive size exceeds configured limit",
                        implementation_->archive,
                        entry.path
                    );
                }
                total += entry.expanded;
                report.compressed_bytes += entry.compressed;
                report.expanded_bytes += entry.expanded;
                if (entry.expanded != 0) {
                    const double ratio = entry.compressed == 0
                        ? std::numeric_limits<double>::infinity()
                        : static_cast<double>(entry.expanded) /
                              static_cast<double>(entry.compressed);
                    if (ratio > policy.limits.maximum_compression_ratio) {
                        return make_error(
                            ErrorCode::ResourceLimit,
                            Phase::Validating,
                            "entry compression ratio exceeds configured limit",
                            implementation_->archive,
                            entry.path
                        );
                    }
                }
            }
            if (entry.path == inventory_path) {
                if (inventory_entry || entry.directory) {
                    return make_error(
                        ErrorCode::DuplicatePath,
                        Phase::Validating,
                        "files.sha256 is duplicated or not a regular file",
                        implementation_->archive,
                        entry.path
                    );
                }
                inventory_entry = &entry;
            }
        }

        if (report.manifest.module) {
            const std::string module_cmake =
                "content/" +
                report.manifest.module->directory +
                "/CMakeLists.txt";
            const auto match = std::find_if(
                entries.begin(),
                entries.end(),
                [&module_cmake](const Entry& entry) {
                    return entry.path == module_cmake && !entry.directory;
                }
            );
            if (match == entries.end()) {
                add_issue(
                    report,
                    ErrorCode::InvalidManifest,
                    "declared module directory has no CMakeLists.txt",
                    module_cmake
                );
            }
        }
        if (report.manifest.project_template) {
            const std::string marker_prefix =
                "content/" +
                report.manifest.project_template->directory +
                "/";
            const std::string marker =
                marker_prefix + ".squared-pg.lua";
            const std::string legacy_marker =
                marker_prefix + ".sdl-pg.lua";
            const auto match = std::find_if(
                entries.begin(),
                entries.end(),
                [&marker, &legacy_marker](const Entry& entry) {
                    return
                        (entry.path == marker ||
                         entry.path == legacy_marker) &&
                        !entry.directory;
                }
            );
            if (match == entries.end()) {
                add_issue(
                    report,
                    ErrorCode::InvalidManifest,
                    "declared template directory has no "
                    ".squared-pg.lua marker",
                    marker
                );
            }
        }

        if (!inventory_entry) {
            return make_error(
                ErrorCode::InvalidManifest,
                Phase::Validating,
                "SQ archive does not contain .squared/files.sha256",
                implementation_->archive
            );
        }
        auto inventory_text = extract_string(
            *archive,
            *inventory_entry,
            policy.limits.maximum_manifest_bytes,
            implementation_->archive,
            Phase::Validating
        );
        if (!inventory_text) return inventory_text.error();
        auto parsed = parse_inventory(
            inventory_text.value(),
            policy.limits,
            implementation_->archive
        );
        if (!parsed) return parsed.error();
        auto& inventory = parsed.value();

        std::vector<std::pair<std::string, std::string>> actual;
        for (const auto& entry : entries) {
            if (entry.directory || entry.path == inventory_path) continue;
            const auto expected = inventory.find(entry.path);
            if (expected == inventory.end()) {
                add_issue(
                    report,
                    ErrorCode::ChecksumMismatch,
                    "regular file is missing from files.sha256",
                    entry.path
                );
                continue;
            }
            std::string digest = expected->second;
            if (policy.verify_checksums) {
                auto hashed = hash_entry(
                    *archive,
                    entry,
                    policy.limits.maximum_file_bytes,
                    implementation_->archive
                );
                if (!hashed) return hashed.error();
                digest = std::move(hashed.value());
                if (digest != expected->second) {
                    add_issue(
                        report,
                        ErrorCode::ChecksumMismatch,
                        "file SHA-256 does not match files.sha256",
                        entry.path
                    );
                }
            }
            actual.emplace_back(entry.path, std::move(digest));
            inventory.erase(expected);
        }
        if (policy.require_complete_inventory) {
            for (const auto& [path, unused] : inventory) {
                (void)unused;
                add_issue(
                    report,
                    ErrorCode::ChecksumMismatch,
                    "files.sha256 references a missing regular file",
                    path
                );
            }
        }
        std::sort(actual.begin(), actual.end());
        internal::Sha256 identity;
        for (const auto& [path, digest] : actual) {
            identity.update(path);
            identity.update(std::string_view("\0", 1));
            identity.update(digest);
            identity.update("\n");
        }
        report.content_digest = identity.finish_hex();
        return report;
    } catch (const std::bad_alloc&) {
        return make_error(
            ErrorCode::ResourceLimit,
            Phase::Validating,
            "memory allocation failed during SQ validation",
            implementation_->archive
        );
    } catch (const std::exception& exception) {
        return make_error(
            ErrorCode::InternalError,
            Phase::Validating,
            exception.what(),
            implementation_->archive
        );
    } catch (...) {
        return make_error(
            ErrorCode::InternalError,
            Phase::Validating,
            "unexpected SQ validation failure",
            implementation_->archive
        );
    }
}

Result<ExtractionReport> Package::extract_transactionally(
    const std::filesystem::path& destination,
    const ExtractionPolicy& policy
) const noexcept
{
    std::filesystem::path staging;
    StagingCleanup cleanup(staging, policy.preserve_failed_staging);
    try {
        if (policy.replace_existing) {
            return make_error(
                ErrorCode::UnsupportedArchive,
                Phase::Extracting,
                "destination replacement is not supported in SQ version 0",
                destination
            );
        }
        std::error_code error;
        if (std::filesystem::exists(destination, error)) {
            return make_error(
                ErrorCode::TransactionFailed,
                Phase::Extracting,
                "extraction destination already exists",
                destination
            );
        }
        auto validation = validate(policy.validation);
        if (!validation) return validation.error();
        if (!validation.value().valid()) {
            return make_error(
                ErrorCode::ChecksumMismatch,
                Phase::Extracting,
                "SQ package did not pass validation",
                implementation_->archive
            );
        }

        auto opened = open_archive(
            implementation_->archive,
            Phase::Extracting
        );
        if (!opened) return opened.error();
        ArchiveHandle archive = std::move(opened.value());
        auto listed = read_entries(
            *archive,
            implementation_->archive,
            Phase::Extracting
        );
        if (!listed) return listed.error();

        staging = staging_path(destination);
        std::filesystem::create_directories(staging);
        ExtractionReport report;
        report.destination = destination;
        report.content_digest = validation.value().content_digest;

        for (const auto& entry : listed.value()) {
            constexpr std::string_view prefix = "content/";
            if (!entry.path.starts_with(prefix) || entry.directory) continue;
            const std::string relative = entry.path.substr(prefix.size());
            if (relative.empty()) continue;
            const std::filesystem::path output_path = staging / relative;
            std::filesystem::create_directories(output_path.parent_path());
            FileSink sink;
            sink.maximum = policy.validation.limits.maximum_file_bytes;
            sink.output.open(output_path, std::ios::binary | std::ios::trunc);
            if (!sink.output) {
                return make_error(
                    ErrorCode::ExtractionFailed,
                    Phase::Extracting,
                    "cannot create extracted file",
                    output_path,
                    entry.path
                );
            }
            if (!mz_zip_reader_extract_to_callback(
                    archive.get(),
                    entry.index,
                    write_file,
                    &sink,
                    0
                )) {
                return make_error(
                    sink.failed ? ErrorCode::ExtractionFailed
                                : ErrorCode::InvalidArchive,
                    Phase::Extracting,
                    "cannot stream extracted file",
                    output_path,
                    entry.path
                );
            }
            sink.output.close();
            if (!sink.output) {
                return make_error(
                    ErrorCode::ExtractionFailed,
                    Phase::Extracting,
                    "cannot close extracted file",
                    output_path,
                    entry.path
                );
            }
            ++report.files_written;
            report.bytes_written += sink.written;
        }
        std::filesystem::rename(staging, destination);
        cleanup.commit();
        return report;
    } catch (const std::exception& exception) {
        return make_error(
            ErrorCode::TransactionFailed,
            Phase::Committing,
            exception.what(),
            destination
        );
    } catch (...) {
        return make_error(
            ErrorCode::InternalError,
            Phase::Extracting,
            "unexpected SQ extraction failure",
            destination
        );
    }
}

}  // namespace squared::sq
