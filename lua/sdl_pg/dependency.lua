--- Generator-only external dependency provider dispatch.
-- @module sdl_pg.dependency

local dependency = {}

-- IDs are stable generator coordinates. Implementations remain built in and
-- are never copied into generated applications.
local providers = {
    ["android-sdl2"] = {
        label = "Android SDL2",
        module = "sdl_pg.kit"
    }
}

local ordered_ids = {
    "android-sdl2"
}

local function require_provider(identifier)
    local definition = providers[identifier]
    if not definition then
        error("unknown dependency provider: " .. tostring(identifier), 0)
    end
    return definition, require(definition.module)
end

--- Register an archive through a dependency-specific provider.
-- @param settings Resolved generator configuration.
-- @param identifier Stable dependency provider ID.
-- @param archive Local dependency archive.
-- @return Registration record.
function dependency.add(settings, identifier, archive)
    local definition, adapter = require_provider(identifier)
    return {
        id = identifier,
        label = definition.label,
        root = adapter.add(settings, archive)
    }
end

--- Require one configured dependency before project staging.
-- @param settings Resolved generator configuration.
-- @param identifier Stable dependency provider ID.
-- @return Resolved dependency record.
function dependency.require(settings, identifier)
    local definition, adapter = require_provider(identifier)
    local root, values = adapter.require_active(settings)
    return {
        id = identifier,
        label = definition.label,
        root = root,
        archive = values.kit_archive,
        sha256 = values.kit_sha256
    }
end

--- Read one dependency-provider status.
-- @param settings Resolved generator configuration.
-- @param identifier Stable dependency provider ID.
-- @return Status record.
function dependency.status(settings, identifier)
    local definition, adapter = require_provider(identifier)
    local status = adapter.status(settings)
    status.id = identifier
    status.label = definition.label
    return status
end

--- List all built-in dependency-provider statuses in stable order.
-- @param settings Resolved generator configuration.
-- @return Array of status records.
function dependency.list(settings)
    local result = {}
    for _, identifier in ipairs(ordered_ids) do
        result[#result + 1] = dependency.status(settings, identifier)
    end
    return result
end

return dependency
