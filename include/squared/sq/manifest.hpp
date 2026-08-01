#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace squared::sq {

/**
 * @brief Supported semantic kinds carried by an SQ container.
 */
enum class PackageKind {
    Template,  ///< Project-generation template.
    Module,    ///< Optional project API or implementation contribution.
    Kit,       ///< Platform or build dependency kit.
    Cartridge ///< Application-owned cartridge payload.
};

/**
 * @brief One exact package dependency declared by another SQ package.
 */
struct PackageRequirement {
    /// Required semantic package kind.
    PackageKind kind{PackageKind::Module};
    /// Permanent package identifier.
    std::string id;
    /// Exact package version.
    std::string version;
};

/**
 * @brief Build-system contribution declared by a module package.
 *
 * A module is extracted beneath the project root. Its directory must live
 * below modules/, contain a CMakeLists.txt, and publish the named CMake target.
 */
struct ModuleContribution {
    /// Portable project-relative directory, for example modules/squared-time.
    std::string directory;
    /// Public CMake target exported by the module.
    std::string cmake_target;
    /// Exact module dependencies that must be composed first.
    std::vector<PackageRequirement> requirements;
};

/**
 * @brief One user-facing field accepted by a project template.
 */
struct TemplateField {
    /// Stable lowercase field name used by the builder.
    std::string name;
    /// Uppercase placeholder rendered into template paths and files.
    std::string variable;
    /// Whether project creation requires a resolved value.
    bool required{true};
    /// Literal default value, or an empty string when none is declared.
    std::string default_value;
};

/**
 * @brief Declarative project-generation contract carried by a template.
 *
 * Version 0 templates contain only data and text placeholders. Executable
 * hooks are explicitly rejected.
 */
struct TemplateContribution {
    /// Stable builder profile selected by this template.
    std::string profile;
    /// Portable package-relative directory containing the template tree.
    std::string directory;
    /// Form fields displayed or accepted by the project builder.
    std::vector<TemplateField> fields;
    /// Exact independently registered package dependencies.
    std::vector<PackageRequirement> requirements;
    /// Reserved flag that must remain false in format version 0.
    bool executable_hooks{false};
};

/**
 * @brief Parsed mandatory fields from .squared/manifest.json.
 *
 * The original strict JSON is retained so a kind-specific validator can
 * inspect fields unknown to the base SQ envelope.
 */
struct Manifest {
    /// SQ envelope version; development packages use zero.
    std::uint32_t format_version{0};
    /// Semantic package kind.
    PackageKind kind{PackageKind::Template};
    /// Permanent reverse-domain package identifier.
    std::string id;
    /// Semantic package version.
    std::string version;
    /// Human-readable package name.
    std::string name;
    /// Optional human-readable package summary.
    std::string description;
    /// Strict original JSON retained for kind-specific validation.
    std::string original_json;
    /// Required build contribution for packages whose kind is module.
    std::optional<ModuleContribution> module;
    /// Required generation contract for packages whose kind is template.
    std::optional<TemplateContribution> project_template;
};

/**
 * @brief Return the stable lowercase package-kind identifier.
 */
[[nodiscard]] const char* to_string(PackageKind kind) noexcept;

}  // namespace squared::sq
