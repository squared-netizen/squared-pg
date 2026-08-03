--- Install private Lua launchers and user configuration.
-- @script setup

if _VERSION ~= "Lua 5.4" then
    io.stderr:write("ERROR: setup.lua requires Lua 5.4\n")
    os.exit(1)
end

local script_path = arg[0] or "setup.lua"
local root = script_path:match("^(.*)/[^/]+$") or "."
local absolute_root = root

if root:sub(1, 1) ~= "/" then
    local lfs = require("lfs")
    local relative_root = root:gsub("^%./", "")
    absolute_root = relative_root == "." and lfs.currentdir() or
        lfs.currentdir() .. "/" .. relative_root
end

package.path = table.concat({
    absolute_root .. "/lua/?.lua",
    absolute_root .. "/lua/?/init.lua",
    package.path
}, ";")

_G.SQUARED_PG_ROOT = absolute_root
_G.SDL_PG_ROOT = absolute_root

local fs = require("sdl_pg.fs")
local path = require("sdl_pg.path")
local package_registry = require("sdl_pg.package_registry")
local config = require("sdl_pg.config")

local action = arg[1] or "install"
if action ~= "install" then
    io.stderr:write("Usage: setup.lua install\n")
    os.exit(2)
end

local home = os.getenv("HOME")
if not home or home == "" then
    io.stderr:write("ERROR: HOME is not set\n")
    os.exit(1)
end

local canonical_root = path.join(home, ".squared/squared-pg")
if absolute_root ~= canonical_root then
    io.stderr:write(
        "ERROR: Squared Project Generator must be installed from ",
        canonical_root,
        "\n",
        "Move or clone the repository there, rebuild the private ",
        "toolchain, and run setup again.\n"
    )
    os.exit(1)
end

local private_lua =
    path.join(absolute_root, "build/private-lua/bin/lua-5.4.8")
if fs.mode(private_lua) ~= "file" then
    io.stderr:write(
        "ERROR: private Lua is missing; run lua5.4 toolchain.lua first\n"
    )
    os.exit(1)
end

local bin_directory =
    os.getenv("SQUARED_PG_BIN_DIR") or
    os.getenv("SDL_PG_BIN_DIR") or
    path.join(home, ".bin")
local config_directory = path.join(home, ".config/squared-pg")
local config_file = path.join(config_directory, "config.lua")
local legacy_config_file =
    path.join(home, ".config/sdl-pg/config.lua")
local primary_cache_root = path.join(home, ".local/share/squared-pg")
local legacy_cache_root = path.join(home, ".local/share/sdl-pg")
local default_cache_root =
    fs.mode(legacy_cache_root) == "directory" and
    legacy_cache_root or primary_cache_root

fs.mkdir_p(bin_directory)
fs.mkdir_p(config_directory)
fs.mkdir_p(path.join(home, "sandbox"))
fs.mkdir_p(path.join(home, "projects"))

if not fs.exists(config_file) then
    if fs.mode(legacy_config_file) == "file" then
        fs.copy_file(legacy_config_file, config_file)
    else
        fs.write_file(config_file, table.concat({
            "--- User configuration for Squared Project Generator.",
            "",
            "return {",
            "    sandbox_root = " ..
                string.format("%q", path.join(home, "sandbox")) ..
                ",",
            "    projects_root = " ..
                string.format("%q", path.join(home, "projects")) ..
                ",",
            "    cache_root = " ..
                string.format("%q", default_cache_root),
            "}",
            ""
        }, "\n"))
    end
end

