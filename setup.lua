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
    absolute_root = lfs.currentdir() .. "/" .. root:gsub("^%./", "")
end

package.path = table.concat({
    absolute_root .. "/lua/?.lua",
    absolute_root .. "/lua/?/init.lua",
    package.path
}, ";")

_G.SDL_PG_ROOT = absolute_root

local fs = require("sdl_pg.fs")
local path = require("sdl_pg.path")

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

local private_lua =
    path.join(absolute_root, "build/private-lua/bin/lua-5.4.8")
if fs.mode(private_lua) ~= "file" then
    io.stderr:write(
        "ERROR: private Lua is missing; run lua5.4 toolchain.lua first\n"
    )
    os.exit(1)
end

local bin_directory =
    os.getenv("SDL_PG_BIN_DIR") or path.join(home, ".bin")
local config_directory = path.join(home, ".config/sdl-pg")
local config_file = path.join(config_directory, "config.lua")

fs.mkdir_p(bin_directory)
fs.mkdir_p(config_directory)
fs.mkdir_p(path.join(home, "sandbox"))
fs.mkdir_p(path.join(home, "projects"))

if not fs.exists(config_file) then
    fs.write_file(config_file, table.concat({
        "--- User configuration for SDL Project Generator.",
        "",
        "return {",
        "    sandbox_root = " ..
            string.format("%q", path.join(home, "sandbox")) ..
            ",",
        "    projects_root = " ..
            string.format("%q", path.join(home, "projects")) ..
            ",",
        "    cache_root = " ..
            string.format("%q", path.join(home, ".local/share/sdl-pg")),
        "}",
        ""
    }, "\n"))
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

local short_launcher = path.join(bin_directory, "sdl-pg")
local long_launcher =
    path.join(bin_directory, "sdl-project-generator")

fs.write_file(short_launcher, launcher)
fs.write_file(long_launcher, launcher)

local function shell_quote(value)
    return "'" .. tostring(value):gsub("'", "'\\''") .. "'"
end

local chmod_command =
    "chmod 755 " ..
    shell_quote(short_launcher) ..
    " " ..
    shell_quote(long_launcher)
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

print("Installed: " .. short_launcher)
print("Installed: " .. long_launcher)
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
