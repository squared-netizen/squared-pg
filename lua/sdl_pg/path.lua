--- Safe path helpers for Squared Project Generator.
-- @module sdl_pg.path

local path = {}

--- Join path components with one slash.
-- @param ... Path components.
-- @return Joined path.
function path.join(...)
    local parts = {...}
    local components = {}
    local absolute = false

    for index, part in ipairs(parts) do
        part = tostring(part or "")

        if part ~= "" then
            if index == 1 and part:sub(1, 1) == "/" then
                absolute = true
            end

            for component in part:gmatch("[^/]+") do
                components[#components + 1] = component
            end
        end
    end

    local result = table.concat(components, "/")

    if absolute then
        return "/" .. result
    end

    return result ~= "" and result or "."
end

--- Return the parent directory of a path.
-- @param value Input path.
-- @return Parent path.
function path.dirname(value)
    local normalized = tostring(value):gsub("/+$", "")
    local parent = normalized:match("^(.*)/[^/]+$")

    if not parent or parent == "" then
        return "."
    end

    return parent
end

--- Return the final component of a path.
-- @param value Input path.
-- @return Basename.
function path.basename(value)
    local normalized = tostring(value):gsub("/+$", "")
    return normalized:match("([^/]+)$") or normalized
end

--- Validate a project directory name.
-- @param name Candidate project name.
-- @return true on success.
-- @raise When the name is unsafe or unsupported.
function path.validate_project_name(name)
    if type(name) ~= "string" or name == "" then
        error("project name is required", 0)
    end

    if name == "." or name == ".." or name:find("..", 1, true) then
        error("project name cannot contain '..'", 0)
    end

    if not name:match("^[A-Za-z][A-Za-z0-9_-]*$") then
        error(
            "project name must start with a letter and contain only " ..
            "letters, numbers, '_' or '-'",
            0
        )
    end

    return true
end

--- Convert a project name to a C/C++ identifier.
-- @param name Valid project name.
-- @return Identifier.
function path.identifier(name)
    path.validate_project_name(name)
    return name:gsub("-", "_"):lower()
end

return path
