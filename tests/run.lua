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
local documentation = require("sdl_pg.docs")
local fs = require("sdl_pg.fs")
local main = require("sdl_pg.main")
local path = require("sdl_pg.path")
local plugin_runtime_test = require("plugin-runtime-test")
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
local foundation_marker =
    assert(loadfile(path.join(foundation, ".sdl-pg.lua")))()
test.equal(
    foundation_marker.generator_version,
    "0.5.0",
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
    ".github/workflows/docs.yml",
    "app/build.gradle",
    "app/src/main/AndroidManifest.xml",
    "app/src/main/cpp/CMakeLists.txt",
    "app/src/main/cpp/application/CMakeLists.txt",
    "app/src/main/cpp/application/src/application.cpp",
    "app/src/main/cpp/application/src/script_runtime.cpp",
    "app/src/main/cpp/platform/sdl_main.cpp",
    "app/src/main/cpp/squared/data/json.cpp",
    "app/src/main/cpp/squared/graphics/context.cpp",
    "app/src/main/cpp/squared/graphics2d/orthographic_camera.cpp",
    "app/src/main/cpp/squared/graphics2d/sprite_batch.cpp",
    "app/src/main/cpp/squared/graphics2d/texture.cpp",
    "app/src/main/cpp/squared/graphics2d/texture_atlas.cpp",
    "app/src/main/cpp/squared/math/matrix4.cpp",
    "app/src/main/cpp/squared/messaging/message_dispatcher.cpp",
    "app/src/main/cpp/squared/time/timepiece.cpp",
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
    "app/src/main/java/org/libsdl/app/SDLActivity.java",
    "docs/Plugins.md",
    "docs/Application.md",
    "docs/API.md",
    "docs/Doxyfile.cpp",
    "docs/Doxyfile.java",
    "docs/Data.md",
    "docs/Graphics2D.md",
    "docs/Messaging.md",
    "docs/Time.md",
    "docs/ldoc-config.ld",
    "gradle/wrapper/gradle-wrapper.jar",
    "app/src/main/cpp/application/include/lua_rogue/application.hpp",
    "app/src/main/cpp/application/include/lua_rogue/script_runtime.hpp",
    "include/squared/application/application.hpp",
    "include/squared/application/event.hpp",
    "include/squared/data/json.hpp",
    "include/squared/graphics/color.hpp",
    "include/squared/graphics/context.hpp",
    "include/squared/graphics2d/orthographic_camera.hpp",
    "include/squared/graphics2d/sprite.hpp",
    "include/squared/graphics2d/sprite_batch.hpp",
    "include/squared/graphics2d/texture.hpp",
    "include/squared/graphics2d/texture_atlas.hpp",
    "include/squared/graphics2d/texture_region.hpp",
    "include/squared/math/matrix4.hpp",
    "include/squared/math/vector2.hpp",
    "include/squared/messaging/message_dispatcher.hpp",
    "include/squared/messaging/telegram.hpp",
    "include/squared/messaging/telegram_provider.hpp",
    "include/squared/messaging/telegraph.hpp",
    "include/squared/time/deadline_queue.hpp",
    "include/squared/time/timepiece.hpp",
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
    "android.permission.WRITE_EXTERNAL_STORAGE",
    "android.permission.MANAGE_EXTERNAL_STORAGE"
}) do
    test.truthy(
        manifest:find(permission, 1, true) ~= nil,
        "native diagnostic storage permission " .. permission
    )
end
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
local android_marker =
    assert(loadfile(path.join(android_project, ".sdl-pg.lua")))()
test.equal(
    android_marker.generator_version,
    "0.5.0",
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
    "add_library(yyjson STATIC",
    "add_library(squared_data STATIC",
    "add_library(squared_time STATIC",
    "add_library(squared_messaging STATIC",
    "add_library(squared_graphics2d STATIC",
    "add_subdirectory(application)",
    "platform/sdl_main.cpp",
    "project_application",
    "find_library(SQUARED_GLES2_LIBRARY NAMES GLESv2)",
    "/system/lib64/libGLESv2.so",
    "squared/data/json.cpp",
    "squared/messaging/message_dispatcher.cpp",
    "squared/time/timepiece.cpp",
    "squared/graphics2d/sprite_batch.cpp",
    "squared/graphics2d/texture_atlas.cpp"
}) do
    test.truthy(
        native_cmake:find(required, 1, true) ~= nil,
        "graphics2d build contract " .. required
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
    "squared_time",
    "squared_messaging"
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
    "squared/graphics/context.cpp",
    "squared/graphics2d/sprite_batch.cpp",
    "squared/graphics2d/texture.cpp"
}) do
    local source = fs.read_file(path.join(
        android_project,
        "app/src/main/cpp",
        source_name
    ))
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
    "include/squared/graphics2d/texture_atlas.hpp"
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
    "app/src/main/cpp/squared/graphics2d/texture_atlas.cpp"
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
    "include/squared/messaging/message_dispatcher.hpp"
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
    "include/squared/messaging/telegram_provider.hpp"
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
    "include/squared/messaging/telegram.hpp"
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
    "-DYYJSON_ROOT=",
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

local json_header = fs.read_file(path.join(
    android_project,
    "include/squared/data/json.hpp"
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
    "app/src/main/cpp/squared/data/json.cpp"
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
    output[1]:find("0.5.0", 1, true) ~= nil,
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
print("Squared strict JSON staging: OK")
print("Squared OpenGL ES graphics2d foundation: OK")
print("Protected Lua library policy: OK")
print("C++ to Lua lifecycle contract: OK")
print("Deterministic plug-in lifecycle: OK")
print("Plug-in capability and module isolation: OK")
print("Obsidian, Doxygen, and LDoc scaffold: OK")
print("Local and GitHub documentation workflow: OK")
print("CLI result handling: OK")
