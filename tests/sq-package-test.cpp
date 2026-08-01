#include <squared/sq/package.hpp>
#include <squared/sq/writer.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>

namespace {

void require(bool condition, const std::string& message)
{
    if (condition) return;
    std::cerr << "SQ package test failed: " << message << '\n';
    std::exit(1);
}

void write_file(
    const std::filesystem::path& path,
    const std::string& contents
)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(output), "create fixture " + path.string());
    output << contents;
    require(static_cast<bool>(output), "write fixture " + path.string());
}

std::string read_file(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "open extracted fixture");
    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()
    );
}

}  // namespace

int main()
{
    const auto unique =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path root =
        std::filesystem::current_path() /
        ("squared-sq-test-" + std::to_string(unique));
    const std::filesystem::path content = root / "content";
    const std::filesystem::path first_archive = root / "first.sq";
    const std::filesystem::path second_archive = root / "second.sq";
    const std::filesystem::path invalid_archive = root / "invalid.sq";
    const std::filesystem::path extracted = root / "extracted";
    const std::filesystem::path template_content =
        root / "template-content";
    const std::filesystem::path template_archive =
        root / "android-template.sq";
    const std::filesystem::path legacy_template_archive =
        root / "legacy-android-template.sq";

    std::filesystem::create_directories(content);
    write_file(
        content /
            "modules/squared-time/include/squared/time/timepiece.hpp",
        "#pragma once\n"
    );
    write_file(
        content / "modules/squared-time/src/timepiece.cpp",
        "int squared_time_fixture = 1;\n"
    );
    write_file(
        content / "modules/squared-time/CMakeLists.txt",
        "add_library(squared_time STATIC src/timepiece.cpp)\n"
    );

    squared::sq::Manifest manifest;
    manifest.format_version = 0;
    manifest.kind = squared::sq::PackageKind::Module;
    manifest.id = "dev.squarednetizen.squared.time";
    manifest.version = "0.6.0-dev.1";
    manifest.name = "Squared Time";
    manifest.description = "Portable deterministic time domains";
    manifest.module = squared::sq::ModuleContribution{
        "modules/squared-time",
        "squared_time",
        {}
    };
    manifest.module->requirements = {
        {
            squared::sq::PackageKind::Module,
            "dev.squarednetizen.squared.base",
            "1.0.0"
        }
    };

    auto first = squared::sq::PackageWriter::create_from_directory(
        manifest,
        content,
        first_archive
    );
    require(
        static_cast<bool>(first),
        first ? "" : first.error().message
    );
    require(first.value().file_count == 3, "writer counts content files");

    auto second = squared::sq::PackageWriter::create_from_directory(
        manifest,
        content,
        second_archive
    );
    require(
        static_cast<bool>(second),
        second ? "" : second.error().message
    );
    require(
        first.value().content_digest == second.value().content_digest,
        "content identity is deterministic"
    );
    require(
        first.value().archive_sha256 == second.value().archive_sha256,
        "archive bytes are deterministic"
    );

    squared::sq::Manifest invalid_manifest = manifest;
    invalid_manifest.original_json =
        "{"
        "\"format\":\"dev.squarednetizen.sq\","
        "\"format_version\":0,"
        "\"kind\":\"module\","
        "\"id\":\"dev.squarednetizen.squared.time\","
        "\"version\":\"../../escape\","
        "\"name\":\"Invalid\","
        "\"module\":{"
        "\"directory\":\"modules/squared-time\","
        "\"cmake_target\":\"squared_time\""
        "}"
        "}";
    auto invalid = squared::sq::PackageWriter::create_from_directory(
        invalid_manifest,
        content,
        invalid_archive
    );
    require(!invalid, "writer rejects an unsafe manifest version");
    require(
        !std::filesystem::exists(invalid_archive),
        "failed writer transaction leaves no destination"
    );

    auto package = squared::sq::Package::open(first_archive);
    require(
        static_cast<bool>(package),
        package ? "" : package.error().message
    );
    require(
        package.value().manifest().kind ==
            squared::sq::PackageKind::Module,
        "manifest kind"
    );
    require(
        package.value().manifest().id ==
            "dev.squarednetizen.squared.time",
        "manifest id"
    );
    require(
        package.value().manifest().module->requirements.size() == 1,
        "module requirement parsed"
    );
    require(
        package.value().manifest().module->requirements[0].id ==
            "dev.squarednetizen.squared.base",
        "module requirement identity"
    );

    squared::sq::ValidationPolicy expected_module;
    expected_module.expected_kind = squared::sq::PackageKind::Module;
    auto validation = package.value().validate(expected_module);
    require(
        static_cast<bool>(validation),
        validation ? "" : validation.error().message
    );
    require(validation.value().valid(), "package validation");
    require(
        validation.value().content_digest == first.value().content_digest,
        "reader and writer content identities agree"
    );

    squared::sq::ValidationPolicy wrong_kind;
    wrong_kind.expected_kind = squared::sq::PackageKind::Template;
    auto wrong_validation = package.value().validate(wrong_kind);
    require(static_cast<bool>(wrong_validation), "kind report returned");
    require(!wrong_validation.value().valid(), "wrong kind is rejected");

    squared::sq::ExtractionPolicy extraction_policy;
    extraction_policy.validation.expected_kind =
        squared::sq::PackageKind::Module;
    auto extraction = package.value().extract_transactionally(
        extracted,
        extraction_policy
    );
    require(
        static_cast<bool>(extraction),
        extraction ? "" : extraction.error().message
    );
    require(extraction.value().files_written == 3, "extract file count");
    require(
        read_file(
            extracted / "modules/squared-time/src/timepiece.cpp"
        ) ==
            "int squared_time_fixture = 1;\n",
        "extracted content"
    );

    auto second_extraction = package.value().extract_transactionally(
        extracted,
        extraction_policy
    );
    require(!second_extraction, "existing destination is protected");
    require(
        second_extraction.error().code ==
            squared::sq::ErrorCode::TransactionFailed,
        "existing destination error code"
    );

    write_file(
        template_content / "template/.squared-pg.lua",
        "return {format = 3}\n"
    );
    write_file(
        template_content / "template/app/build.gradle",
        "versionName \"{{BASE_VERSION}}\"\n"
    );
    squared::sq::Manifest template_manifest;
    template_manifest.format_version = 0;
    template_manifest.kind = squared::sq::PackageKind::Template;
    template_manifest.id =
        "dev.squarednetizen.template.android-sdl2-lua";
    template_manifest.version = "0.6.0-dev.1";
    template_manifest.name = "Squared Android SDL2 Lua";
    squared::sq::TemplateContribution template_contribution;
    template_contribution.profile = "android_sdl2_lua";
    template_contribution.directory = "template";
    template_contribution.fields = {
        {"project_name", "PROJECT_NAME", true, ""},
        {"base_version", "BASE_VERSION", false, "0.1.0"}
    };
    template_contribution.requirements = {
        {
            squared::sq::PackageKind::Module,
            "dev.squarednetizen.squared.time",
            "0.6.0-dev.1"
        }
    };
    template_manifest.project_template =
        std::move(template_contribution);

    auto template_write =
        squared::sq::PackageWriter::create_from_directory(
            template_manifest,
            template_content,
            template_archive
        );
    require(
        static_cast<bool>(template_write),
        template_write ? "" : template_write.error().message
    );
    auto template_package =
        squared::sq::Package::open(template_archive);
    require(
        static_cast<bool>(template_package),
        template_package ? "" : template_package.error().message
    );
    require(
        template_package.value().manifest().project_template.has_value(),
        "template contribution parsed"
    );
    require(
        template_package.value()
                .manifest()
                .project_template
                ->requirements.size() == 1,
        "template requirement parsed"
    );
    squared::sq::ValidationPolicy expected_template;
    expected_template.expected_kind =
        squared::sq::PackageKind::Template;
    auto template_validation =
        template_package.value().validate(expected_template);
    require(
        static_cast<bool>(template_validation),
        template_validation ? "" : template_validation.error().message
    );
    require(
        template_validation.value().valid(),
        "template package validation"
    );

    std::filesystem::remove(
        template_content / "template/.squared-pg.lua"
    );
    write_file(
        template_content / "template/.sdl-pg.lua",
        "return {format = 3}\n"
    );
    auto legacy_template_write =
        squared::sq::PackageWriter::create_from_directory(
            template_manifest,
            template_content,
            legacy_template_archive
        );
    require(
        static_cast<bool>(legacy_template_write),
        legacy_template_write ? "" : legacy_template_write.error().message
    );
    auto legacy_template_package =
        squared::sq::Package::open(legacy_template_archive);
    require(
        static_cast<bool>(legacy_template_package),
        legacy_template_package ?
            "" : legacy_template_package.error().message
    );
    auto legacy_template_validation =
        legacy_template_package.value().validate(expected_template);
    require(
        static_cast<bool>(legacy_template_validation),
        legacy_template_validation ?
            "" : legacy_template_validation.error().message
    );
    require(
        legacy_template_validation.value().valid(),
        "legacy template marker validation"
    );

    std::filesystem::remove_all(root);
    std::cout << "SQ package read, validation, writing, and extraction: OK\n";
    return 0;
}
