local lfs = require("lfs")
local sq = require("squared.sq")

local root = assert(arg[1], "repository root argument is required")
local work = root .. "/build/sq-lua-binding-test"

local function remove_tree(path)
    local mode = lfs.symlinkattributes(path, "mode")
    if not mode then return end
    if mode == "directory" then
        for name in lfs.dir(path) do
            if name ~= "." and name ~= ".." then
                remove_tree(path .. "/" .. name)
            end
        end
        assert(lfs.rmdir(path))
    else
        assert(os.remove(path))
    end
end

local function mkdir_p(path)
    local current = path:sub(1, 1) == "/" and "/" or ""
    for component in path:gmatch("[^/]+") do
        current = current == "/" and current .. component or
            (current == "" and component or current .. "/" .. component)
        if not lfs.attributes(current, "mode") then
            assert(lfs.mkdir(current))
        end
    end
end

local function write(path, value)
    mkdir_p(path:match("^(.*)/[^/]+$"))
    local file = assert(io.open(path, "wb"))
    assert(file:write(value))
    assert(file:close())
end

remove_tree(work)
write(work .. "/sha256.txt", "abc")
assert(
    sq.sha256_file(work .. "/sha256.txt") ==
        "ba7816bf8f01cfea414140de5dae2223" ..
        "b00361a396177a9cb410ff61f20015ad"
)
local missing_hash_ok, missing_hash_error = pcall(function()
    sq.sha256_file(work .. "/missing-sha256.txt")
end)
assert(not missing_hash_ok)
assert(tostring(missing_hash_error):find("cannot open file", 1, true))

mkdir_p(
    work ..
        "/content/modules/squared-time/include/squared/time"
)
write(
    work ..
        "/content/modules/squared-time/include/squared/time/timepiece.hpp",
    "#pragma once\n"
)
write(
    work .. "/content/modules/squared-time/CMakeLists.txt",
    "add_library(squared_time INTERFACE)\n"
)

local created, create_error = sq.create_from_directory({
    manifest = {
        kind = "module",
        id = "dev.squarednetizen.squared.time",
        version = "0.6.0-dev.1",
        name = "Squared Time",
        description = "Lua binding proof",
        module = {
            directory = "modules/squared-time",
            cmake_target = "squared_time",
            requires = {
                {
                    kind = "module",
                    id = "dev.squarednetizen.squared.base",
                    version = "1.0.0"
                }
            }
        }
    },
    content_directory = work .. "/content",
    destination = work .. "/squared-time.sq"
})
assert(created, create_error and create_error.message)
assert(created.file_count == 2)
assert(#created.archive_sha256 == 64)

local package <close>, open_error = sq.open(work .. "/squared-time.sq")
assert(package, open_error and open_error.message)
local manifest = package:manifest()
assert(manifest.kind == "module")
assert(manifest.id == "dev.squarednetizen.squared.time")
assert(manifest.module.directory == "modules/squared-time")
assert(manifest.module.cmake_target == "squared_time")
assert(#manifest.module.requires == 1)
assert(
    manifest.module.requires[1].id ==
        "dev.squarednetizen.squared.base"
)

local report, validation_error = package:validate({
    expected_kind = "module"
})
assert(report, validation_error and validation_error.message)
assert(report.valid)
assert(#report.content_digest == 64)

local extracted, extraction_error =
    package:extract_transactionally(
        work .. "/extracted",
        {expected_kind = "module"}
    )
assert(extracted, extraction_error and extraction_error.message)
assert(extracted.files_written == 2)

package:close()
local closed, closed_error = pcall(function()
    package:manifest()
end)
assert(not closed)
assert(tostring(closed_error):find("closed", 1, true))

write(
    work .. "/template-content/template/.squared-pg.lua",
    "return {format = 3}\n"
)
write(
    work .. "/template-content/template/.sdl-pg.lua",
    "return {format = 3}\n"
)
write(
    work .. "/template-content/template/app/build.gradle",
    'versionName "{{BASE_VERSION}}"\n'
)
local template_created, template_create_error = sq.create_from_directory({
    manifest = {
        kind = "template",
        id = "dev.squarednetizen.template.android-sdl2-lua",
        version = "0.6.0-dev.1",
        name = "Squared Android SDL2 Lua",
        template = {
            profile = "android_sdl2_lua",
            directory = "template",
            executable_hooks = false,
            fields = {
                {
                    name = "project_name",
                    variable = "PROJECT_NAME",
                    required = true
                },
                {
                    name = "base_version",
                    variable = "BASE_VERSION",
                    required = false,
                    default = "0.1.0"
                }
            },
            requires = {
                {
                    kind = "module",
                    id = "dev.squarednetizen.squared.time",
                    version = "0.6.0-dev.1"
                }
            }
        }
    },
    content_directory = work .. "/template-content",
    destination = work .. "/android-template.sq"
})
assert(
    template_created,
    template_create_error and template_create_error.message
)
local template_package <close>, template_open_error =
    sq.open(work .. "/android-template.sq")
assert(
    template_package,
    template_open_error and template_open_error.message
)
local template_manifest = template_package:manifest()
assert(template_manifest.kind == "template")
assert(template_manifest.template.profile == "android_sdl2_lua")
assert(#template_manifest.template.fields == 2)
assert(#template_manifest.template.requires == 1)
template_package:close()

remove_tree(work)
print("Squared SQ private Lua binding: OK")
