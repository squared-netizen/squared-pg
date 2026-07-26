--- Persistent private-cache state.
-- @module sdl_pg.state

local fs = require("sdl_pg.fs")
local path = require("sdl_pg.path")

local state = {}

local function quote(value)
    if value == nil then
        return "nil"
    end

    return string.format("%q", value)
end

--- Load generator cache state.
-- @param settings Resolved configuration.
-- @return State table.
function state.load(settings)
    local filename = path.join(settings.cache_root, "state.lua")

    if not fs.exists(filename) then
        return {}
    end

    local chunk, message = loadfile(filename, "t", {})
    if not chunk then
        error("cannot load state: " .. tostring(message), 0)
    end

    local ok, result = pcall(chunk)
    if not ok or type(result) ~= "table" then
        error("invalid generator state: " .. tostring(result), 0)
    end

    return result
end

--- Save generator cache state.
-- @param settings Resolved configuration.
-- @param values State table.
function state.save(settings, values)
    fs.mkdir_p(settings.cache_root)
    local filename = path.join(settings.cache_root, "state.lua")
    local temporary = filename .. ".new"

    fs.write_file(temporary, table.concat({
        "--- Managed by sdl-pg. Do not edit while a command is running.",
        "",
        "return {",
        "    kit_root = " .. quote(values.kit_root) .. ",",
        "    kit_archive = " .. quote(values.kit_archive) .. ",",
        "    kit_sha256 = " .. quote(values.kit_sha256) .. ",",
        "    wrapper_root = " .. quote(values.wrapper_root),
        "}",
        ""
    }, "\n"))

    local renamed, message = os.rename(temporary, filename)
    if not renamed then
        os.remove(temporary)
        error("cannot save generator state: " .. tostring(message), 0)
    end
end

return state
