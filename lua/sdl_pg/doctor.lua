--- Offline environment inspection.
-- @module sdl_pg.doctor

local fs = require("sdl_pg.fs")
local path = require("sdl_pg.path")

local doctor = {}

local function find_on_path(name, search_path)
    for directory in tostring(search_path or ""):gmatch("[^:]+") do
        local candidate = path.join(directory, name)
        local mode = fs.mode(candidate)

        if mode == "file" or mode == "link" then
            return candidate
        end
    end

    return nil
end

--- Inspect the local generator environment without executing tools.
-- @param settings Resolved generator configuration.
-- @param environment Environment table.
-- @return Result records.
-- @return Boolean indicating whether required items were found.
function doctor.inspect(settings, environment)
    local records = {}
    local healthy = true

    local function add(label, value, required, ok)
        records[#records + 1] = {
            label = label,
            value = value,
            required = required,
            ok = ok
        }

        if required and not ok then
            healthy = false
        end
    end

    add(
        "sandbox root",
        settings.sandbox_root,
        true,
        fs.mode(settings.sandbox_root) == "directory"
    )
    add(
        "projects root",
        settings.projects_root,
        true,
        fs.mode(settings.projects_root) == "directory"
    )

    for _, command in ipairs({
        {"lua5.4", true},
        {"clang", true},
        {"cmake", true},
        {"ninja", true},
        {"ctest", true},
        {"git", false},
        {"gh", false},
        {"java", true},
        {"gradle", false}
    }) do
        local location = find_on_path(command[1], environment.PATH)
        add(
            command[1],
            location or "not found",
            command[2],
            location ~= nil
        )
    end

    return records, healthy
end

return doctor
