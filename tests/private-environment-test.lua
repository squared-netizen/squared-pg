local root = assert(arg[1], "project root argument is required")
local private_prefix = root .. "/build/private-lua"

assert(_VERSION == "Lua 5.4")

assert(
    package.path ==
        private_prefix .. "/share/lua/5.4/?.lua;" ..
        private_prefix .. "/share/lua/5.4/?/init.lua"
)

assert(
    package.cpath ==
        private_prefix .. "/lib/lua/5.4/?.so"
)

assert(not package.path:find(".luarocks", 1, true))
assert(not package.cpath:find(".luarocks", 1, true))
assert(not package.path:find(";;", 1, true))
assert(not package.cpath:find(";;", 1, true))

local lfs = require("lfs")
local path = require("pl.path")
local List = require("pl.List")
local ldoc_tools = require("ldoc.tools")
local ldoc_markup = require("ldoc.markup")

assert(type(lfs.currentdir()) == "string")
assert(path.basename("/tmp/example.lua") == "example.lua")
assert(List({1, 2, 3}):join(",") == "1,2,3")
assert(type(ldoc_tools) == "table")
assert(type(ldoc_markup) == "table")

local lfs_location =
    assert(package.searchpath("lfs", package.cpath))

assert(
    lfs_location:sub(1, #private_prefix) == private_prefix,
    "lfs resolved outside the private prefix: " .. lfs_location
)

print("Private runtime and module isolation: OK")
