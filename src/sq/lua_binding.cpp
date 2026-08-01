#include <squared/sq/lua.hpp>
#include <squared/sq/package.hpp>
#include <squared/sq/writer.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <filesystem>
#include <new>
#include <optional>
#include <string>
#include <utility>

namespace {

constexpr const char* package_metatable = "squared.sq.package";

struct LuaPackage {
    squared::sq::Package* value{nullptr};
};

void set_string(lua_State* state, const char* key, const std::string& value)
{
    lua_pushlstring(state, value.data(), value.size());
    lua_setfield(state, -2, key);
}

void push_error(lua_State* state, const squared::sq::Error& error)
{
    lua_createtable(state, 0, 6);
    lua_pushstring(state, squared::sq::to_string(error.code));
    lua_setfield(state, -2, "code");
    lua_pushstring(state, squared::sq::to_string(error.phase));
    lua_setfield(state, -2, "phase");
    set_string(state, "message", error.message);
    set_string(state, "archive_path", error.archive_path);
    set_string(
        state,
        "filesystem_path",
        error.filesystem_path.generic_string()
    );
    lua_pushboolean(
        state,
        error.code == squared::sq::ErrorCode::Cancelled
    );
    lua_setfield(state, -2, "recoverable");
}

int return_error(lua_State* state, const squared::sq::Error& error)
{
    lua_pushnil(state);
    push_error(state, error);
    return 2;
}

LuaPackage* check_package(lua_State* state, int index)
{
    auto* package = static_cast<LuaPackage*>(
        luaL_checkudata(state, index, package_metatable)
    );
    if (!package->value) {
        luaL_error(state, "attempt to use a closed squared.sq package");
    }
    return package;
}

std::optional<squared::sq::PackageKind> parse_kind(
    lua_State* state,
    int index,
    bool optional
)
{
    if (optional && lua_isnoneornil(state, index)) return std::nullopt;
    const char* value = luaL_checkstring(state, index);
    if (std::string_view(value) == "template") {
        return squared::sq::PackageKind::Template;
    }
    if (std::string_view(value) == "module") {
        return squared::sq::PackageKind::Module;
    }
    if (std::string_view(value) == "kit") {
        return squared::sq::PackageKind::Kit;
    }
    if (std::string_view(value) == "cartridge") {
        return squared::sq::PackageKind::Cartridge;
    }
    luaL_error(state, "unsupported SQ package kind: %s", value);
    return std::nullopt;
}

squared::sq::ValidationPolicy validation_options(
    lua_State* state,
    int index
)
{
    squared::sq::ValidationPolicy policy;
    if (lua_isnoneornil(state, index)) return policy;
    luaL_checktype(state, index, LUA_TTABLE);
    lua_getfield(state, index, "expected_kind");
    policy.expected_kind = parse_kind(state, -1, true);
    lua_pop(state, 1);
    return policy;
}

void push_manifest(lua_State* state, const squared::sq::Manifest& manifest)
{
    lua_createtable(state, 0, 10);
    lua_pushstring(state, "dev.squarednetizen.sq");
    lua_setfield(state, -2, "format");
    lua_pushinteger(state, manifest.format_version);
    lua_setfield(state, -2, "format_version");
    lua_pushstring(state, squared::sq::to_string(manifest.kind));
    lua_setfield(state, -2, "kind");
    set_string(state, "id", manifest.id);
    set_string(state, "version", manifest.version);
    set_string(state, "name", manifest.name);
    set_string(state, "description", manifest.description);
    set_string(state, "original_json", manifest.original_json);
    if (manifest.module) {
        lua_createtable(state, 0, 3);
        set_string(state, "directory", manifest.module->directory);
        set_string(state, "cmake_target", manifest.module->cmake_target);
        lua_createtable(
            state,
            static_cast<int>(manifest.module->requirements.size()),
            0
        );
        int requirement_index = 1;
        for (const auto& requirement : manifest.module->requirements) {
            lua_createtable(state, 0, 3);
            lua_pushstring(
                state,
                squared::sq::to_string(requirement.kind)
            );
            lua_setfield(state, -2, "kind");
            set_string(state, "id", requirement.id);
            set_string(state, "version", requirement.version);
            lua_rawseti(state, -2, requirement_index++);
        }
        lua_setfield(state, -2, "requires");
        lua_setfield(state, -2, "module");
    }
    if (manifest.project_template) {
        const auto& project_template = *manifest.project_template;
        lua_createtable(state, 0, 5);
        set_string(state, "profile", project_template.profile);
        set_string(state, "directory", project_template.directory);
        lua_pushboolean(state, project_template.executable_hooks);
        lua_setfield(state, -2, "executable_hooks");

        lua_createtable(
            state,
            static_cast<int>(project_template.fields.size()),
            0
        );
        int field_index = 1;
        for (const auto& field : project_template.fields) {
            lua_createtable(state, 0, 4);
            set_string(state, "name", field.name);
            set_string(state, "variable", field.variable);
            lua_pushboolean(state, field.required);
            lua_setfield(state, -2, "required");
            set_string(state, "default", field.default_value);
            lua_rawseti(state, -2, field_index++);
        }
        lua_setfield(state, -2, "fields");

        lua_createtable(
            state,
            static_cast<int>(project_template.requirements.size()),
            0
        );
        int requirement_index = 1;
        for (const auto& requirement : project_template.requirements) {
            lua_createtable(state, 0, 3);
            lua_pushstring(
                state,
                squared::sq::to_string(requirement.kind)
            );
            lua_setfield(state, -2, "kind");
            set_string(state, "id", requirement.id);
            set_string(state, "version", requirement.version);
            lua_rawseti(state, -2, requirement_index++);
        }
        lua_setfield(state, -2, "requires");
        lua_setfield(state, -2, "template");
    }
}

int package_close(lua_State* state)
{
    auto* package = static_cast<LuaPackage*>(
        luaL_checkudata(state, 1, package_metatable)
    );
    delete package->value;
    package->value = nullptr;
    return 0;
}

int package_manifest(lua_State* state)
{
    const auto* package = check_package(state, 1);
    push_manifest(state, package->value->manifest());
    return 1;
}

int package_validate(lua_State* state)
{
    auto* package = check_package(state, 1);
    const auto policy = validation_options(state, 2);
    auto result = package->value->validate(policy);
    if (!result) return return_error(state, result.error());

    const auto& report = result.value();
    lua_createtable(state, 0, 8);
    lua_pushboolean(state, report.valid());
    lua_setfield(state, -2, "valid");
    push_manifest(state, report.manifest);
    lua_setfield(state, -2, "manifest");
    lua_pushinteger(state, static_cast<lua_Integer>(report.entry_count));
    lua_setfield(state, -2, "entry_count");
    lua_pushinteger(state, static_cast<lua_Integer>(report.compressed_bytes));
    lua_setfield(state, -2, "compressed_bytes");
    lua_pushinteger(state, static_cast<lua_Integer>(report.expanded_bytes));
    lua_setfield(state, -2, "expanded_bytes");
    set_string(state, "content_digest", report.content_digest);

    lua_createtable(
        state,
        static_cast<int>(report.issues.size()),
        0
    );
    int issue_index = 1;
    for (const auto& issue : report.issues) {
        lua_createtable(state, 0, 4);
        lua_pushstring(
            state,
            issue.severity == squared::sq::IssueSeverity::Error
                ? "error"
                : "warning"
        );
        lua_setfield(state, -2, "severity");
        lua_pushstring(state, squared::sq::to_string(issue.code));
        lua_setfield(state, -2, "code");
        set_string(state, "message", issue.message);
        set_string(state, "archive_path", issue.archive_path);
        lua_rawseti(state, -2, issue_index++);
    }
    lua_setfield(state, -2, "issues");
    lua_pushnil(state);
    return 2;
}

int package_extract(lua_State* state)
{
    auto* package = check_package(state, 1);
    const char* destination = luaL_checkstring(state, 2);
    squared::sq::ExtractionPolicy policy;
    policy.validation = validation_options(state, 3);
    auto result = package->value->extract_transactionally(
        std::filesystem::path(destination),
        policy
    );
    if (!result) return return_error(state, result.error());

    const auto& report = result.value();
    lua_createtable(state, 0, 4);
    set_string(
        state,
        "destination",
        report.destination.generic_string()
    );
    lua_pushinteger(state, static_cast<lua_Integer>(report.files_written));
    lua_setfield(state, -2, "files_written");
    lua_pushinteger(state, static_cast<lua_Integer>(report.bytes_written));
    lua_setfield(state, -2, "bytes_written");
    set_string(state, "content_digest", report.content_digest);
    lua_pushnil(state);
    return 2;
}

int module_open(lua_State* state)
{
    const char* path = luaL_checkstring(state, 1);
    auto result = squared::sq::Package::open(std::filesystem::path(path));
    if (!result) return return_error(state, result.error());

    auto* userdata = static_cast<LuaPackage*>(
        lua_newuserdatauv(state, sizeof(LuaPackage), 0)
    );
    userdata->value = nullptr;
    try {
        userdata->value =
            new squared::sq::Package(std::move(result.value()));
    } catch (const std::bad_alloc&) {
        return luaL_error(state, "cannot allocate SQ package userdata");
    }
    luaL_setmetatable(state, package_metatable);
    lua_pushnil(state);
    return 2;
}

std::string required_field(
    lua_State* state,
    int table,
    const char* name
)
{
    lua_getfield(state, table, name);
    size_t length = 0;
    const char* value = luaL_checklstring(state, -1, &length);
    std::string result(value, length);
    lua_pop(state, 1);
    return result;
}

int module_create(lua_State* state)
{
    luaL_checktype(state, 1, LUA_TTABLE);
    lua_getfield(state, 1, "manifest");
    squared::sq::Manifest manifest;
    if (!lua_isnil(state, -1)) {
        luaL_checktype(state, -1, LUA_TTABLE);
        const int manifest_index = lua_gettop(state);
        manifest.format_version = 0;
        lua_getfield(state, manifest_index, "kind");
        manifest.kind = *parse_kind(state, -1, false);
        lua_pop(state, 1);
        manifest.id = required_field(state, manifest_index, "id");
        manifest.version = required_field(state, manifest_index, "version");
        manifest.name = required_field(state, manifest_index, "name");
        lua_getfield(state, manifest_index, "description");
        if (!lua_isnil(state, -1)) {
            size_t length = 0;
            const char* value = luaL_checklstring(state, -1, &length);
            manifest.description.assign(value, length);
        }
        lua_pop(state, 1);
        lua_getfield(state, manifest_index, "module");
        if (!lua_isnil(state, -1)) {
            luaL_checktype(state, -1, LUA_TTABLE);
            const int module_index = lua_gettop(state);
            squared::sq::ModuleContribution contribution;
            contribution.directory =
                required_field(state, module_index, "directory");
            contribution.cmake_target =
                required_field(state, module_index, "cmake_target");
            lua_getfield(state, module_index, "requires");
            if (!lua_isnil(state, -1)) {
                luaL_checktype(state, -1, LUA_TTABLE);
                const lua_Integer requirement_count =
                    luaL_len(state, -1);
                for (lua_Integer index = 1;
                     index <= requirement_count;
                     ++index) {
                    lua_geti(state, -1, index);
                    luaL_checktype(state, -1, LUA_TTABLE);
                    const int requirement_index = lua_gettop(state);
                    squared::sq::PackageRequirement requirement;
                    lua_getfield(state, requirement_index, "kind");
                    requirement.kind =
                        *parse_kind(state, -1, false);
                    lua_pop(state, 1);
                    requirement.id =
                        required_field(state, requirement_index, "id");
                    requirement.version =
                        required_field(
                            state,
                            requirement_index,
                            "version"
                        );
                    contribution.requirements.push_back(
                        std::move(requirement)
                    );
                    lua_pop(state, 1);
                }
            }
            lua_pop(state, 1);
            manifest.module = std::move(contribution);
        }
        lua_pop(state, 1);

        lua_getfield(state, manifest_index, "template");
        if (!lua_isnil(state, -1)) {
            luaL_checktype(state, -1, LUA_TTABLE);
            const int template_index = lua_gettop(state);
            squared::sq::TemplateContribution contribution;
            contribution.profile =
                required_field(state, template_index, "profile");
            contribution.directory =
                required_field(state, template_index, "directory");
            lua_getfield(state, template_index, "executable_hooks");
            if (!lua_isnil(state, -1)) {
                contribution.executable_hooks =
                    lua_toboolean(state, -1) != 0;
            }
            lua_pop(state, 1);

            lua_getfield(state, template_index, "fields");
            luaL_checktype(state, -1, LUA_TTABLE);
            const lua_Integer field_count = luaL_len(state, -1);
            for (lua_Integer index = 1; index <= field_count; ++index) {
                lua_geti(state, -1, index);
                luaL_checktype(state, -1, LUA_TTABLE);
                const int field_index = lua_gettop(state);
                squared::sq::TemplateField field;
                field.name = required_field(state, field_index, "name");
                field.variable =
                    required_field(state, field_index, "variable");
                lua_getfield(state, field_index, "required");
                if (!lua_isnil(state, -1)) {
                    field.required = lua_toboolean(state, -1) != 0;
                }
                lua_pop(state, 1);
                lua_getfield(state, field_index, "default");
                if (!lua_isnil(state, -1)) {
                    size_t length = 0;
                    const char* value =
                        luaL_checklstring(state, -1, &length);
                    field.default_value.assign(value, length);
                }
                lua_pop(state, 1);
                contribution.fields.push_back(std::move(field));
                lua_pop(state, 1);
            }
            lua_pop(state, 1);

            lua_getfield(state, template_index, "requires");
            luaL_checktype(state, -1, LUA_TTABLE);
            const lua_Integer requirement_count = luaL_len(state, -1);
            for (lua_Integer index = 1;
                 index <= requirement_count;
                 ++index) {
                lua_geti(state, -1, index);
                luaL_checktype(state, -1, LUA_TTABLE);
                const int requirement_index = lua_gettop(state);
                squared::sq::PackageRequirement requirement;
                lua_getfield(state, requirement_index, "kind");
                requirement.kind = *parse_kind(state, -1, false);
                lua_pop(state, 1);
                requirement.id =
                    required_field(state, requirement_index, "id");
                requirement.version =
                    required_field(state, requirement_index, "version");
                contribution.requirements.push_back(
                    std::move(requirement)
                );
                lua_pop(state, 1);
            }
            lua_pop(state, 1);
            manifest.project_template = std::move(contribution);
        }
        lua_pop(state, 1);
    }
    lua_pop(state, 1);

    lua_getfield(state, 1, "manifest_json");
    if (!lua_isnil(state, -1)) {
        size_t length = 0;
        const char* value = luaL_checklstring(state, -1, &length);
        manifest.original_json.assign(value, length);
    } else if (manifest.id.empty()) {
        return luaL_error(
            state,
            "create_from_directory requires manifest or manifest_json"
        );
    }
    lua_pop(state, 1);

    const std::string content =
        required_field(state, 1, "content_directory");
    const std::string destination =
        required_field(state, 1, "destination");

    squared::sq::WriterOptions options;
    lua_getfield(state, 1, "compression_level");
    if (!lua_isnil(state, -1)) {
        options.compression_level = static_cast<std::uint32_t>(
            luaL_checkinteger(state, -1)
        );
    }
    lua_pop(state, 1);

    auto result = squared::sq::PackageWriter::create_from_directory(
        manifest,
        content,
        destination,
        options
    );
    if (!result) return return_error(state, result.error());

    const auto& report = result.value();
    lua_createtable(state, 0, 5);
    set_string(state, "archive", report.archive.generic_string());
    lua_pushinteger(state, static_cast<lua_Integer>(report.file_count));
    lua_setfield(state, -2, "file_count");
    lua_pushinteger(state, static_cast<lua_Integer>(report.expanded_bytes));
    lua_setfield(state, -2, "expanded_bytes");
    set_string(state, "content_digest", report.content_digest);
    set_string(state, "archive_sha256", report.archive_sha256);
    lua_pushnil(state);
    return 2;
}

}  // namespace

