--- Legacy SDL Project Generator command entry point.
-- @script sdl-pg

local script_path = arg[0] or "sdl-pg.lua"
local root = script_path:match("^(.*)/[^/]+$") or "."

if root:sub(1, 1) ~= "/" then
    local lfs = require("lfs")
    root = lfs.currentdir() .. "/" .. root:gsub("^%./", "")
end

_G.SQUARED_PG_ROOT = root
_G.SDL_PG_ROOT = root

package.path = table.concat({
    root .. "/lua/?.lua",
    root .. "/lua/?/init.lua",
    package.path
}, ";")

local arguments = {}
for index = 1, #arg do
    arguments[index] = arg[index]
end

local main = require("sdl_pg.main")
os.exit(main.run(arguments), true)
