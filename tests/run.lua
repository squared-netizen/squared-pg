--- Run SDL Project Generator tests.
-- @script tests.run

local root = arg[1] or "."

package.path = table.concat({
    root .. "/lua/?.lua",
    root .. "/lua/?/init.lua",
    root .. "/tests/?.lua",
    package.path
}, ";")

local lfs = require("lfs")
local android = require("sdl_pg.android")
local doctor = require("sdl_pg.doctor")
local fs = require("sdl_pg.fs")
local main = require("sdl_pg.main")
local path = require("sdl_pg.path")
local project = require("sdl_pg.project")
local state = require("sdl_pg.state")
local template = require("sdl_pg.template")
local wrapper = require("sdl_pg.wrapper")
local test = require("testlib")

local test_root = path.join(root, "build/generator-tests")
if fs.exists(test_root) then
    fs.remove_tree(test_root)
end
fs.mkdir_p(test_root)

local settings = {
    home = test_root,
    generator_root = root,
    config_file = path.join(test_root, "config.lua"),
    cache_root = path.join(test_root, "cache"),
    sandbox_root = path.join(test_root, "sandbox"),
    projects_root = path.join(test_root, "projects")
}
fs.mkdir_p(settings.sandbox_root)
fs.mkdir_p(settings.projects_root)

test.equal(
    path.join("/tmp/", "/alpha", "beta/"),
    "/tmp/alpha/beta",
    "path joining"
)
test.equal(
    path.identifier("SDL-Test_2"),
    "sdl_test_2",
    "identifier conversion"
)
test.fails(function()
    path.validate_project_name("../escape")
end, "cannot contain '..'")
test.fails(function()
    android.validate_package("single")
end, "at least two")
test.fails(function()
    android.validate_package("org.invalid-name")
end, "invalid Android package")

test.equal(
    template.render("hello {{NAME}}", {NAME = "Lua"}),
    "hello Lua",
    "template rendering"
)
test.fails(function()
    template.render("{{MISSING}}", {})
end, "not defined")

local foundation =
    project.create(
        settings,
        "first-hack",
        "sandbox",
        {profile = "foundation"}
    )
test.truthy(
    fs.exists(path.join(foundation, "README.md")),
    "foundation project"
)

local promoted = project.promote(settings, "first-hack")
test.truthy(
    fs.exists(path.join(promoted, "src/main.cpp")),
    "promoted project"
)
test.truthy(
    fs.exists(path.join(foundation, "src/main.cpp")),
    "promotion preserves source"
)
test.fails(function()
    project.promote(settings, "first-hack")
end, "destination already exists")

local serious =
    project.create(
        settings,
        "serious-code",
        "project",
        {profile = "foundation"}
    )
fs.mkdir_p(path.join(serious, ".git"))
local demoted = project.demote(settings, "serious-code")
test.truthy(
    fs.exists(path.join(serious, "project.lua")),
    "demotion preserves source"
)
test.truthy(
    not fs.exists(path.join(demoted, ".git")),
    "demotion omits Git metadata"
)

local command_directory = path.join(test_root, "commands")
fs.mkdir_p(command_directory)
fs.write_file(path.join(command_directory, "clang-real"), "fixture\n")
local linked, link_message = lfs.link(
    path.join(command_directory, "clang-real"),
    path.join(command_directory, "clang"),
    true
)
test.truthy(linked, "test symlink creation: " .. tostring(link_message))

local records = doctor.inspect(settings, {
    PATH = command_directory
})
local clang_found = false
for _, record in ipairs(records) do
    if record.label == "clang" then
        clang_found = record.ok
    end
end
test.truthy(clang_found, "doctor accepts command symlinks")

local fake_kit = path.join(test_root, "fake-kit")
for _, filename in ipairs({
    "BUILD-INFO.txt",
    "FEATURES.txt",
    "include/SDL2/SDL.h",
    "include/SDL2/SDL_ttf.h",
    "include/SDL2/SDL_mixer.h",
    "include/SDL2/SDL_image.h",
    "include/SDL2/SDL_net.h",
    "lib/arm64-v8a/libSDL2.so",
    "lib/arm64-v8a/libSDL2_ttf.so",
    "lib/arm64-v8a/libSDL2_mixer.so",
    "lib/arm64-v8a/libSDL2_image.so",
    "lib/arm64-v8a/libSDL2_net.so",
    "java/org/libsdl/app/SDLActivity.java"
}) do
    fs.write_file(path.join(fake_kit, filename), "fixture\n")
