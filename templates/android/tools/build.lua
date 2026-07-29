--- Offline-first Android build driver.
-- Invoke with the system Lua 5.4 interpreter.
-- @script build

if _VERSION ~= "Lua 5.4" then
    io.stderr:write("ERROR: tools/build.lua requires Lua 5.4\n")
    os.exit(1)
end

local function quote(value)
    return "'" .. tostring(value):gsub("'", "'\\''") .. "'"
end

local function run(arguments, working_directory)
    local command = {}
    for index, value in ipairs(arguments) do
        command[index] = quote(value)
    end

    if working_directory then
        command = {
            quote("cmake"),
            quote("-E"),
            quote("chdir"),
            quote(working_directory),
            table.concat(command, " ")
        }
    end

    local rendered = table.concat(command, " ")
    print("+ " .. rendered)
    local ok, reason, code = os.execute(rendered)
    if not ok then
        error(
            string.format(
                "command failed (%s %s): %s",
                tostring(reason),
                tostring(code),
                rendered
            ),
            0
        )
    end
end

local function parent(value)
    return value:match("^(.*)/[^/]+$") or "."
end

local function mkdir_p(value)
    local current = value:sub(1, 1) == "/" and "/" or ""
    for component in value:gmatch("[^/]+") do
        if component ~= "." then
            current =
                current == "/" and ("/" .. component) or
                (current == "" and component or current .. "/" .. component)
            local ok = os.rename(current, current)
            if not ok then
                local created, message = os.execute(
                    quote("cmake") .. " " ..
                    quote("-E") .. " " ..
                    quote("make_directory") .. " " ..
                    quote(current)
                )
                if not created then
                    error("cannot create " .. current .. ": " ..
                        tostring(message), 0)
                end
            end
        end
    end
end

local function copy_file(source, destination)
    mkdir_p(parent(destination))
    local input, message = io.open(source, "rb")
    if not input then
        error("cannot open " .. source .. ": " .. tostring(message), 0)
    end
    local output, output_message = io.open(destination, "wb")
    if not output then
        input:close()
        error("cannot write " .. destination .. ": " ..
            tostring(output_message), 0)
    end

    while true do
        local block = input:read(128 * 1024)
        if not block then break end
        assert(output:write(block))
    end
    input:close()
    output:close()
end

local function exists(filename)
    local file = io.open(filename, "rb")
    if file then
        file:close()
        return true
    end
    return false
end

local script = arg[0] or "tools/build.lua"
local tools_directory = parent(script)
local relative_root = parent(tools_directory)
local root

if script:sub(1, 1) == "/" then
    root = relative_root
else
    local working_directory = os.getenv("PWD")
    if not working_directory or working_directory == "" then
        error("PWD is not set", 0)
    end

    root =
        relative_root == "." and working_directory or
        working_directory .. "/" .. relative_root
end

local prefix = os.getenv("PREFIX")

if not prefix or prefix == "" then
    error("PREFIX is not set; this builder expects Termux", 0)
end

local clean = false
local offline = true
for index = 1, #arg do
    if arg[index] == "--clean" then
        clean = true
    elseif arg[index] == "--online-once" then
        offline = false
    else
        error("Usage: lua5.4 tools/build.lua [--clean] [--online-once]", 0)
    end
end

local source = root .. "/app/src/main/cpp"
local build = root .. "/native-build"
local output = root .. "/app/src/main/jniLibs/arm64-v8a"
local sdl = root .. "/third_party/SDL2"
local lua = root .. "/third_party/lua-5.4.8"
local yyjson = root .. "/third_party/yyjson-0.12.0"
local sdk = prefix .. "/opt/android-sdk"
local wrapper_jar = root .. "/gradle/wrapper/gradle-wrapper.jar"

for _, required in ipairs({
    sdl .. "/include/SDL2/SDL.h",
    sdl .. "/include/SDL2/SDL_opengles2_khrplatform.h",
    sdl .. "/include/SDL2/SDL_opengles2_gl2platform.h",
    sdl .. "/include/SDL2/SDL_opengles2_gl2.h",
    sdl .. "/lib/arm64-v8a/libSDL2.so",
    lua .. "/src/lua.h",
    yyjson .. "/src/yyjson.c",
    yyjson .. "/src/yyjson.h",
    wrapper_jar,
    prefix .. "/bin/clang",
    prefix .. "/bin/clang++",
    prefix .. "/lib/libc++_shared.so",
    "/system/lib64/libGLESv2.so"
}) do
    if not exists(required) then
        error("required build input is missing: " .. required, 0)
    end
end

if clean and os.rename(build, build) then
    local backup =
        build .. ".old-" .. os.date("!%Y%m%d-%H%M%S")
    assert(os.rename(build, backup))
    print("Previous native build moved to: " .. backup)
end

mkdir_p(build)
mkdir_p(output)

run({
    "cmake",
    "-S", source,
    "-B", build,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Debug",
    "-DCMAKE_C_COMPILER=" .. prefix .. "/bin/clang",
    "-DCMAKE_CXX_COMPILER=" .. prefix .. "/bin/clang++",
    "-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=" .. output,
    "-DSDL2_ROOT=" .. sdl,
    "-DLUA_ROOT=" .. lua,
    "-DYYJSON_ROOT=" .. yyjson,
    "-DPROJECT_ROOT=" .. root
})
run({"cmake", "--build", build, "--parallel", "2"})

for _, library in ipairs({
    "libSDL2.so",
    "libSDL2_ttf.so",
    "libSDL2_mixer.so",
    "libSDL2_image.so",
    "libSDL2_net.so"
}) do
    copy_file(
        sdl .. "/lib/arm64-v8a/" .. library,
        output .. "/" .. library
    )
end
copy_file(
    prefix .. "/lib/libc++_shared.so",
    output .. "/libc++_shared.so"
)

local properties = assert(io.open(root .. "/local.properties", "wb"))
assert(properties:write("sdk.dir=", sdk, "\n"))
assert(properties:close())

local gradle = {
    "java",
    "-classpath", wrapper_jar,
    "org.gradle.wrapper.GradleWrapperMain",
    "--no-daemon",
    "-PsdlBuildToolsVersion=34.0.4",
    "-Pandroid.aapt2FromMavenOverride=" ..
        sdk .. "/build-tools/34.0.4/aapt2"
}
if offline then
    gradle[#gradle + 1] = "--offline"
end
gradle[#gradle + 1] = "assembleDebug"
run(gradle, root)

local apk = root .. "/app/build/outputs/apk/debug/app-debug.apk"
if not exists(apk) then
    error("Gradle completed without producing " .. apk, 0)
end

local download = "/sdcard/Download/{{PROJECT_ID}}-debug.apk"
local download_test = io.open("/sdcard/Download/.sdl-pg-write-test", "wb")
if download_test then
    download_test:close()
    os.remove("/sdcard/Download/.sdl-pg-write-test")
    copy_file(apk, download)
    print("Build successful: " .. download)
else
    print("Build successful: " .. apk)
end
