--- Android SDL2 project population.
-- @module sdl_pg.android

local fs = require("sdl_pg.fs")
local dependency = require("sdl_pg.dependency")
local package_registry = require("sdl_pg.package_registry")
local path = require("sdl_pg.path")
local template = require("sdl_pg.template")
local wrapper = require("sdl_pg.wrapper")

local android = {}

--- Validate an Android application identifier.
-- @param value Candidate Java package name.
-- @return true on success.
-- @raise When the identifier is invalid.
function android.validate_package(value)
    if type(value) ~= "string" or value == "" then
        error("--package is required for an Android project", 0)
    end

    local segments = 0
    for segment in value:gmatch("[^.]+") do
        if not segment:match("^[A-Za-z_][A-Za-z0-9_]*$") then
            error("invalid Android package segment: " .. segment, 0)
        end
        segments = segments + 1
    end

    if segments < 2 or value:find("%.%.", 1, true) then
        error("Android package must contain at least two valid segments", 0)
    end

    return true
end

local function display_name(name)
    local result = name:gsub("[-_]+", " ")
    return result:gsub("^%l", string.upper)
end

local function validate_application_name(value)
    if type(value) ~= "string" or value == "" or #value > 100 or
        not value:match("^[A-Za-z0-9 ._-]+$") or
        not value:sub(1, 1):match("[A-Za-z0-9]") or
        not value:sub(-1):match("[A-Za-z0-9]") then
        error(
            "application name must use 1 to 100 letters, digits, " ..
            "spaces, dots, underscores, or hyphens",
            0
        )
    end
    return value
end

local function validate_base_version(value)
    if type(value) ~= "string" or #value == 0 or #value > 80 or
        not value:match("^[A-Za-z0-9._+-]+$") or
        not value:sub(1, 1):match("[A-Za-z0-9]") or
        not value:sub(-1):match("[A-Za-z0-9]") then
        error("base version is not a portable version identifier", 0)
    end
    return value
end

local function copy_wrapper(source, destination)
    for _, filename in ipairs({
        "gradlew",
        "gradlew.bat",
        "gradle/wrapper/gradle-wrapper.jar",
        "gradle/wrapper/gradle-wrapper.properties"
    }) do
        fs.copy_file(
            path.join(source, filename),
            path.join(destination, filename)
        )
    end
end

--- Validate Android options and resolve frontend dependencies.
-- @param settings Resolved generator configuration.
-- @param name Project name.
-- @param options Android generation options.
-- @return Prepared generator-only dependency record.
function android.resolve_dependencies(settings, name, options)
    android.validate_package(options.package_name)
    local application_name = validate_application_name(
        options.application_name or display_name(name)
    )
    local base_version = validate_base_version(
        options.base_version or "0.1.0"
    )
    return {
        frontend = dependency.require(settings, "android-sdl2"),
        application_name = application_name,
        base_version = base_version
    }
end

--- Populate a staged directory with an Android SDL2 project.
-- @param settings Resolved generator configuration.
-- @param destination Empty staged project directory.
-- @param name Project name.
-- @param options Android generation options.
-- @param template_record Resolved Android template package.
-- @param resolved_modules Resolved module dependency order.
-- @param dependencies Preflighted generator-only dependencies.
function android.populate(
    settings,
    destination,
    name,
    options,
    template_record,
    resolved_modules,
    dependencies
)
    if not settings.generator_root then
        error("generator root is unavailable", 0)
    end

    if template_record.template.profile ~= "android_sdl2_lua" then
        error(
            "template does not provide the Android SDL2 Lua profile: " ..
            template_record.id,
            0
        )
    end

    local kit_root = dependencies.frontend.root
    local wrapper_root = wrapper.require_active(settings)
    local lua_root =
        path.join(settings.generator_root, "third_party/lua-5.4.8")
    local template_root = path.join(
        template_record.content_root,
        template_record.template.directory
    )

    if fs.mode(lua_root) ~= "directory" then
        error(
            "private Lua sources are not extracted; " ..
            "run lua5.4 toolchain.lua in the generator",
            0
        )
    end

    local identifier = path.identifier(name)
    local values = {
        project_name = name,
        package_name = options.package_name,
        application_name = dependencies.application_name,
        base_version = dependencies.base_version
    }
    local manage_all_files = options.manage_all_files == true
    local variables = {
        PROJECT_ID = identifier,
        MANAGE_ALL_FILES_ENABLED = manage_all_files and "true" or "false",
        MANAGE_ALL_FILES_PERMISSION = manage_all_files and [[
    <uses-permission
        android:name="android.permission.MANAGE_EXTERNAL_STORAGE" />]] or "",
        MANAGE_ALL_FILES_WARNING = manage_all_files and [[
> [!WARNING]
> This project was generated with `--manage-all-files`. It declares Android's
> broad `MANAGE_EXTERNAL_STORAGE` permission and opens the app-specific
> **All files access** settings once on first startup when access is missing.
> Grant it only when the application genuinely needs unrestricted shared
> storage access. You can revoke it under **Settings > Apps > Special app
> access > All files access**.]] or ""
    }
    for _, field in ipairs(template_record.template.fields) do
        local value = values[field.name]
        if (value == nil or value == "") and field.default ~= "" then
            value = field.default
        end
        if (value == nil or value == "") and field.required then
            error("required template field is missing: " .. field.name, 0)
        end
        if value ~= nil then
            variables[field.variable] = value
        end
    end

    template.render_tree(template_root, destination, variables)

    -- Template packages retain the legacy marker so older 0.6 development
    -- validators can import them. Newly generated projects expose only the
    -- squared-pg identity.
    local legacy_marker = path.join(destination, ".sdl-pg.lua")
    if fs.mode(legacy_marker) == "file" then
        fs.remove_tree(legacy_marker)
    end

    for _, module_record in ipairs(resolved_modules) do
        package_registry.apply_module(
            settings,
            destination,
            module_record.id,
            module_record.version
        )
    end

    local project_kit = path.join(destination, "third_party/SDL2")
    fs.copy_tree_transactional(kit_root, project_kit)

    for _, unneeded in ipairs({"java", "tests"}) do
        local candidate = path.join(project_kit, unneeded)
        if fs.exists(candidate) then
            fs.remove_tree(candidate)
        end
    end

    fs.copy_tree_transactional(
        path.join(kit_root, "java/org"),
        path.join(destination, "app/src/main/java/org")
    )

    fs.copy_tree_transactional(
        lua_root,
        path.join(destination, "third_party/lua-5.4.8")
    )

    fs.copy_file(
        path.join(settings.generator_root, "licenses/Lua-LICENSE.txt"),
        path.join(destination, "licenses/Lua-LICENSE.txt")
    )
    fs.copy_file(
        path.join(
            settings.generator_root,
            "licenses/DejaVu-Fonts-LICENSE.txt"
        ),
        path.join(destination, "licenses/DejaVu-Fonts-LICENSE.txt")
    )

    copy_wrapper(wrapper_root, destination)
end

return android
