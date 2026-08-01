#include <squared/sq/package.hpp>
#include <squared/sq/writer.hpp>

#include "sha256.hpp"

#define MINIZ_NO_ZLIB_APIS
#include <miniz.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace squared::sq {
namespace {

struct SourceFile {
    std::string archive_path;
    std::filesystem::path source_path;
    std::uint64_t size{0};
    std::string digest;
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

bool allowed(unsigned char character) noexcept
{
    return std::isalnum(character) != 0 ||
           character == '.' || character == '_' || character == '-' ||
           character == '+' || character == '@' ||
           character == '{' || character == '}';
}

bool portable_relative_path(std::string_view path) noexcept
{
    if (path.empty() || path.front() == '/' || path.size() > 240) return false;
    std::size_t start = 0;
    while (start < path.size()) {
        const std::size_t slash = path.find('/', start);
        const std::size_t end =
            slash == std::string_view::npos ? path.size() : slash;
        const std::string_view component = path.substr(start, end - start);
        if (component.empty() || component == "." || component == ".." ||
            component.size() > 100) {
            return false;
        }
        for (const unsigned char character : component) {
            if (!allowed(character)) return false;
        }
        if (slash == std::string_view::npos) break;
        start = slash + 1;
    }
    return true;
}

std::string escape_json(std::string_view value)
{
    std::ostringstream output;
    for (const unsigned char character : value) {
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (character < 0x20) {
                constexpr char digits[] = "0123456789abcdef";
                output << "\\u00"
                       << digits[(character >> 4U) & 0xfU]
                       << digits[character & 0xfU];
            } else {
                output << static_cast<char>(character);
            }
        }
    }
    return output.str();
}

std::string canonical_manifest(const Manifest& manifest)
{
    if (!manifest.original_json.empty()) return manifest.original_json;
    std::ostringstream output;
    output << "{\n"
           << "  \"format\": \"dev.squarednetizen.sq\",\n"
           << "  \"format_version\": 0,\n"
           << "  \"kind\": \"" << to_string(manifest.kind) << "\",\n"
           << "  \"id\": \"" << escape_json(manifest.id) << "\",\n"
           << "  \"version\": \"" << escape_json(manifest.version)
           << "\",\n"
           << "  \"name\": \"" << escape_json(manifest.name) << "\"";
    if (!manifest.description.empty()) {
        output << ",\n  \"description\": \""
               << escape_json(manifest.description) << "\"";
    }
    if (manifest.module) {
        output << ",\n  \"module\": {\n"
               << "    \"directory\": \""
               << escape_json(manifest.module->directory) << "\",\n"
               << "    \"cmake_target\": \""
               << escape_json(manifest.module->cmake_target) << "\",\n"
               << "    \"requires\": [";
        for (std::size_t index = 0;
             index < manifest.module->requirements.size();
             ++index) {
            const auto& requirement =
                manifest.module->requirements[index];
            output << (index == 0 ? "\n" : ",\n")
                   << "      {\"kind\":\""
                   << to_string(requirement.kind)
                   << "\",\"id\":\""
                   << escape_json(requirement.id)
                   << "\",\"version\":\""
                   << escape_json(requirement.version)
                   << "\"}";
        }
        output << (manifest.module->requirements.empty() ? "" : "\n")
               << "    ]\n"
               << "  }";
    }
    if (manifest.project_template) {
        const auto& project_template = *manifest.project_template;
        output << ",\n  \"template\": {\n"
               << "    \"profile\": \""
               << escape_json(project_template.profile) << "\",\n"
               << "    \"directory\": \""
               << escape_json(project_template.directory) << "\",\n"
               << "    \"executable_hooks\": "
               << (project_template.executable_hooks ? "true" : "false")
               << ",\n"
               << "    \"fields\": [";
        for (std::size_t index = 0;
             index < project_template.fields.size();
             ++index) {
            const auto& field = project_template.fields[index];
            output << (index == 0 ? "\n" : ",\n")
                   << "      {\"name\":\""
                   << escape_json(field.name)
                   << "\",\"variable\":\""
                   << escape_json(field.variable)
                   << "\",\"required\":"
                   << (field.required ? "true" : "false");
            if (!field.default_value.empty()) {
                output << ",\"default\":\""
                       << escape_json(field.default_value) << "\"";
            }
            output << "}";
        }
        output << (project_template.fields.empty() ? "" : "\n")
               << "    ],\n"
               << "    \"requires\": [";
        for (std::size_t index = 0;
             index < project_template.requirements.size();
             ++index) {
            const auto& requirement =
                project_template.requirements[index];
            output << (index == 0 ? "\n" : ",\n")
                   << "      {\"kind\":\""
                   << to_string(requirement.kind)
                   << "\",\"id\":\""
                   << escape_json(requirement.id)
                   << "\",\"version\":\""
                   << escape_json(requirement.version)
                   << "\"}";
        }
        output << (project_template.requirements.empty() ? "" : "\n")
               << "    ]\n"
               << "  }";
    }
    output << "\n}\n";
    return output.str();
}

struct Reader {
    std::ifstream input;
};

size_t read_file(
    void* opaque,
    mz_uint64 offset,
    void* buffer,
    size_t size
)
{
    auto& reader = *static_cast<Reader*>(opaque);
    reader.input.clear();
    reader.input.seekg(static_cast<std::streamoff>(offset));
    if (!reader.input) return 0;
    reader.input.read(static_cast<char*>(buffer), size);
    return static_cast<size_t>(reader.input.gcount());
}

bool add_memory(
    mz_zip_archive& archive,
    const std::string& path,
    const std::string& contents,
    mz_uint level,
    bool deterministic
)
{
    MZ_TIME_T fixed_time{};
#ifndef MINIZ_NO_TIME
    fixed_time = 315532800;
#endif
    return mz_zip_writer_add_mem_ex_v2(
        &archive,
        path.c_str(),
        contents.data(),
        contents.size(),
        nullptr,
        0,
        level,
        0,
        0,
        deterministic ? &fixed_time : nullptr,
        nullptr,
        0,
        nullptr,
        0
    ) == MZ_TRUE;
}

bool add_source(
    mz_zip_archive& archive,
    const SourceFile& source,
    mz_uint level,
    bool deterministic
)
{
    Reader reader;
    reader.input.open(source.source_path, std::ios::binary);
    if (!reader.input) return false;
    MZ_TIME_T fixed_time{};
#ifndef MINIZ_NO_TIME
    fixed_time = 315532800;
#endif
    return mz_zip_writer_add_read_buf_callback(
        &archive,
        source.archive_path.c_str(),
        read_file,
        &reader,
        source.size,
        deterministic ? &fixed_time : nullptr,
        nullptr,
        0,
        level,
        nullptr,
        0,
        nullptr,
        0
    ) == MZ_TRUE;
}

std::atomic<std::uint64_t> writer_counter{0};

std::filesystem::path temporary_path(
    const std::filesystem::path& destination
)
{
    const auto counter = ++writer_counter;
    const auto ticks = std::chrono::steady_clock::now()
                           .time_since_epoch()
                           .count();
    return destination.string() + ".sq-partial-" +
           std::to_string(ticks) + "-" + std::to_string(counter);
}

}  // namespace

