--- Explicit external-process adapter.
-- @module sdl_pg.process

local process = {}

local function shell_quote(value)
    return "'" .. tostring(value):gsub("'", "'\\''") .. "'"
end

--- Run an external tool with separately quoted arguments.
-- This adapter is reserved for compilers, CMake, Java, and archive extraction.
-- Filesystem copying remains native Lua code.
-- @param arguments Array containing executable and arguments.
-- @param[opt] working_directory Directory in which the command should run.
function process.run(arguments, working_directory)
    assert(type(arguments) == "table", "arguments must be a table")
    assert(#arguments > 0, "an executable is required")

    local command = {}

    if working_directory then
        command[#command + 1] = shell_quote("cmake")
        command[#command + 1] = shell_quote("-E")
        command[#command + 1] = shell_quote("chdir")
        command[#command + 1] = shell_quote(working_directory)
    end

    for _, argument in ipairs(arguments) do
        command[#command + 1] = shell_quote(argument)
    end

    local rendered = table.concat(command, " ")
    io.stdout:write("+ ", rendered, "\n")

    local succeeded, reason, code = os.execute(rendered)
    if not succeeded then
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

return process
