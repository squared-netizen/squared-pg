--- Generated-project API documentation orchestration.
-- @module sdl_pg.docs

local lfs = require("lfs")
local fs = require("sdl_pg.fs")
local path = require("sdl_pg.path")
local process = require("sdl_pg.process")

local docs = {}
local marker_names = {".squared-pg.lua", ".sdl-pg.lua"}

local function find_project_root(start)
    local current = start

    while true do
        for _, marker_name in ipairs(marker_names) do
            if fs.mode(path.join(current, marker_name)) == "file" then
                return current
            end
        end

        local parent = path.dirname(current)
        if parent == current or current == "/" or parent == "." then
            break
        end
        current = parent
    end

    error(
        "current directory is not inside a squared-pg project",
        0
    )
end

local function require_file(filename, description)
    if fs.mode(filename) ~= "file" then
        error(description .. " is missing: " .. filename, 0)
    end
end

--- Prepare deterministic documentation commands for a generated project.
-- This function performs validation but does not execute external programs.
-- @param settings Resolved generator configuration.
-- @param[opt] start Directory at or below the generated project root.
-- @return Documentation execution plan.
function docs.plan(settings, start)
    if not settings.generator_root then
        error("generator root is unavailable", 0)
    end

    local project_root = find_project_root(start or lfs.currentdir())
    local private_prefix =
        path.join(settings.generator_root, "build/private-lua")
    local private_lua =
        path.join(private_prefix, "bin/lua-5.4.8")
    local private_run =
        path.join(settings.generator_root, "tools/private-run.lua")
    local ldoc =
        path.join(private_prefix, "share/lua/5.4/ldoc.lua")

    require_file(private_lua, "private Lua interpreter")
    require_file(private_run, "private Lua environment runner")
    require_file(ldoc, "private LDoc entry point")
    require_file(
        path.join(project_root, "docs/ldoc-config.ld"),
        "generated-project LDoc configuration"
    )
    require_file(
        path.join(project_root, "docs/Doxyfile.cpp"),
        "C++ Doxygen configuration"
    )
    require_file(
        path.join(project_root, "docs/Doxyfile.java"),
        "Java Doxygen configuration"
    )

    return {
        project_root = project_root,
        commands = {
            {
                private_lua,
                private_run,
                ldoc,
                "-c",
                "docs/ldoc-config.ld",
                "-d",
                "build/docs/lua",
                "app/src/main/assets/lua"
            },
            {"doxygen", "docs/Doxyfile.cpp"},
            {"doxygen", "docs/Doxyfile.java"}
        },
        outputs = {
            lua = path.join(project_root, "build/docs/lua/index.html"),
            cpp = path.join(project_root, "build/docs/cpp/html/index.html"),
            java =
                path.join(project_root, "build/docs/java/html/index.html")
        }
    }
end

--- Generate Lua, C++, and Java API documentation.
-- @param settings Resolved generator configuration.
-- @param[opt] start Directory at or below the generated project root.
-- @return Paths to the three generated API indexes.
function docs.build(settings, start)
    local plan = docs.plan(settings, start)

    for _, command in ipairs(plan.commands) do
        process.run(command, plan.project_root)
    end

    for language, filename in pairs(plan.outputs) do
        if fs.mode(filename) ~= "file" then
            error(
                language .. " documentation did not produce " .. filename,
                0
            )
        end
    end

    return plan.outputs
end

return docs