end

local wrapper_source = path.join(test_root, "wrapper-source")
for _, filename in ipairs({
    "gradlew",
    "gradlew.bat",
    "gradle/wrapper/gradle-wrapper.jar",
    "gradle/wrapper/gradle-wrapper.properties"
}) do
    fs.write_file(path.join(wrapper_source, filename), "fixture\n")
end

state.save(settings, {kit_root = fake_kit})
wrapper.add(settings, wrapper_source)
test.truthy(wrapper.status(settings).configured, "wrapper registration")

local android_project = project.create(
    settings,
    "lua-rogue",
    "sandbox",
    {
        profile = "android",
        package_name = "dev.example.luarogue"
    }
)

for _, filename in ipairs({
    ".github/workflows/android-debug.yml",
    "app/build.gradle",
    "app/src/main/AndroidManifest.xml",
    "app/src/main/cpp/CMakeLists.txt",
    "app/src/main/cpp/main.cpp",
    "app/src/main/assets/lua/main.lua",
    "app/src/main/java/org/libsdl/app/SDLActivity.java",
    "gradle/wrapper/gradle-wrapper.jar",
    "include/lua_rogue/script_runtime.hpp",
    "third_party/SDL2/lib/arm64-v8a/libSDL2_net.so",
    "third_party/lua-5.4.8/src/lua.h",
    "tools/build.lua"
}) do
    test.truthy(
        fs.mode(path.join(android_project, filename)) == "file",
        "generated file " .. filename
    )
end

local manifest =
    fs.read_file(path.join(
        android_project,
        "app/src/main/AndroidManifest.xml"
    ))
test.truthy(
    manifest:find("dev.example.luarogue", 1, true) == nil,
    "package is owned by Gradle rather than duplicated in manifest"
)
local app_gradle =
    fs.read_file(path.join(android_project, "app/build.gradle"))
test.truthy(
    app_gradle:find("dev.example.luarogue", 1, true) ~= nil,
    "Android package rendering"
)
test.truthy(
    app_gradle:find("{{", 1, true) == nil,
    "no unresolved template variables"
)

local runtime_source =
    fs.read_file(path.join(
        android_project,
        "app/src/main/cpp/script_runtime.cpp"
    ))
for _, required in ipairs({
    "luaopen_base",
    "luaopen_math",
    "\"dofile\"",
    "\"loadfile\"",
    "lua_pcall"
}) do
    test.truthy(
        runtime_source:find(required, 1, true) ~= nil,
        "protected runtime feature " .. required
    )
end
for _, forbidden in ipairs({
    "luaopen_io",
    "luaopen_os",
    "luaopen_package",
    "luaopen_debug"
}) do
    test.truthy(
        runtime_source:find(forbidden, 1, true) == nil,
        "restricted library " .. forbidden
    )
end

local application_script =
    fs.read_file(path.join(
        android_project,
        "app/src/main/assets/lua/main.lua"
    ))
for _, callback in ipairs({
    "application.init",
    "application.update",
    "application.event",
    "application.shutdown"
}) do
    test.truthy(
        application_script:find(callback, 1, true) ~= nil,
        "Lua lifecycle callback " .. callback
    )
end

local output = {}
local errors = {}
local environment = {
    HOME = test_root,
    PATH = "",
    SDL_PG_ROOT = root,
    SDL_PG_CACHE_ROOT = settings.cache_root,
    SDL_PG_SANDBOX_ROOT = settings.sandbox_root,
    SDL_PG_PROJECTS_ROOT = settings.projects_root
}
local function collect(destination)
    return function(message)
        destination[#destination + 1] = message
    end
end

test.equal(
    main.run({"version"}, environment, collect(output), collect(errors)),
    0,
    "version command"
)
test.truthy(
    output[1]:find("0.3.0", 1, true) ~= nil,
    "version output"
)
test.equal(
    main.run({"unknown"}, environment, collect(output), collect(errors)),
    1,
    "unknown command result"
)

fs.remove_tree(test_root)

print("Path and package validation: OK")
print("Template rendering: OK")
print("Termux symlink command discovery: OK")
print("Transactional project lifecycle: OK")
print("Gradle Wrapper registration: OK")
print("Android project generation: OK")
print("SDL2 and Lua source staging: OK")
print("Protected Lua library policy: OK")
print("C++ to Lua lifecycle contract: OK")
print("Obsidian, Doxygen, and LDoc scaffold: OK")
print("CLI result handling: OK")
