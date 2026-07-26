--- Run one Lua script with only the private module trees enabled.
-- @script private-run

local script_path = arg[0] or "tools/private-run.lua"
local root = script_path:match("^(.*)/tools/[^/]+$") or "."
local private_prefix = root .. "/build/private-lua"

local target = arg[1]

if not target then
    io.stderr:write("Usage: private-run.lua TARGET.lua [ARGUMENTS...]\n")
    os.exit(2)
end

package.path = table.concat({
    private_prefix .. "/share/lua/5.4/?.lua",
    private_prefix .. "/share/lua/5.4/?/init.lua"
}, ";")

package.cpath =
    private_prefix .. "/lib/lua/5.4/?.so"

for index = 1, #arg do
    arg[index - 1] = arg[index]
end

arg[#arg] = nil
arg[0] = target

local chunk, load_error = loadfile(target)

if not chunk then
    io.stderr:write(
        "ERROR: cannot load ",
        target,
        ": ",
        tostring(load_error),
        "\n"
    )
    os.exit(1)
end

local succeeded, result = xpcall(chunk, debug.traceback)

if not succeeded then
    io.stderr:write("ERROR: ", tostring(result), "\n")
    os.exit(1)
end

return result
