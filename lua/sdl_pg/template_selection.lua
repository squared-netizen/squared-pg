--- Active project-template selection.
-- @module sdl_pg.template_selection

local package_registry = require("sdl_pg.package_registry")
local state = require("sdl_pg.state")

local selection = {}

--- Return the selected template coordinate or the built-in default.
-- @param settings Resolved generator configuration.
-- @return Template ID, version, and selection source.
function selection.current(settings)
    local values = state.load(settings)
    local defaults = package_registry.defaults.template
    if values.template_id and values.template_version then
        return values.template_id, values.template_version, "selected"
    end
    return defaults.id, defaults.version, "default"
end

--- Select a registered, fully resolvable template.
-- @param settings Resolved generator configuration.
-- @param identifier Template package ID.
-- @param version Exact template version.
-- @return Template record and ordered dependencies.
function selection.use(settings, identifier, version)
    local record, dependencies = package_registry.require_template(
        settings,
        identifier,
        version
    )
    local values = state.load(settings)
    values.template_id = identifier
    values.template_version = version
    state.save(settings, values)
    return record, dependencies
end

--- Inspect the active template without mutating state.
-- @param settings Resolved generator configuration.
-- @return Status record.
function selection.status(settings)
    local identifier, version, source = selection.current(settings)
    local result = {
        id = identifier,
        version = version,
        source = source,
        coordinate = identifier .. "@" .. version,
        registered = package_registry.find(settings, identifier, version) ~= nil
    }
    local ok, record_or_error, dependencies = pcall(
        package_registry.require_template,
        settings,
        identifier,
        version
    )
    result.resolvable = ok
    if ok then
        result.record = record_or_error
        result.dependency_count = #dependencies
    else
        result.error = tostring(record_or_error):gsub("[\r\n]+", " ")
        result.dependency_count = 0
    end
    return result
end

return selection
