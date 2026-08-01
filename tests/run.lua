--- Run Squared Project Generator tests.
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
local config = require("sdl_pg.config")
local dependency = require("sdl_pg.dependency")
local doctor = require("sdl_pg.doctor")
local documentation = require("sdl_pg.docs")
local fs = require("sdl_pg.fs")
local main = require("sdl_pg.main")
local package_builder = require("sdl_pg.package_builder")
local package_registry = require("sdl_pg.package_registry")
local path = require("sdl_pg.path")
local plugin_runtime_test = require("plugin-runtime-test")
local provider = require("sdl_pg.provider")
local project = require("sdl_pg.project")
local state = require("sdl_pg.state")
local template = require("sdl_pg.template")
local template_selection = require("sdl_pg.template_selection")
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

local naming_home = path.join(test_root, "naming-home")
local legacy_config =
    path.join(naming_home, ".config/sdl-pg/config.lua")
local legacy_cache = path.join(naming_home, ".local/share/sdl-pg")
fs.mkdir_p(legacy_cache)
fs.write_file(legacy_config, table.concat({
    "return {",
    "    cache_root = " .. string.format("%q", legacy_cache),
    "}",
    ""
}, "\n"))
local migrated_settings = config.load({HOME = naming_home})
test.equal(
    migrated_settings.config_file,
    legacy_config,
    "legacy configuration fallback"
)
test.equal(
    migrated_settings.cache_root,
    legacy_cache,
    "legacy cache reuse"
)
local primary_settings = config.load({
    HOME = naming_home,
    SQUARED_PG_ROOT = "primary-root",
    SDL_PG_ROOT = "legacy-root"
})
test.equal(
    primary_settings.generator_root,
    "primary-root",
    "squared-pg environment precedence"
)

local application_package = package_registry.add(
    settings,
    path.join(
        root,
        "build/packages/squared-application-0.6.0-dev.1.sq"
    )
)
test.equal(
    application_package.id,
    "dev.squarednetizen.squared.application",
    "built-in Application package registration"
)
local graphics_package = package_registry.add(
    settings,
    path.join(root, "build/packages/squared-graphics-0.6.0-dev.1.sq")
)
test.equal(
    graphics_package.id,
    "dev.squarednetizen.squared.graphics",
    "built-in Graphics package registration"
)
local math_package = package_registry.add(
    settings,
    path.join(root, "build/packages/squared-math-0.6.0-dev.1.sq")
)
test.equal(
    math_package.id,
    "dev.squarednetizen.squared.math",
    "built-in Math package registration"
)
local graphics2d_package = package_registry.add(
    settings,
    path.join(
        root,
        "build/packages/squared-graphics2d-0.6.0-dev.1.sq"
    )
)
test.equal(
    graphics2d_package.id,
    "dev.squarednetizen.squared.graphics2d",
    "built-in Graphics2D package registration"
)
local scene2d_package = package_registry.add(
    settings,
    path.join(root, "build/packages/squared-scene2d-0.6.0-dev.1.sq")
)
test.equal(
    scene2d_package.id,
    "dev.squarednetizen.squared.scene2d",
    "built-in Scene2D package registration"
)
local time_package = package_registry.add(
    settings,
    path.join(root, "build/packages/squared-time-0.6.0-dev.1.sq")
)
test.equal(
    time_package.id,
    "dev.squarednetizen.squared.time",
    "built-in Time package registration"
)
local data_package = package_registry.add(
    settings,
    path.join(root, "build/packages/squared-data-0.6.0-dev.1.sq")
)
test.equal(
    data_package.id,
    "dev.squarednetizen.squared.data",
    "built-in Data package registration"
)
local messaging_package = package_registry.add(
    settings,
    path.join(
        root,
        "build/packages/squared-messaging-0.6.0-dev.1.sq"
    )
)
test.equal(
    messaging_package.id,
    "dev.squarednetizen.squared.messaging",
    "built-in Messaging package registration"
)
local android_template_package = package_registry.add(
    settings,
    path.join(
        root,
        "build/packages/squared-android-template-0.6.0-dev.14.sq"
    )
)
test.equal(
    android_template_package.id,
    "dev.squarednetizen.template.android-sdl2-lua",
    "built-in Android template registration"
)
local original_require_template = package_registry.require_template
package_registry.require_template = function()
    return {
        id = "dev.example.template.unsupported",
        version = "1.0.0",
        template = {profile = "unsupported_frontend"}
    }, {}
end
test.fails(function()
    project.create(
        settings,
        "unsupported-provider-hack",
        "sandbox",
        {profile = "template"}
    )
end, "unsupported template profile: unsupported_frontend")
package_registry.require_template = original_require_template
test.truthy(
    not fs.exists(path.join(
        settings.sandbox_root,
        "unsupported-provider-hack"
    )),
    "unsupported provider fails before destination creation"
)

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
test.fails(function()
    project.create(
        settings,
        "invalid-app-name",
        "sandbox",
        {
            profile = "template",
            package_name = "dev.example.invalidappname",
            application_name = "Unsafe <name>"
        }
    )
end, "application name must use")
test.fails(function()
    project.create(
        settings,
        "missing-dependency-hack",
        "sandbox",
        {
            profile = "template",
            package_name = "dev.example.missingdependency"
        }
    )
end, "no SDL2 kit is registered")
test.truthy(
    not fs.exists(path.join(
        settings.sandbox_root,
        "missing-dependency-hack"
    )),
    "missing frontend dependency fails before destination creation"
)
test.fails(function()
    dependency.status(settings, "unknown-frontend")
end, "unknown dependency provider")

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
local foundation_marker =
    assert(loadfile(path.join(foundation, ".squared-pg.lua")))()
