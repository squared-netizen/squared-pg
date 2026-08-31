// SPDX-License-Identifier: MIT
//
// Error code metadata. Pure functions over enums; no dependency on the
// container backends. Exists partly so that a fresh checkout builds and
// tests green before any container code is written, which keeps
// tools/sqcart-isolation.fish meaningful from day one.

#include "sqcart/sqcart.hpp"

namespace sqcart {

EngineCategory engine_category(ErrorCode code) noexcept {
    switch (code) {
        // Container and content problems: the resource is unusable as given.
        case ErrorCode::container_invalid:
        case ErrorCode::path_unsafe:
        case ErrorCode::limit_exceeded:
        case ErrorCode::manifest_missing:
        case ErrorCode::integrity_failed:
            return EngineCategory::resource;

        // Well-formed container, contents violate the specification.
        case ErrorCode::manifest_malformed:
        case ErrorCode::kind_invalid:
        case ErrorCode::payload_missing:
        case ErrorCode::native_code_prohibited:
            return EngineCategory::validation;

        // The cartridge may be valid; this reader cannot handle it.
        case ErrorCode::format_unsupported:
        case ErrorCode::feature_unsupported:
            return EngineCategory::compatibility;

        case ErrorCode::io_failed:
            return EngineCategory::filesystem;

        case ErrorCode::internal:
            return EngineCategory::internal;
    }
    return EngineCategory::internal;
}

std::string_view to_string(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::container_invalid:      return "cartridge.container_invalid";
        case ErrorCode::path_unsafe:            return "cartridge.path_unsafe";
        case ErrorCode::limit_exceeded:         return "cartridge.limit_exceeded";
        case ErrorCode::manifest_missing:       return "cartridge.manifest_missing";
        case ErrorCode::manifest_malformed:     return "cartridge.manifest_malformed";
        case ErrorCode::format_unsupported:     return "cartridge.format_unsupported";
        case ErrorCode::feature_unsupported:    return "cartridge.feature_unsupported";
        case ErrorCode::kind_invalid:           return "cartridge.kind_invalid";
        case ErrorCode::payload_missing:        return "cartridge.payload_missing";
        case ErrorCode::native_code_prohibited: return "cartridge.native_code_prohibited";
        case ErrorCode::integrity_failed:       return "cartridge.integrity_failed";
        case ErrorCode::io_failed:              return "cartridge.io_failed";
        case ErrorCode::internal:               return "cartridge.internal";
    }
    return "cartridge.internal";
}

std::string_view to_string(Kind kind) noexcept {
    switch (kind) {
        case Kind::cartridge:        return "cartridge";
        case Kind::project_template: return "template";
        case Kind::kit:              return "kit";
        case Kind::package:          return "package";
        case Kind::asset_bundle:     return "asset-bundle";
        case Kind::plugin:           return "plugin";
    }
    return "";
}

std::optional<Kind> kind_from_string(std::string_view s) noexcept {
    if (s == "cartridge")    { return Kind::cartridge; }
    if (s == "template")     { return Kind::project_template; }
    if (s == "kit")          { return Kind::kit; }
    if (s == "package")      { return Kind::package; }
    if (s == "asset-bundle") { return Kind::asset_bundle; }
    if (s == "plugin")       { return Kind::plugin; }
    return std::nullopt;
}

}  // namespace sqcart
