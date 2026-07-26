--- Android SDL2 project population.
-- @module sdl_pg.android

local fs = require("sdl_pg.fs")
local kit = require("sdl_pg.kit")
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

--- Populate a staged directory with an Android SDL2 project.
-- @param settings Resolved generator configuration.
-- @param destination Empty staged project directory.
-- @param name Project name.
-- @param options Android generation options.
function android.populate(settings, destination, name, options)
    android.validate_package(options.package_name)

    if not settings.generator_root then
        error("generator root is unavailable", 0)
    end

    local kit_root = kit.require_active(settings)
    local wrapper_root = wrapper.require_active(settings)
    local lua_root =
        path.join(settings.generator_root, "third_party/lua-5.4.8")
    local template_root =
        path.join(settings.generator_root, "templates/android")

    if fs.mode(lua_root) ~= "directory" then
        error(
            "private Lua sources are not extracted; " ..
            "run lua5.4 toolchain.lua in the generator",
            0
        )
    end

    local identifier = path.identifier(name)
    local variables = {
        PROJECT_NAME = name,
        PROJECT_ID = identifier,
        PROJECT_TITLE = display_name(name),
        PACKAGE_NAME = options.package_name
    }

    template.render_tree(template_root, destination, variables)

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

    copy_wrapper(wrapper_root, destination)
end

return android