local settings = config.load({
    HOME = home,
    SQUARED_PG_ROOT = absolute_root,
    SQUARED_PG_CONFIG = os.getenv("SQUARED_PG_CONFIG"),
    SQUARED_PG_CACHE_ROOT = os.getenv("SQUARED_PG_CACHE_ROOT"),
    SQUARED_PG_SANDBOX_ROOT = os.getenv("SQUARED_PG_SANDBOX_ROOT"),
    SQUARED_PG_PROJECTS_ROOT = os.getenv("SQUARED_PG_PROJECTS_ROOT"),
    SDL_PG_ROOT = absolute_root,
    SDL_PG_CONFIG = os.getenv("SDL_PG_CONFIG"),
    SDL_PG_CACHE_ROOT = os.getenv("SDL_PG_CACHE_ROOT"),
    SDL_PG_SANDBOX_ROOT = os.getenv("SDL_PG_SANDBOX_ROOT"),
    SDL_PG_PROJECTS_ROOT = os.getenv("SDL_PG_PROJECTS_ROOT")
})
for _, built_in in ipairs(package_registry.builtins) do
    if not package_registry.find(
            settings,
            built_in.id,
            built_in.version
        ) then
        local archive = path.join(
            absolute_root,
            "build/packages/" .. built_in.archive
        )
        if fs.mode(archive) ~= "file" then
            io.stderr:write(
                "ERROR: built-in SQ package is missing: ",
                built_in.archive,
                "; run lua5.4 toolchain.lua first\n"
            )
            os.exit(1)
        end
        local record = package_registry.add(settings, archive)
        print(
            "Registered: " ..
            record.id ..
            " " ..
            record.version
        )
    end
end

local launcher = table.concat({
    "#!" .. private_lua,
    "",
    "package.path = table.concat({",
    "    " ..
        string.format("%q", absolute_root .. "/lua/?.lua") ..
        ",",
    "    " ..
        string.format("%q", absolute_root .. "/lua/?/init.lua") ..
        ",",
    "    " ..
        string.format(
            "%q",
            absolute_root ..
            "/build/private-lua/share/lua/5.4/?.lua"
        ) ..
        ",",
    "    " ..
        string.format(
            "%q",
            absolute_root ..
            "/build/private-lua/share/lua/5.4/?/init.lua"
        ),
    "}, \";\")",
    "",
    "package.cpath = " ..
        string.format(
            "%q",
            absolute_root .. "/build/private-lua/lib/lua/5.4/?.so"
        ),
    "",
    "_G.SQUARED_PG_ROOT = " .. string.format("%q", absolute_root),
    "_G.SQUARED_PG_PRIVATE_LUA = " .. string.format("%q", private_lua),
    "_G.SDL_PG_ROOT = " .. string.format("%q", absolute_root),
    "",
    "local arguments = {}",
    "for index = 1, #arg do",
    "    arguments[index] = arg[index]",
    "end",
    "",
    "local main = require(\"sdl_pg.main\")",
    "os.exit(main.run(arguments), true)",
    ""
}, "\n")

local launchers = {
    path.join(bin_directory, "squared-pg"),
    path.join(bin_directory, "squared-project-generator"),
    path.join(bin_directory, "sdl-pg"),
    path.join(bin_directory, "sdl-project-generator")
}

for _, filename in ipairs(launchers) do
    fs.write_file(filename, launcher)
end

local function shell_quote(value)
    return "'" .. tostring(value):gsub("'", "'\\''") .. "'"
end

local chmod_command = "chmod 755"
for _, filename in ipairs(launchers) do
    chmod_command = chmod_command .. " " .. shell_quote(filename)
end
local succeeded, reason, code = os.execute(chmod_command)

if not succeeded then
    io.stderr:write(
        "ERROR: chmod failed (",
        tostring(reason),
        " ",
        tostring(code),
        ")\n"
    )
    os.exit(1)
end

for _, filename in ipairs(launchers) do
    print("Installed: " .. filename)
end
print("Configuration: " .. config_file)

local path_value = os.getenv("PATH") or ""
local found = false

for directory in path_value:gmatch("[^:]+") do
    if directory == bin_directory then
        found = true
        break
    end
end

if not found then
    print("")
    print("Add this directory to PATH:")
    print("  " .. bin_directory)
end