extern "C" int luaopen_squared_sq(lua_State* state)
{
    if (sizeof(lua_Integer) < sizeof(std::int64_t)) {
        return luaL_error(
            state,
            "squared.sq requires 64-bit Lua integers"
        );
    }

    if (luaL_newmetatable(state, package_metatable)) {
        static const luaL_Reg methods[] = {
            {"manifest", package_manifest},
            {"validate", package_validate},
            {"extract_transactionally", package_extract},
            {"close", package_close},
            {nullptr, nullptr}
        };
        lua_newtable(state);
        luaL_setfuncs(state, methods, 0);
        lua_setfield(state, -2, "__index");
        lua_pushcfunction(state, package_close);
        lua_setfield(state, -2, "__gc");
        lua_pushcfunction(state, package_close);
        lua_setfield(state, -2, "__close");
        lua_pushliteral(state, "squared.sq package");
        lua_setfield(state, -2, "__name");
    }
    lua_pop(state, 1);

    static const luaL_Reg functions[] = {
        {"open", module_open},
        {"create_from_directory", module_create},
        {nullptr, nullptr}
    };
    luaL_newlib(state, functions);
    lua_pushinteger(state, 0);
    lua_setfield(state, -2, "format_version");
    lua_pushliteral(state, "3.1.2");
    lua_setfield(state, -2, "miniz_version");
    return 1;
}