Result<WriteReport> PackageWriter::create_from_directory(
    const Manifest& manifest,
    const std::filesystem::path& content_directory,
    const std::filesystem::path& destination,
    const WriterOptions& options
) noexcept
{
    std::filesystem::path temporary;
    mz_zip_archive archive{};
    bool writer_open = false;
    try {
        std::error_code filesystem_error;
        if (!std::filesystem::is_directory(
                content_directory,
                filesystem_error
            )) {
            return make_error(
                ErrorCode::FileNotFound,
                Phase::Writing,
                "SQ content source is not a directory",
                content_directory
            );
        }
        if (std::filesystem::exists(destination, filesystem_error)) {
            return make_error(
                ErrorCode::TransactionFailed,
                Phase::Writing,
                "SQ destination already exists",
                destination
            );
        }
        if (options.compression_level > 9) {
            return make_error(
                ErrorCode::InternalError,
                Phase::Writing,
                "compression level must be in the inclusive range 0 to 9",
                destination
            );
        }
        if (!options.deterministic) {
            return make_error(
                ErrorCode::UnsupportedArchive,
                Phase::Writing,
                "SQ version 0 always requires deterministic output",
                destination
            );
        }

        std::vector<SourceFile> sources;
        std::uint64_t expanded_bytes = 0;
        for (std::filesystem::recursive_directory_iterator iterator(
                 content_directory,
                 std::filesystem::directory_options::none
             ), end;
             iterator != end;
             ++iterator) {
            const auto status = iterator->symlink_status();
            if (std::filesystem::is_directory(status)) continue;
            if (!std::filesystem::is_regular_file(status)) {
                return make_error(
                    ErrorCode::UnsupportedArchive,
                    Phase::Writing,
                    "SQ source contains a symbolic link or special entry",
                    iterator->path()
                );
            }
            const std::filesystem::path relative =
                std::filesystem::relative(iterator->path(), content_directory);
            const std::string relative_name = relative.generic_string();
            if (!portable_relative_path(relative_name)) {
                return make_error(
                    ErrorCode::UnsafePath,
                    Phase::Writing,
                    "SQ source path is not portable",
                    iterator->path(),
                    relative_name
                );
            }
            SourceFile source;
            source.archive_path = "content/" + relative_name;
            source.source_path = iterator->path();
            source.size = iterator->file_size();
            if (source.size >
                std::numeric_limits<std::uint64_t>::max() -
                    expanded_bytes) {
                return make_error(
                    ErrorCode::ResourceLimit,
                    Phase::Writing,
                    "combined source size exceeds the writer limit",
                    iterator->path(),
                    source.archive_path
                );
            }
            std::string hash_message;
            if (!internal::sha256_file(
                    source.source_path,
                    source.digest,
                    hash_message
                )) {
                return make_error(
                    ErrorCode::ExtractionFailed,
                    Phase::Writing,
                    hash_message,
                    source.source_path,
                    source.archive_path
                );
            }
            expanded_bytes += source.size;
            sources.push_back(std::move(source));
        }
        std::sort(
            sources.begin(),
            sources.end(),
            [](const SourceFile& left, const SourceFile& right) {
                return left.archive_path < right.archive_path;
            }
        );

        const std::string manifest_json = canonical_manifest(manifest);
        std::vector<std::pair<std::string, std::string>> inventory_entries;
        inventory_entries.emplace_back(
            ".squared/manifest.json",
            internal::sha256_hex(manifest_json)
        );
        for (const auto& source : sources) {
            inventory_entries.emplace_back(
                source.archive_path,
                source.digest
            );
        }
        std::sort(inventory_entries.begin(), inventory_entries.end());

        std::ostringstream inventory_stream;
        internal::Sha256 identity;
        for (const auto& [path, digest] : inventory_entries) {
            inventory_stream << digest << "  " << path << '\n';
            identity.update(path);
            identity.update(std::string_view("\0", 1));
            identity.update(digest);
            identity.update("\n");
        }
        const std::string inventory = inventory_stream.str();
        const std::string content_digest = identity.finish_hex();

        const std::filesystem::path destination_parent =
            destination.parent_path();
        if (!destination_parent.empty()) {
            std::filesystem::create_directories(destination_parent);
        }
        temporary = temporary_path(destination);
        const std::string temporary_native = temporary.string();
        if (!mz_zip_writer_init_file_v2(
                &archive,
                temporary_native.c_str(),
                0,
                0
            )) {
            return make_error(
                ErrorCode::TransactionFailed,
                Phase::Writing,
                mz_zip_get_error_string(mz_zip_get_last_error(&archive)),
                temporary
            );
        }
        writer_open = true;
        const mz_uint level = options.compress
            ? options.compression_level
            : 0;

        struct Pending {
            std::string path;
            const SourceFile* source{nullptr};
            const std::string* memory{nullptr};
        };
        std::vector<Pending> pending;
        pending.push_back({
            ".squared/files.sha256",
            nullptr,
            &inventory
        });
        pending.push_back({
            ".squared/manifest.json",
            nullptr,
            &manifest_json
        });
        for (const auto& source : sources) {
            pending.push_back({source.archive_path, &source, nullptr});
        }
        std::sort(
            pending.begin(),
            pending.end(),
            [](const Pending& left, const Pending& right) {
                return left.path < right.path;
            }
        );
        for (const auto& item : pending) {
            const bool added = item.source
                ? add_source(
                      archive,
                      *item.source,
                      level,
                      options.deterministic
                  )
                : add_memory(
                      archive,
                      item.path,
                      *item.memory,
                      level,
                      options.deterministic
                  );
            if (!added) {
                const std::string message =
                    mz_zip_get_error_string(mz_zip_get_last_error(&archive));
                mz_zip_writer_end(&archive);
                writer_open = false;
                std::filesystem::remove(temporary, filesystem_error);
                return make_error(
                    ErrorCode::TransactionFailed,
                    Phase::Writing,
                    message,
                    temporary,
                    item.path
                );
            }
        }
        if (!mz_zip_writer_finalize_archive(&archive)) {
            const std::string message =
                mz_zip_get_error_string(mz_zip_get_last_error(&archive));
            mz_zip_writer_end(&archive);
            writer_open = false;
            std::filesystem::remove(temporary, filesystem_error);
            return make_error(
                ErrorCode::TransactionFailed,
                Phase::Writing,
                message,
                temporary
            );
        }
        mz_zip_writer_end(&archive);
        writer_open = false;

        auto package = Package::open(temporary);
        if (!package) {
            std::filesystem::remove(temporary, filesystem_error);
            return package.error();
        }
        auto validation = package.value().validate();
        if (!validation || !validation.value().valid()) {
            std::filesystem::remove(temporary, filesystem_error);
            return validation
                ? make_error(
                      ErrorCode::ChecksumMismatch,
                      Phase::Writing,
                      "new SQ archive failed validation",
                      temporary
                  )
                : validation.error();
        }
        if (validation.value().content_digest != content_digest) {
            std::filesystem::remove(temporary, filesystem_error);
            return make_error(
                ErrorCode::ChecksumMismatch,
                Phase::Writing,
                "new SQ archive content identity changed during writing",
                temporary
            );
        }
        std::string archive_sha256;
        std::string hash_message;
        if (!internal::sha256_file(
                temporary,
                archive_sha256,
                hash_message
            )) {
            std::filesystem::remove(temporary, filesystem_error);
            return make_error(
                ErrorCode::TransactionFailed,
                Phase::Writing,
                hash_message,
                temporary
            );
        }
        std::filesystem::rename(temporary, destination);
        temporary.clear();

        WriteReport report;
        report.archive = destination;
        report.file_count = sources.size();
        report.expanded_bytes = expanded_bytes;
        report.content_digest = content_digest;
        report.archive_sha256 = std::move(archive_sha256);
        return report;
    } catch (const std::exception& exception) {
        if (writer_open) mz_zip_writer_end(&archive);
        if (!temporary.empty()) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
        }
        return make_error(
            ErrorCode::TransactionFailed,
            Phase::Writing,
            exception.what(),
            destination
        );
    } catch (...) {
        if (writer_open) mz_zip_writer_end(&archive);
        if (!temporary.empty()) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
        }
        return make_error(
            ErrorCode::InternalError,
            Phase::Writing,
            "unexpected SQ package-writing failure",
            destination
        );
    }
}

}  // namespace squared::sq
