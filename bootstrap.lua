--- Configure, build, and test the pinned Lua embedding smoke project.
-- @script bootstrap

if _VERSION ~= "Lua 5.4" then
    io.stderr:write(
        "ERROR: bootstrap.lua requires Lua 5.4; found ",
        tostring(_VERSION),
        "\n"
    )
    os.exit(1)
end

local script_path = arg[0] or "bootstrap.lua"
local root = script_path:match("^(.*)/[^/]+$") or "."

local archive =
    root .. "/third_party/cache/lua-5.4.8.tar.gz"
local lua_header =
    root .. "/third_party/lua-5.4.8/src/lua.h"
local expected_sha256 =
    "4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae"

local function shell_quote(value)
    return "'" .. tostring(value):gsub("'", "'\\''") .. "'"
end

local function exists(path)
    local file = io.open(path, "rb")

    if file then
        file:close()
        return true
    end

    return false
end

local function run(arguments)
    local command = {}

    for index, argument in ipairs(arguments) do
        command[index] = shell_quote(argument)
    end

    io.stdout:write("+ ", table.concat(command, " "), "\n")

    local succeeded, reason, code = os.execute(table.concat(command, " "))

    if not succeeded then
        error(
            string.format(
                "command failed (%s %s): %s",
                tostring(reason),
                tostring(code),
                table.concat(command, " ")
            ),
            0
        )
    end
end

local sha256 = dofile(root .. "/tools/sha256.lua")
local actual_sha256 = sha256.file(archive)

if actual_sha256 ~= expected_sha256 then
    error(
        string.format(
            "Lua archive checksum mismatch\nexpected: %s\nactual:   %s",
            expected_sha256,
            actual_sha256
        ),
        0
    )
end

print("Lua 5.4.8 source archive checksum: OK")

if not exists(lua_header) then
    run({
        "cmake",
        "-E",
        "chdir",
        root .. "/third_party",
        "cmake",
        "-E",
        "tar",
        "xzf",
        "cache/lua-5.4.8.tar.gz"
    })
end

if not exists(lua_header) then
    error("Lua source extraction did not produce " .. lua_header, 0)
end

run({
    "cmake",
    "-S",
    root,
    "-B",
    root .. "/build",
    "-G",
    "Ninja",
    "-DCMAKE_BUILD_TYPE=Debug"
})

run({
    "cmake",
    "--build",
    root .. "/build",
    "--parallel",
    "2"
})

run({
    "ctest",
    "--test-dir",
    root .. "/build",
    "--output-on-failure"
})

print("")
print("Lua 5.4.8 embedding smoke test completed successfully.")
print("Private interpreter: " .. root .. "/build/bin/lua-5.4.8")
print("C++ host: " .. root .. "/build/bin/lua_embed_smoke")