test.equal(
    foundation_marker.generator,
    "squared-pg",
    "foundation generator identity"
)
test.equal(
    foundation_marker.generator_version,
    "0.6.0-dev.5",
    "foundation generator version"
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

local legacy_project =
    project.create(
        settings,
        "legacy-marker-hack",
        "sandbox",
        {profile = "foundation"}
    )
assert(os.rename(
    path.join(legacy_project, ".squared-pg.lua"),
    path.join(legacy_project, ".sdl-pg.lua")
))
local legacy_promoted = project.promote(settings, "legacy-marker-hack")
test.truthy(
    fs.exists(path.join(legacy_promoted, ".sdl-pg.lua")),
    "legacy project marker compatibility"
)

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

local linked_output_root = path.join(test_root, "linked-output-real")
fs.mkdir_p(linked_output_root)
local absolute_linked_output_root = linked_output_root
if absolute_linked_output_root:sub(1, 1) ~= "/" then
    absolute_linked_output_root = path.join(
        lfs.currentdir(),
        absolute_linked_output_root
    )
end
local output_linked, output_link_message = lfs.link(
    absolute_linked_output_root,
    path.join(test_root, "linked-output"),
    true
)
test.truthy(
    output_linked,
    "output-directory symlink creation: " .. tostring(output_link_message)
)
fs.mkdir_p(
    path.join(test_root, "linked-output/packages"),
    {follow_directory_links = true}
)
test.equal(
    fs.mode(path.join(linked_output_root, "packages")),
    "directory",
    "explicit output-directory symlink traversal"
)

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
local provider_resolution = provider.resolve(
    settings,
    "provider-proof",
    {package_name = "dev.example.providerproof"}
)
test.equal(
    provider_resolution.profile,
    "android_sdl2_lua",
    "default template provider profile"
)
test.equal(
    provider_resolution.adapter,
    android,
    "Android SDL2 provider selection"
)
test.equal(
    provider_resolution.dependencies.frontend.id,
    "android-sdl2",
    "Android SDL2 dependency preflight"
)

local android_project = project.create(
    settings,
    "lua-rogue",
    "sandbox",
    {
        profile = "template",
        package_name = "dev.example.luarogue",
        application_name = "Lua Rogue Test",
        base_version = "2.3.4"
    }
)

for _, filename in ipairs({
    ".github/workflows/android-debug.yml",
    ".github/workflows/docs.yml",
    "app/build.gradle",
    "app/src/main/AndroidManifest.xml",
    "app/src/main/cpp/CMakeLists.txt",
    "app/src/main/cpp/application/CMakeLists.txt",
    "app/src/main/cpp/application/src/application.cpp",
    "app/src/main/cpp/application/src/script_runtime.cpp",
    "app/src/main/cpp/platform/sdl_main.cpp",
    "modules/squared-graphics/CMakeLists.txt",
    "modules/squared-graphics/src/context.cpp",
    "modules/squared-graphics/include/squared/graphics/color.hpp",
    "modules/squared-graphics/include/squared/graphics/context.hpp",
    "modules/squared-graphics2d/CMakeLists.txt",
    "modules/squared-graphics2d/src/orthographic_camera.cpp",
    "modules/squared-graphics2d/src/sprite_batch.cpp",
    "modules/squared-graphics2d/src/texture.cpp",
    "modules/squared-graphics2d/src/texture_atlas.cpp",
    "modules/squared-graphics2d/include/squared/graphics2d/orthographic_camera.hpp",
    "modules/squared-graphics2d/include/squared/graphics2d/sprite.hpp",
    "modules/squared-graphics2d/include/squared/graphics2d/sprite_batch.hpp",
    "modules/squared-graphics2d/include/squared/graphics2d/texture.hpp",
    "modules/squared-graphics2d/include/squared/graphics2d/texture_atlas.hpp",
    "modules/squared-graphics2d/include/squared/graphics2d/texture_region.hpp",
    "modules/squared-scene2d/CMakeLists.txt",
    "modules/squared-scene2d/src/actor.cpp",
    "modules/squared-scene2d/src/group.cpp",
    "modules/squared-scene2d/src/stage.cpp",
    "modules/squared-scene2d/include/squared/scene2d/actor.hpp",
    "modules/squared-scene2d/include/squared/scene2d/group.hpp",
    "modules/squared-scene2d/include/squared/scene2d/stage.hpp",
    "modules/squared-math/CMakeLists.txt",
    "modules/squared-math/src/matrix4.cpp",
    "modules/squared-math/include/squared/math/matrix4.hpp",
    "modules/squared-math/include/squared/math/vector2.hpp",
    "modules/squared-data/CMakeLists.txt",
    "modules/squared-data/src/json.cpp",
    "modules/squared-data/include/squared/data/json.hpp",
    "modules/squared-time/CMakeLists.txt",
    "modules/squared-time/src/timepiece.cpp",
    "app/src/main/assets/diagnostics/json-ttf-status.json",
    "app/src/main/assets/fonts/DejaVuSansMono.ttf",
    "app/src/main/assets/graphics/lifecycle-status.atlas",
    "app/src/main/assets/graphics/lifecycle-status.png",
    "app/src/main/assets/lua/bootstrap.lua",
    "app/src/main/assets/lua/plugins.lua",
    "app/src/main/assets/lua/runtime/module_loader.lua",
    "app/src/main/assets/lua/runtime/plugin_manager.lua",
    "app/src/main/assets/lua/plugins/diagnostics/manifest.lua",
    "app/src/main/assets/lua/plugins/diagnostics/main.lua",
    "app/src/main/assets/lua/plugins/orbit/manifest.lua",
    "app/src/main/assets/lua/plugins/orbit/main.lua",
    "app/src/main/assets/lua/plugins/orbit/palette.lua",
    "app/src/main/java/dev/squarednetizen/generated/SquaredActivity.java",
    "app/src/main/java/org/libsdl/app/SDLActivity.java",
    "docs/Plugins.md",
    "docs/Application.md",
    "docs/Squared-Application.md",
    "docs/API.md",
    "docs/Doxyfile.cpp",
    "docs/Doxyfile.java",
    "docs/Data.md",
    "docs/Graphics2D.md",
    "docs/Scene2D.md",
    "docs/Graphics.md",
    "docs/Messaging.md",
    "docs/Math.md",
    "docs/Time.md",
    "docs/ldoc-config.ld",
    "gradle/wrapper/gradle-wrapper.jar",
    "app/src/main/cpp/application/include/lua_rogue/application.hpp",
    "app/src/main/cpp/application/include/lua_rogue/script_runtime.hpp",
    "modules/squared-time/include/squared/time/deadline_queue.hpp",
    "modules/squared-time/include/squared/time/timepiece.hpp",
    "modules/squared-messaging/CMakeLists.txt",
    "modules/squared-messaging/src/message_dispatcher.cpp",
    "modules/squared-messaging/include/squared/messaging/message_dispatcher.hpp",
    "modules/squared-messaging/include/squared/messaging/telegram.hpp",
    "modules/squared-messaging/include/squared/messaging/telegram_provider.hpp",
    "modules/squared-messaging/include/squared/messaging/telegraph.hpp",
    "modules/squared-application/CMakeLists.txt",
    "modules/squared-application/include/squared/application/application.hpp",
    "modules/squared-application/include/squared/application/event.hpp",
    "third_party/SDL2/lib/arm64-v8a/libSDL2_net.so",
    "third_party/lua-5.4.8/src/lua.h",
    "third_party/yyjson-0.12.0/src/yyjson.c",
    "third_party/yyjson-0.12.0/src/yyjson.h",
    "licenses/DejaVu-Fonts-LICENSE.txt",
    "licenses/yyjson-LICENSE.txt",
    "tools/build.lua"
}) do
    test.truthy(
        fs.mode(path.join(android_project, filename)) == "file",
        "generated file " .. filename
    )
end

for _, obsolete in ipairs({
    "app/src/main/cpp/squared/data/json.cpp",
    "include/squared/data/json.hpp",
    "app/src/main/cpp/squared/messaging/message_dispatcher.cpp",
    "include/squared/messaging/message_dispatcher.hpp",
    "include/squared/messaging/telegram.hpp",
    "include/squared/messaging/telegram_provider.hpp",
    "include/squared/messaging/telegraph.hpp",
    "include/squared/application/application.hpp",
    "include/squared/application/event.hpp",
    "app/src/main/cpp/squared/graphics/context.cpp",
    "include/squared/graphics/color.hpp",
    "include/squared/graphics/context.hpp",
    "app/src/main/cpp/squared/graphics2d/orthographic_camera.cpp",
    "app/src/main/cpp/squared/graphics2d/sprite_batch.cpp",
    "app/src/main/cpp/squared/graphics2d/texture.cpp",
    "app/src/main/cpp/squared/graphics2d/texture_atlas.cpp",
    "include/squared/graphics2d/orthographic_camera.hpp",
    "include/squared/graphics2d/sprite.hpp",
    "include/squared/graphics2d/sprite_batch.hpp",
    "include/squared/graphics2d/texture.hpp",
    "include/squared/graphics2d/texture_atlas.hpp",
    "include/squared/graphics2d/texture_region.hpp",
    "app/src/main/cpp/squared/math/matrix4.cpp",
    "include/squared/math/matrix4.hpp",
    "include/squared/math/vector2.hpp"
}) do
    test.truthy(
        not fs.exists(path.join(android_project, obsolete)),
        "template no longer owns extracted module file " .. obsolete
    )
end

for _, generator_only in ipairs({
    "lua/sdl_pg/provider.lua",
    "lua/sdl_pg/dependency.lua",
    "app/src/main/assets/lua/sdl_pg/provider.lua",
    "app/src/main/assets/lua/sdl_pg/dependency.lua"
}) do
    test.truthy(
        not fs.exists(path.join(android_project, generator_only)),
        "generated project omits dispatcher " .. generator_only
    )
end

local documentation_plan = documentation.plan(
    settings,
    path.join(android_project, "app/src/main")
)
test.equal(
    documentation_plan.project_root,
    android_project,
    "documentation project-root discovery"
)
test.equal(
    #documentation_plan.commands,
    3,
    "documentation command count"
)
test.truthy(
    documentation_plan.outputs.lua:find(
        "build/docs/lua/index.html",
        1,
        true
    ) ~= nil,
    "Lua documentation output"
)

for _, filename in ipairs({
    "docs/ldoc-config.ld",
    "docs/Doxyfile.cpp",
    "docs/Doxyfile.java"
}) do
    local contents = fs.read_file(path.join(android_project, filename))
    test.truthy(
        contents:find("RECURSIVE", 1, true) ~= nil or
            contents:find(
                "app/src/main/assets/lua",
                1,
                true
            ) ~= nil,
        "recursive documentation configuration " .. filename
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
test.truthy(
    manifest:find("0x00020000", 1, true) ~= nil,
    "OpenGL ES 2 manifest requirement"
)
for _, permission in ipairs({
    "android.permission.READ_EXTERNAL_STORAGE",
    "android.permission.WRITE_EXTERNAL_STORAGE"
}) do
    test.truthy(
        manifest:find(permission, 1, true) ~= nil,
        "native diagnostic storage permission " .. permission
    )
end
test.truthy(
    manifest:find("android.permission.MANAGE_EXTERNAL_STORAGE", 1, true) == nil,
    "broad storage permission is opt-in"
)
local app_gradle =
    fs.read_file(path.join(android_project, "app/build.gradle"))
test.truthy(
    app_gradle:find("dev.example.luarogue", 1, true) ~= nil,
    "Android package rendering"
)
test.truthy(
    app_gradle:find('versionName "2.3.4"', 1, true) ~= nil,
    "Android base-version rendering"
)
test.truthy(
    app_gradle:find(
        '"MANAGE_ALL_FILES_ON_FIRST_START",\n            "false"',
        1,
        true
    ) ~= nil,
    "broad storage first-start behavior is disabled by default"
)
local special_access_activity = fs.read_file(path.join(
    android_project,
    "app/src/main/java/dev/squarednetizen/generated/SquaredActivity.java"
))
for _, value in ipairs({
    "BuildConfig.MANAGE_ALL_FILES_ON_FIRST_START",
    "import android.content.ActivityNotFoundException;",
    "ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION",
    'Uri.parse("package:" + getPackageName())',
    "REQUESTED_ALL_FILES"
}) do
    test.truthy(
        special_access_activity:find(value, 1, true) ~= nil,
        "one-time Android special-access activity " .. value
    )
end
test.truthy(
    special_access_activity:find(
        "import android.app.ActivityNotFoundException;",
        1,
        true
    ) == nil,
    "special-access activity uses the Android content exception package"
)
test.truthy(
    fs.read_file(path.join(android_project, "README.md")):find(
        "This project was generated with `--manage-all-files`",
        1,
        true
    ) == nil,
    "default README omits broad storage warning"
)
test.truthy(
    fs.read_file(path.join(
        android_project,
        "app/src/main/res/values/strings.xml"
    )):find("Lua Rogue Test", 1, true) ~= nil,
    "Android application-name rendering"
)
test.truthy(
    app_gradle:find("{{", 1, true) == nil,
    "no unresolved template variables"
)
local android_marker =
    assert(loadfile(path.join(android_project, ".squared-pg.lua")))()
test.truthy(
    not fs.exists(path.join(android_project, ".sdl-pg.lua")),
    "new Android project omits legacy generator marker"
)
test.equal(
    android_marker.generator,
    "squared-pg",
    "Android generator identity"
)
test.equal(
    android_marker.generator_version,
    "0.6.0-dev.5",
    "Android generator version"
)
local android_metadata =
    assert(loadfile(path.join(android_project, "project.lua")))()
test.equal(
    android_metadata.generator_version,
    android_marker.generator_version,
    "Android metadata version agreement"
)
test.equal(
    android_metadata.plugin_api_version,
    1,
    "Android plug-in API version"
)

local runtime_source =
    fs.read_file(path.join(
        android_project,
        "app/src/main/cpp/application/src/script_runtime.cpp"
    ))
for _, required in ipairs({
    "luaopen_base",
    "luaopen_math",
    "\"_read_asset\"",
    "\"runtime_version\"",
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
local bootstrap_script =
    fs.read_file(path.join(
        android_project,
        "app/src/main/assets/lua/bootstrap.lua"
    ))
for _, contract in ipairs({
    "lua/runtime/module_loader.lua",
    "lua/runtime/plugin_manager.lua",
    "api_version = 1",
    "host = nil",
    "load = nil"
}) do
    test.truthy(
        bootstrap_script:find(contract, 1, true) ~= nil,
        "Lua bootstrap contract " .. contract
    )
end

plugin_runtime_test.run(root, test)

local application_source =
    fs.read_file(path.join(
        android_project,
        "app/src/main/cpp/application/src/application.cpp"
    ))
test.truthy(
    application_source:find("SCENE2D: ", 1, true) ~= nil and
        application_source:find(
            "squared::scene2d::Stage",
            1,
            true
        ) ~= nil,
    "Scene2D on-device hierarchy diagnostic"
)
for _, required in ipairs({
    "squared::graphics::Context",
    "squared::graphics2d::OrthographicCamera",
    "squared::graphics2d::SpriteBatch",
    "squared::graphics2d::TextureRegion",
    "squared::graphics2d::TextureAtlas",
    "find_region(\"lifecycle-status\", 0)",
    "find_region(\"lifecycle-status\", 1)",
    "lifecycle_marked_ = !lifecycle_marked_",
    "using red diagnostic square",
    "squared::data::parse_json",
    "squared::data::write_json",
    "TTF_OpenFont",
    "TTF_RenderUTF8_Blended",
    "diagnostics/json-ttf-status.json",
    "deterministic serialization changed",
    "squared::messaging::MessageDispatcher",
    "squared::messaging::Telegraph",
    "squared::messaging::TelegramProvider",
    "messages_.register_provider",
    "ProviderResult::provided",
    "sample.state.provider-ready",
    "provider_state_received_",
    "verify_pending_snapshot",
    "snapshot_pending",
    "restore_pending",
    "pending_snapshot_ready_",
    "/sdcard/Download/",
    "-diagnostics.log",
    "std::filesystem::create_directories",
    "std::ofstream",
    "TTF_RenderUTF8_Blended_Wrapped",
    "ATLAS: ",
    "JSON_TTF: ",
    "PROVIDER: ",
    "SNAPSHOT: ",
    "TIME: ",
    "LOG: ",
    "MESSAGE: ",
    "telegram_json",
    "messages_.schedule",
    "messages_.update",
    "sample.time.indicator-toggle"
}) do
    test.truthy(
        application_source:find(required, 1, true) ~= nil,
        "graphics2d application contract " .. required
    )
end

local platform_source =
    fs.read_file(path.join(
        android_project,
        "app/src/main/cpp/platform/sdl_main.cpp"
    ))
for _, required in ipairs({
    "create_application()",
    "translate_event",
    "application->handle_event",
    "application->update",
    "application->render",
    "application->pause",
    "application->resume",
    "application->surface_created",
    "application->surface_destroyed",
    "application->dispose"
}) do
    test.truthy(
        platform_source:find(required, 1, true) ~= nil,
        "SDL application adapter contract " .. required
    )
end
for _, forbidden in ipairs({
    "TextureAtlas",
    "ScriptRuntime",
    "TTF_OpenFont",
    "parse_json"
}) do
    test.truthy(
        platform_source:find(forbidden, 1, true) == nil,
        "SDL adapter excludes application concern " .. forbidden
    )
end

local json_ttf_fixture = fs.read_file(path.join(
    android_project,
    "app/src/main/assets/diagnostics/json-ttf-status.json"
))
for _, required in ipairs({
    [["message"]],
    [["font"]],
    [["pointSize"]],
    [["textColor"]],
    [["panelColor"]],
    [[\u2192]],
    [[\u2502]]
}) do
    test.truthy(
        json_ttf_fixture:find(required, 1, true) ~= nil,
        "JSON/TTF diagnostic fixture " .. required
    )
end

local diagnostic_font = fs.read_file(path.join(
    android_project,
    "app/src/main/assets/fonts/DejaVuSansMono.ttf"
))
test.equal(
    diagnostic_font:sub(1, 4),
    "\0\1\0\0",
    "diagnostic asset uses TrueType"
)
for _, forbidden in ipairs({
    "SDL_CreateRenderer",
    "SDL_RenderFillRect",
    "SDL_RenderPresent"
}) do
    test.truthy(
        application_source:find(forbidden, 1, true) == nil,
        "OpenGL ES path excludes " .. forbidden
    )
end

local native_cmake =
    fs.read_file(path.join(
        android_project,
        "app/src/main/cpp/CMakeLists.txt"
    ))
for _, required in ipairs({
    "squared_register_module",
    "SQUARED_MODULE_DIRECTORIES",
    "TARGET squared_graphics2d",
    "TARGET squared_scene2d",
    "add_subdirectory(application)",
    "platform/sdl_main.cpp",
    "project_application",
    "find_library(SQUARED_GLES2_LIBRARY NAMES GLESv2)",
    "/system/lib64/libGLESv2.so",
    "squared_graphics",
    "squared_math"
}) do
    test.truthy(
        native_cmake:find(required, 1, true) ~= nil,
        "graphics2d build contract " .. required
    )
end

local scene2d_cmake = fs.read_file(path.join(
    android_project,
    "modules/squared-scene2d/CMakeLists.txt"
))
for _, required in ipairs({
    "TARGET squared_graphics2d",
    "GLOB_RECURSE SQUARED_SCENE2D_SOURCES",
    "add_library(squared_scene2d STATIC",
    "target_link_libraries(squared_scene2d PUBLIC",
    "squared_graphics2d",
    "squared_register_module(squared_scene2d)"
}) do
    test.truthy(
        scene2d_cmake:find(required, 1, true) ~= nil,
        "Squared Scene2D module build contract " .. required
    )
end

local graphics2d_cmake = fs.read_file(path.join(
    android_project,
    "modules/squared-graphics2d/CMakeLists.txt"
))
for _, required in ipairs({
    "foreach(required_target SDL2 SDL2_image)",
    "SDL2_INCLUDE_DIR",
    "SQUARED_GLES2_LIBRARY",
    "GLOB_RECURSE SQUARED_GRAPHICS2D_SOURCES",
    "add_library(squared_graphics2d STATIC",
    "target_link_libraries(squared_graphics2d PUBLIC",
    "squared_graphics",
    "squared_math",
    "squared_register_module(squared_graphics2d)"
}) do
    test.truthy(
        graphics2d_cmake:find(required, 1, true) ~= nil,
        "Squared Graphics2D module build contract " .. required
    )
end

local graphics_cmake = fs.read_file(path.join(
    android_project,
    "modules/squared-graphics/CMakeLists.txt"
))
for _, required in ipairs({
    "TARGET SDL2",
    "SDL2_INCLUDE_DIR",
    "SQUARED_GLES2_LIBRARY",
    "GLOB_RECURSE SQUARED_GRAPHICS_SOURCES",
    "add_library(squared_graphics STATIC",
    "target_link_libraries(squared_graphics PUBLIC",
    "squared_register_module(squared_graphics)"
}) do
    test.truthy(
        graphics_cmake:find(required, 1, true) ~= nil,
        "Squared Graphics module build contract " .. required
    )
end

local math_cmake = fs.read_file(path.join(
    android_project,
    "modules/squared-math/CMakeLists.txt"
))
for _, required in ipairs({
    "GLOB_RECURSE SQUARED_MATH_SOURCES",
    "CONFIGURE_DEPENDS",
    "add_library(squared_math STATIC",
    "POSITION_INDEPENDENT_CODE ON",
    "target_include_directories(squared_math PUBLIC",
    "squared_register_module(squared_math)"
}) do
    test.truthy(
        math_cmake:find(required, 1, true) ~= nil,
        "Squared Math module build contract " .. required
    )
end

local messaging_cmake = fs.read_file(path.join(
    android_project,
    "modules/squared-messaging/CMakeLists.txt"
))
for _, required in ipairs({
    "GLOB_RECURSE SQUARED_MESSAGING_SOURCES",
    "CONFIGURE_DEPENDS",
    "add_library(squared_messaging STATIC",
    "squared_data",
    "squared_time",
    "squared_register_module(squared_messaging)"
}) do
    test.truthy(
        messaging_cmake:find(required, 1, true) ~= nil,
        "Squared Messaging module build contract " .. required
    )
end

local application_module_cmake = fs.read_file(path.join(
    android_project,
    "modules/squared-application/CMakeLists.txt"
))
for _, required in ipairs({
    "add_library(squared_application INTERFACE)",
    "target_compile_features(squared_application INTERFACE cxx_std_20)",
    "target_include_directories(squared_application INTERFACE",
    "squared_register_module(squared_application)"
}) do
    test.truthy(
        application_module_cmake:find(required, 1, true) ~= nil,
        "Squared Application module build contract " .. required
    )
end
test.truthy(
    application_module_cmake:find(
        "GLOB_RECURSE",
        1,
        true
    ) == nil,
    "header-only Application module has no source discovery"
)

local data_cmake = fs.read_file(path.join(
    android_project,
    "modules/squared-data/CMakeLists.txt"
))
for _, required in ipairs({
    "add_library(squared_data_yyjson STATIC",
    "add_library(squared_data STATIC",
    "GLOB_RECURSE SQUARED_DATA_SOURCES",
    "CONFIGURE_DEPENDS",
    "squared_register_module(squared_data)"
}) do
    test.truthy(
        data_cmake:find(required, 1, true) ~= nil,
        "Squared Data module build contract " .. required
    )
end

local time_cmake = fs.read_file(path.join(
    android_project,
    "modules/squared-time/CMakeLists.txt"
))
for _, required in ipairs({
    "GLOB_RECURSE SQUARED_TIME_SOURCES",
    "CONFIGURE_DEPENDS",
    "add_library(squared_time STATIC",
    "squared_register_module(squared_time)"
}) do
    test.truthy(
        time_cmake:find(required, 1, true) ~= nil,
        "Squared Time module build contract " .. required
    )
end

local application_cmake =
    fs.read_file(path.join(
        android_project,
        "app/src/main/cpp/application/CMakeLists.txt"
    ))
for _, required in ipairs({
    "GLOB_RECURSE PROJECT_APPLICATION_SOURCES",
    "CONFIGURE_DEPENDS",
    "/src/*.cpp",
    "add_library(project_application STATIC",
    "squared_application",
    "squared_messaging",
    "squared_scene2d"
}) do
    test.truthy(
        application_cmake:find(required, 1, true) ~= nil,
        "developer application build contract " .. required
    )
end
test.truthy(
    fs.exists(path.join(
        android_project,
        "app/src/main/cpp/main.cpp"
    )) == false,
    "obsolete generated main.cpp is absent"
)
for _, source_name in ipairs({
    "modules/squared-graphics/src/context.cpp",
    "modules/squared-graphics2d/src/sprite_batch.cpp",
    "modules/squared-graphics2d/src/texture.cpp"
}) do
    local source = fs.read_file(path.join(android_project, source_name))
    for _, header in ipairs({
        "SDL_opengles2_khrplatform.h",
        "SDL_opengles2_gl2platform.h",
        "SDL_opengles2_gl2.h"
    }) do
        test.truthy(
            source:find(header, 1, true) ~= nil,
            "SDL-bundled GLES2 header " ..
                header ..
                " in " ..
                source_name
        )
    end
    test.truthy(
        source:find("GLES3/gl3.h", 1, true) == nil,
        "no external GLES3 header " .. source_name
    )
end

local atlas_header = fs.read_file(path.join(
    android_project,
    "modules/squared-graphics2d/include/squared/graphics2d/texture_atlas.hpp"
))
for _, required in ipairs({
    "class TextureAtlas final",
    "std::vector<std::unique_ptr<Texture>>",
    "std::optional<std::array<int, 4>>",
    "find_region"
}) do
    test.truthy(
        atlas_header:find(required, 1, true) ~= nil,
        "texture atlas public contract " .. required
    )
end

local atlas_source = fs.read_file(path.join(
    android_project,
    "modules/squared-graphics2d/src/texture_atlas.cpp"
))
for _, required in ipairs({
    "parse_atlas",
    "safe_relative_path",
    "TextureWrap::Repeat",
    "parsed.rotated",
    "duplicate region",
    "textures_.swap(textures)",
    "regions_.swap(regions)"
}) do
    test.truthy(
        atlas_source:find(required, 1, true) ~= nil,
        "texture atlas implementation contract " .. required
    )
end

local messaging_header = fs.read_file(path.join(
    android_project,
    "modules/squared-messaging/include/squared/messaging/message_dispatcher.hpp"
))
for _, required in ipairs({
    "class MessageDispatcher final",
    "register_endpoint",
    "register_provider",
    "subscribe",
    "send(Telegram telegram)",
    "send_now",
    "schedule",
    "cancel",
    "inspect_pending",
    "snapshot_pending",
    "restore_pending",
    "PendingSnapshotStatus",
    "PendingRestoreStatus",
    "maximum_deliveries_per_update"
}) do
    test.truthy(
        messaging_header:find(required, 1, true) ~= nil,
        "messaging public contract " .. required
    )
end

local provider_header = fs.read_file(path.join(
    android_project,
    "modules/squared-messaging/include/squared/messaging/telegram_provider.hpp"
))
for _, required in ipairs({
    "class TelegramProvider",
    "struct ProviderResult",
    "enum class ProviderStatus",
    "NoCurrentState",
    "provide"
}) do
    test.truthy(
        provider_header:find(required, 1, true) ~= nil,
        "Telegram provider public contract " .. required
    )
end

local telegram_header = fs.read_file(path.join(
    android_project,
    "modules/squared-messaging/include/squared/messaging/telegram.hpp"
))
for _, required in ipairs({
    "class MessageId final",
    "class EndpointId final",
    "class Telegram final",
    "CorrelationId",
    "ReceiptStatus",
    "receipt_message_id"
}) do
    test.truthy(
        telegram_header:find(required, 1, true) ~= nil,
        "Telegram public contract " .. required
    )
end

local lifecycle_atlas = fs.read_file(path.join(
    android_project,
    "app/src/main/assets/graphics/lifecycle-status.atlas"
))
for _, required in ipairs({
    "lifecycle-status.png",
    "filter: Nearest, Nearest",
    "index: 0",
    "index: 1"
}) do
    test.truthy(
        lifecycle_atlas:find(required, 1, true) ~= nil,
        "lifecycle atlas fixture " .. required
    )
end

local lifecycle_png = fs.read_file(path.join(
    android_project,
    "app/src/main/assets/graphics/lifecycle-status.png"
))
test.equal(
    lifecycle_png:sub(1, 8),
    "\137PNG\r\n\26\n",
    "lifecycle atlas page uses PNG"
)

local build_driver =
    fs.read_file(path.join(android_project, "tools/build.lua"))
for _, required in ipairs({
    "yyjson-0.12.0",
    "SDL_opengles2_khrplatform.h",
    "SDL_opengles2_gl2platform.h",
    "SDL_opengles2_gl2.h",
    "/system/lib64/libGLESv2.so"
}) do
    test.truthy(
        build_driver:find(required, 1, true) ~= nil,
        "Termux GLES2 build prerequisite " .. required
    )
end
test.truthy(
    build_driver:find("-DYYJSON_ROOT=", 1, true) == nil,
    "Squared Data owns yyjson build configuration"
)

local json_header = fs.read_file(path.join(
    android_project,
    "modules/squared-data/include/squared/data/json.hpp"
))
for _, required in ipairs({
    "namespace squared::data",
    "class JsonValue final",
    "reject_duplicate_keys{true}",
    "parse_json",
    "write_json"
}) do
    test.truthy(
        json_header:find(required, 1, true) ~= nil,
        "strict JSON public contract " .. required
    )
end

local json_source = fs.read_file(path.join(
    android_project,
    "modules/squared-data/src/json.cpp"
))
for _, required in ipairs({
    "#include <yyjson.h>",
    "YYJSON_READ_NOFLAG",
    "JsonErrorCode::DuplicateKey",
    "YYJSON_WRITE_PRETTY_TWO_SPACES",
    "YYJSON_WRITE_NEWLINE_AT_END"
}) do
    test.truthy(
        json_source:find(required, 1, true) ~= nil,
        "strict JSON implementation contract " .. required
    )
end

local output = {}
local errors = {}
local environment = {
    HOME = test_root,
    PATH = "",
    SQUARED_PG_ROOT = root,
    SQUARED_PG_CACHE_ROOT = settings.cache_root,
    SQUARED_PG_SANDBOX_ROOT = settings.sandbox_root,
    SQUARED_PG_PROJECTS_ROOT = settings.projects_root,
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
    output[1]:find("squared-pg 0.6.0-dev.5", 1, true) ~= nil,
    "version output"
)
output = {}
errors = {}
test.equal(
    main.run(
        {"dependency", "status", "android-sdl2"},
        environment,
        collect(output),
        collect(errors)
    ),
    0,
    "dependency-provider status command"
)
test.truthy(
    output[1]:find("android-sdl2  configured", 1, true) ~= nil,
    "dependency-provider status output"
)
output = {}
errors = {}
test.equal(
    main.run(
        {"kit", "status"},
        environment,
        collect(output),
        collect(errors)
    ),
    0,
    "legacy kit status command"
)
test.truthy(
    output[1]:find("SDL2 kit: ", 1, true) ~= nil,
    "legacy kit compatibility output"
)
output = {}
errors = {}
test.equal(
    main.run(
        {"package", "status"},
        environment,
        collect(output),
        collect(errors)
    ),
    0,
    "package status command"
)
test.truthy(
    table.concat(output, "\n"):find(
        "dev.squarednetizen.squared.time",
        1,
        true
    ) ~= nil,
    "package status Time output"
)
test.truthy(
    table.concat(output, "\n"):find(
        "dev.squarednetizen.squared.data",
        1,
        true
    ) ~= nil,
    "package status Data output"
)
test.truthy(
    table.concat(output, "\n"):find(
        "dev.squarednetizen.squared.messaging",
        1,
        true
    ) ~= nil,
    "package status Messaging output"
)
test.truthy(
    table.concat(output, "\n"):find(
        "dev.squarednetizen.squared.application",
        1,
        true
    ) ~= nil,
    "package status Application output"
)
test.truthy(
    table.concat(output, "\n"):find(
        "dev.squarednetizen.squared.graphics",
        1,
        true
    ) ~= nil,
    "package status Graphics output"
)
test.truthy(
    table.concat(output, "\n"):find(
        "dev.squarednetizen.squared.graphics2d",
        1,
        true
    ) ~= nil,
    "package status Graphics2D output"
)
test.truthy(
    table.concat(output, "\n"):find(
        "dev.squarednetizen.squared.math",
        1,
        true
    ) ~= nil,
    "package status Math output"
)
test.truthy(
    table.concat(output, "\n"):find(
        "dev.squarednetizen.squared.scene2d",
        1,
        true
    ) ~= nil,
    "package status Scene2D output"
)
output = {}
errors = {}
test.equal(
    main.run(
        {
            "package",
            "resolve",
            "dev.squarednetizen.template.android-sdl2-lua@0.6.0-dev.14"
        },
        environment,
        collect(output),
        collect(errors)
    ),
    0,
    "package resolve command"
)
test.equal(#output, 9, "package resolve output count")
test.truthy(
    output[1]:find(
        "dev.squarednetizen.squared.application@0.6.0-dev.1",
        1,
        true
    ) ~= nil,
    "package resolve Application dependency order"
)
test.truthy(
    output[2]:find(
        "dev.squarednetizen.squared.data@0.6.0-dev.1",
        1,
        true
    ) ~= nil,
    "package resolve Data dependency order"
)
test.truthy(
    output[3]:find(
        "dev.squarednetizen.squared.time@0.6.0-dev.1",
        1,
        true
    ) ~= nil,
    "package resolve Time dependency order"
)
test.truthy(
    output[4]:find(
        "dev.squarednetizen.squared.messaging@0.6.0-dev.1",
        1,
        true
    ) ~= nil,
    "package resolve Messaging dependency order"
)
test.truthy(
    output[5]:find(
        "dev.squarednetizen.squared.graphics@0.6.0-dev.1",
        1,
        true
    ) ~= nil,
    "package resolve Graphics dependency order"
)
test.truthy(
    output[6]:find(
        "dev.squarednetizen.squared.math@0.6.0-dev.1",
        1,
        true
    ) ~= nil,
    "package resolve Math dependency order"
)
test.truthy(
    output[7]:find(
        "dev.squarednetizen.squared.graphics2d@0.6.0-dev.1",
        1,
        true
    ) ~= nil,
    "package resolve Graphics2D dependency order"
)
test.truthy(
    output[8]:find(
        "dev.squarednetizen.squared.scene2d@0.6.0-dev.1",
        1,
        true
    ) ~= nil,
    "package resolve Scene2D dependency order"
)
test.truthy(
    output[9]:find(
        "dev.squarednetizen.template.android-sdl2-lua@0.6.0-dev.14",
        1,
        true
    ) ~= nil and
        output[9]:find("(root)", 1, true) ~= nil,
    "package resolve root"
)

local selected_template, selected_dependencies = template_selection.use(
    settings,
    "dev.squarednetizen.template.android-sdl2-lua",
    "0.6.0-dev.14"
)
test.equal(
    selected_template.version,
    "0.6.0-dev.14",
    "active template selection"
)
test.equal(#selected_dependencies, 8, "selected template dependency count")
local selection_status = template_selection.status(settings)
test.equal(selection_status.source, "selected", "persisted template source")
test.truthy(selection_status.resolvable, "persisted template resolution")

local package_output = path.join(test_root, "external/squared-time.sq")
local built_package = package_builder.build(
    path.join(root, "packages/squared-time"),
    package_output
)
test.equal(
    built_package.id,
    "dev.squarednetizen.squared.time",
    "external package build identity"
)
test.equal(
    package_builder.verify(package_output).content_digest,
    built_package.content_digest,
    "external package verification"
)
test.fails(function()
    package_builder.build(
        path.join(root, "packages/squared-time"),
        package_output
    )
end, "already exists")

output = {}
errors = {}
local cli_package_output =
    path.join(test_root, "linked-output/packages/cli-time.sq")
test.equal(
    main.run(
        {
            "package", "build",
            path.join(root, "packages/squared-time"),
            cli_package_output
        },
        environment,
        collect(output),
        collect(errors)
    ),
    0,
    "package build command"
)
test.truthy(
    table.concat(output, "\n"):find("Built SQ module", 1, true) ~= nil,
    "package build output"
)
output = {}
errors = {}
test.equal(
    main.run(
        {"package", "verify", cli_package_output},
        environment,
        collect(output),
        collect(errors)
    ),
    0,
    "package verify command"
)
test.equal(output[1], "SQ package: OK", "package verify output")
output = {}
errors = {}
test.equal(
    main.run(
        {
            "template", "use",
            "dev.squarednetizen.template.android-sdl2-lua@0.6.0-dev.14"
        },
        environment,
        collect(output),
        collect(errors)
    ),
    0,
    "template use command"
)
test.truthy(
    output[1]:find("Selected template:", 1, true) ~= nil,
    "template use output"
)

output = {}
errors = {}
test.equal(
    main.run(
        {
            "new",
            "cli-template-hack",
            "--package",
            "dev.example.clitemplatehack",
            "--template",
            "dev.squarednetizen.template.android-sdl2-lua@0.6.0-dev.14",
            "--app-name",
            "CLI Template Proof",
            "--base-version",
            "4.5.6",
            "--manage-all-files"
        },
        environment,
        collect(output),
        collect(errors)
    ),
    0,
    "explicit template project command"
)
local cli_project =
    path.join(settings.sandbox_root, "cli-template-hack")
test.truthy(
    fs.read_file(path.join(
        cli_project,
        "app/src/main/res/values/strings.xml"
    )):find("CLI Template Proof", 1, true) ~= nil,
    "CLI template application-name form field"
)
test.truthy(
    fs.read_file(path.join(
        cli_project,
        "app/build.gradle"
    )):find('versionName "4.5.6"', 1, true) ~= nil,
    "CLI template base-version form field"
)
local cli_manifest = fs.read_file(path.join(
    cli_project,
    "app/src/main/AndroidManifest.xml"
))
test.truthy(
    cli_manifest:find(
        "android.permission.MANAGE_EXTERNAL_STORAGE",
        1,
        true
    ) ~= nil,
    "CLI broad storage manifest switch"
)
test.truthy(
    fs.read_file(path.join(cli_project, "app/build.gradle")):find(
        '"MANAGE_ALL_FILES_ON_FIRST_START",\n            "true"',
        1,
        true
    ) ~= nil,
    "CLI broad storage first-start switch"
)
local cli_readme = fs.read_file(path.join(cli_project, "README.md"))
test.truthy(
    cli_readme:find(
        "This project was generated with `--manage-all-files`",
        1,
        true
    ) ~= nil and
        cli_readme:find("Special app", 1, true) ~= nil,
    "CLI broad storage README warning"
)
output = {}
errors = {}
test.equal(
    main.run(
        {"project", "verify", cli_project},
        environment,
        collect(output),
        collect(errors)
    ),
    0,
    "generated project verification command"
)
test.truthy(
    table.concat(output, "\n"):find("Verification: OK", 1, true) ~= nil,
    "generated project verification output"
)
output = {}
errors = {}
test.equal(
    main.run(
        {"template", "status"},
        environment,
        collect(output),
        collect(errors)
    ),
    0,
    "template status command"
)
test.truthy(
    table.concat(output, "\n"):find("Resolvable: true", 1, true) ~= nil,
    "template status output"
)
output = {}
errors = {}
test.equal(
    main.run({"verbose"}, environment, collect(output), collect(errors)),
    0,
    "verbose report command"
)
test.truthy(
    table.concat(output, "\n"):find("Registered packages (9)", 1, true) ~= nil,
    "verbose package inventory"
)
output = {}
errors = {}
test.equal(
    main.run(
        {"agent-feedback"},
        environment,
        collect(output),
        collect(errors)
    ),
    0,
    "agent feedback command"
)
test.equal(output[1], "SQUARED_PG_AGENT_FEEDBACK_V1", "agent report schema")
test.truthy(#output <= 32, "agent report line bound")
test.truthy(#(table.concat(output, "\n") .. "\n") <= 4096,
    "agent report byte bound")
test.truthy(
    output[#output]:match("^truncated=") ~= nil,
    "agent report truncation marker"
)

local fake_bin = path.join(test_root, "fake-bin")
fs.mkdir_p(fake_bin)
for _, command in ipairs({
    "lua5.4", "clang", "cmake", "ninja", "ctest", "java"
}) do
    fs.write_file(path.join(fake_bin, command), "")
end
local healthy_environment = {}
for key, value in pairs(environment) do healthy_environment[key] = value end
healthy_environment.PATH = fake_bin
output = {}
errors = {}
test.equal(
    main.run(
        {"self-test"},
        healthy_environment,
        collect(output),
        collect(errors)
    ),
    0,
    "installed generator self-test command"
)
test.truthy(
    table.concat(output, "\n"):find("agent feedback byte bound", 1, true) ~= nil,
    "self-test agent feedback bound"
)
output = {}
errors = {}
test.equal(
    main.run(
        {"new", "invalid-storage", "--foundation", "--manage-all-files"},
        environment,
        collect(output),
        collect(errors)
    ),
    1,
    "broad storage switch rejected for foundation profile"
)
test.truthy(
    errors[1]:find("only valid for Android", 1, true) ~= nil,
    "broad storage profile error"
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
print("Squared strict JSON staging: OK")
print("Squared Messaging module staging: OK")
print("Squared Application header-only staging: OK")
print("Squared Graphics module staging: OK")
print("Squared Math module staging: OK")
print("Squared Graphics2D module staging: OK")
print("Squared Scene2D module staging: OK")
print("Protected Lua library policy: OK")
print("C++ to Lua lifecycle contract: OK")
print("Deterministic plug-in lifecycle: OK")
print("Plug-in capability and module isolation: OK")
print("Obsidian, Doxygen, and LDoc scaffold: OK")
print("Local and GitHub documentation workflow: OK")
print("CLI result handling: OK")
