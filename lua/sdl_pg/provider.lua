--- Generator-only template provider selection.
-- @module sdl_pg.provider

local package_registry = require("sdl_pg.package_registry")
local template_selection = require("sdl_pg.template_selection")

local provider = {}

-- Keep this map deliberately small and built in. Provider selection happens
-- once while generating a project; no dispatcher is copied into the result.
local adapters = {
    android_sdl2_lua = "sdl_pg.android"
}

--- Resolve a template and its provider before project staging begins.
-- @param settings Resolved generator configuration.
-- @param name Project name.
-- @param options Template generation options.
-- @return Provider resolution containing the adapter and package graph.
function provider.resolve(settings, name, options)
    options = options or {}
    local selected_id, selected_version =
        template_selection.current(settings)
    local template_id = options.template_id or selected_id
    local template_version = options.template_version or selected_version
    local template_record, resolved_modules =
        package_registry.require_template(
            settings,
            template_id,
            template_version
        )
    local profile = template_record.template.profile
    local adapter_module = adapters[profile]

    if not adapter_module then
        error(
            "unsupported template profile: " ..
            tostring(profile) ..
            " (" .. template_id .. "@" .. template_version .. ")",
            0
        )
    end

    local adapter = require(adapter_module)
    local dependencies = {}
    if adapter.resolve_dependencies then
        dependencies = adapter.resolve_dependencies(settings, name, options)
    end

    return {
        profile = profile,
        adapter = adapter,
        dependencies = dependencies,
        template_record = template_record,
        resolved_modules = resolved_modules
    }
end

--- Populate a staged project using a previously resolved provider.
-- @param settings Resolved generator configuration.
-- @param destination Empty staged project directory.
-- @param name Project name.
-- @param options Template generation options.
-- @param resolution Result returned by `resolve`.
function provider.populate(settings, destination, name, options, resolution)
    resolution.adapter.populate(
        settings,
        destination,
        name,
        options,
        resolution.template_record,
        resolution.resolved_modules,
        resolution.dependencies
    )
end

return provider
