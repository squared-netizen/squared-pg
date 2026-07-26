--- Build and verify the private Lua documentation toolchain.
-- @script toolchain

if _VERSION ~= "Lua 5.4" then
    io.stderr:write(
        "ERROR: toolchain.lua requires Lua 5.4; found ",
        tostring(_VERSION),
        "\n"
    )
    os.exit(1)
end

local script_path = arg[0] or "toolchain.lua"
local root = script_path:match("^(.*)/[^/]+$") or "."

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

local dependencies = {
    {
        name = "Lua 5.4.8",
        archive = "lua-5.4.8.tar.gz",
        marker = "lua-5.4.8/src/lua.h",
        sha256 =
            "4f18ddae154e793e46eeab727c59ef1c0" ..
            "c0c2b744e7b94219710d76f530629ae"
    },
    {
        name = "LuaFileSystem 1.8.0",
        archive = "luafilesystem-v1_8_0.tar.gz",
        marker = "luafilesystem-1_8_0/src/lfs.c",
        sha256 =
            "16d17c788b8093f2047325343f5e9b74" ..
            "cccb1ea96001e45914a58bbae8932495"
    },
    {
        name = "Penlight 1.14.0",
        archive = "penlight-1.14.0.tar.gz",
        marker = "Penlight-1.14.0/lua/pl/init.lua",
        sha256 =
            "2387431c0e83c4189cccb35b989141a3" ..
            "280d735cb5d42bacf3451af9869bebf7"
    },
    {
        name = "LDoc 1.5.0",
        archive = "ldoc-1.5.0.tar.gz",
        marker = "ldoc-1.5.0/ldoc.lua",
        sha256 =
            "4469cd74c8c7f51d3b9ce802d2239ba2" ..
            "b09d3d3a11273c3a5abdf273a0a53531"
    }
}

for _, dependency in ipairs(dependencies) do
    local archive =
        root .. "/third_party/cache/" .. dependency.archive
    local marker =
        root .. "/third_party/" .. dependency.marker
    local actual_sha256 = sha256.file(archive)

    if actual_sha256 ~= dependency.sha256 then
        error(
            string.format(
                "%s checksum mismatch\nexpected: %s\nactual:   %s",
                dependency.name,
                dependency.sha256,
                actual_sha256
            ),
            0
        )
    end

    print(dependency.name .. " source archive checksum: OK")

    if not exists(marker) then
        run({
            "cmake",
            "-E",
            "chdir",
            root .. "/third_party",
            "cmake",
            "-E",
            "tar",
            "xzf",
            "cache/" .. dependency.archive
        })
    end

    if not exists(marker) then
        error(
            dependency.name ..
            " extraction did not produce " ..
            marker,
            0
        )
    end
end

local private_prefix = root .. "/build/private-lua"
local private_lua = private_prefix .. "/bin/lua-5.4.8"

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

run({
    "cmake",
    "--install",
    root .. "/build",
    "--prefix",
    private_prefix
})

if not exists(private_lua) then
    error(
        "private Lua installation did not produce " .. private_lua,
        0
    )
end

run({
    private_lua,
    root .. "/tools/private-run.lua",
    root .. "/tests/private-environment-test.lua",
    root
})

run({
    private_lua,
    root .. "/tools/private-run.lua",
    root .. "/tests/run.lua",
    root
})

local documentation_output =
    root .. "/build/generated-ldoc"

run({
    private_lua,
    root .. "/tools/private-run.lua",
    private_prefix .. "/share/lua/5.4/ldoc.lua",
    "-c",
    root .. "/tests/documentation/config.ld",
    "-d",
    documentation_output,
    root .. "/tests/documentation/sample.lua"
})

run({
    private_lua,
    root .. "/tools/private-run.lua",
    root .. "/tests/documentation-output-test.lua",
    documentation_output
})

local generator_documentation_output =
    root .. "/build/generated-sdl-pg-ldoc"

run({
    private_lua,
    root .. "/tools/private-run.lua",
    private_prefix .. "/share/lua/5.4/ldoc.lua",
    "-c",
    root .. "/docs/config.ld",
    "-d",
    generator_documentation_output,
    root .. "/lua/sdl_pg"
})

run({
    private_lua,
    root .. "/tools/private-run.lua",
    root .. "/tests/generator-documentation-output-test.lua",
    generator_documentation_output
})

local runtime_documentation_output =
    root .. "/build/generated-runtime-ldoc"

run({
    private_lua,
    root .. "/tools/private-run.lua",
    private_prefix .. "/share/lua/5.4/ldoc.lua",
    "-c",
    root .. "/docs/runtime-config.ld",
    "-d",
    runtime_documentation_output,
    root .. "/templates/android/app/src/main/assets/lua"
})

run({
    private_lua,
    root .. "/tools/private-run.lua",
    root .. "/tests/runtime-documentation-output-test.lua",
    runtime_documentation_output
})

print("")
print("Private Lua runtime: OK")
print("LuaFileSystem native module: OK")
print("Private package.path: OK")
print("Private package.cpath: OK")
print("Penlight private modules: OK")
print("LDoc generation with built-in Markdown: OK")
print("SDL Project Generator LDoc API: OK")
print("Generated application runtime LDoc API: OK")
print("Global LuaRocks isolation: OK")
print("SDL Project Generator tests: OK")
print("Offline toolchain test: OK")
