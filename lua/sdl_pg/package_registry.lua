--- Validated local SQ package registry and project composition.
-- @module sdl_pg.package_registry

local fs = require("sdl_pg.fs")
local path = require("sdl_pg.path")
local sq = require("squared.sq")

local registry = {}
local temporary_counter = 0

--- Built-in module coordinates and the current default template.
registry.defaults = {
    application = {
        id = "dev.squarednetizen.squared.application",
        version = "0.6.0-dev.1",
        archive = "squared-application-0.6.0-dev.1.sq"
    },
    graphics = {
        id = "dev.squarednetizen.squared.graphics",
        version = "0.6.0-dev.1",
        archive = "squared-graphics-0.6.0-dev.1.sq"
    },
    graphics2d = {
        id = "dev.squarednetizen.squared.graphics2d",
        version = "0.6.0-dev.1",
        archive = "squared-graphics2d-0.6.0-dev.1.sq"
    },
    math = {
        id = "dev.squarednetizen.squared.math",
        version = "0.6.0-dev.1",
        archive = "squared-math-0.6.0-dev.1.sq"
    },
    scene2d = {
        id = "dev.squarednetizen.squared.scene2d",
        version = "0.6.0-dev.1",
        archive = "squared-scene2d-0.6.0-dev.1.sq"
    },
    time = {
        id = "dev.squarednetizen.squared.time",
        version = "0.6.0-dev.1",
        archive = "squared-time-0.6.0-dev.1.sq"
    },
    data = {
        id = "dev.squarednetizen.squared.data",
        version = "0.6.0-dev.1",
        archive = "squared-data-0.6.0-dev.1.sq"
    },
    messaging = {
        id = "dev.squarednetizen.squared.messaging",
        version = "0.6.0-dev.1",
        archive = "squared-messaging-0.6.0-dev.1.sq"
    },
    template = {
        id = "dev.squarednetizen.template.android-sdl2-lua",
        version = "0.6.0-dev.14",
        archive = "squared-android-template-0.6.0-dev.14.sq"
    }
}

-- Compatibility for private 0.6 development scripts. New code uses the
-- frontend-neutral `template` coordinate.
registry.defaults.android_template = registry.defaults.template

local function fail(error_value)
    if type(error_value) == "table" then
        error(
            string.format(
                "%s [%s/%s]",
                tostring(error_value.message),
                tostring(error_value.phase),
                tostring(error_value.code)
            ),
            0
        )
    end
    error(tostring(error_value), 0)
end

local function package_root(settings, identifier, version)
    return path.join(
        settings.cache_root,
        "packages",
        identifier,
        version
    )
end

local function temporary_sibling(destination)
    repeat
        temporary_counter = temporary_counter + 1
        local candidate =
            destination ..
            ".squared-pg-import-" ..
            tostring(os.time()) ..
            "-" ..
            tostring(temporary_counter)
        if not fs.exists(candidate) then
            return candidate
        end
    until false
end

local function load_record(settings, identifier, version)
    local root = package_root(settings, identifier, version)
    local filename = path.join(root, "record.lua")
    if fs.mode(filename) ~= "file" then
        return nil
    end
    local chunk, message = loadfile(filename, "t", {})
    if not chunk then
        error("cannot load package record: " .. tostring(message), 0)
    end
    local ok, record = pcall(chunk)
    if not ok or type(record) ~= "table" then
        error("invalid package record: " .. tostring(record), 0)
    end
    record.root = root
    record.content_root = path.join(root, "content")
    record.archive = path.join(root, "package.sq")
    return record
end

local function append_template_record(lines, project_template)
    if not project_template then
        return
    end
    lines[#lines + 1] = "    template = {"
    lines[#lines + 1] =
        "        profile = " ..
        string.format("%q", project_template.profile) ..
        ","
    lines[#lines + 1] =
        "        directory = " ..
        string.format("%q", project_template.directory) ..
        ","
    lines[#lines + 1] = "        executable_hooks = false,"
    lines[#lines + 1] = "        fields = {"
    for _, field in ipairs(project_template.fields or {}) do
        lines[#lines + 1] = "            {"
        lines[#lines + 1] =
            "                name = " ..
            string.format("%q", field.name) ..
            ","
        lines[#lines + 1] =
            "                variable = " ..
            string.format("%q", field.variable) ..
            ","
        lines[#lines + 1] =
            "                required = " ..
            tostring(field.required == true) ..
            ","
        lines[#lines + 1] =
            "                default = " ..
            string.format("%q", field.default or "") ..
            ","
        lines[#lines + 1] = "            },"
    end
    lines[#lines + 1] = "        },"
    lines[#lines + 1] = "        requires = {"
    for _, requirement in ipairs(project_template.requires or {}) do
        lines[#lines + 1] = "            {"
        lines[#lines + 1] =
            "                kind = " ..
            string.format("%q", requirement.kind) ..
            ","
        lines[#lines + 1] =
            "                id = " ..
            string.format("%q", requirement.id) ..
            ","
        lines[#lines + 1] =
            "                version = " ..
            string.format("%q", requirement.version) ..
            ","
        lines[#lines + 1] = "            },"
    end
    lines[#lines + 1] = "        },"
    lines[#lines + 1] = "    },"
end

local function append_module_record(lines, module)
    if not module then
        return
    end
    lines[#lines + 1] = "    module = {"
    lines[#lines + 1] =
        "        directory = " ..
        string.format("%q", module.directory) ..
        ","
    lines[#lines + 1] =
        "        cmake_target = " ..
        string.format("%q", module.cmake_target) ..
        ","
    lines[#lines + 1] = "        requires = {"
    for _, requirement in ipairs(module.requires or {}) do
        lines[#lines + 1] = "            {"
        lines[#lines + 1] =
            "                kind = " ..
            string.format("%q", requirement.kind) ..
            ","
        lines[#lines + 1] =
            "                id = " ..
            string.format("%q", requirement.id) ..
            ","
        lines[#lines + 1] =
            "                version = " ..
            string.format("%q", requirement.version) ..
            ","
        lines[#lines + 1] = "            },"
    end
    lines[#lines + 1] = "        },"
    lines[#lines + 1] = "    },"
end

--- Add and validate a backed-up SQ package in the private local registry.
-- The source archive is preserved and never modified.
-- @param settings Resolved generator configuration.
-- @param archive Existing .sq archive.
-- @return Installed package record.
function registry.add(settings, archive)
    if fs.mode(archive) ~= "file" then
        error("SQ package is not a regular file: " .. tostring(archive), 0)
    end

    local imports_root = path.join(settings.cache_root, "package-imports")
    fs.mkdir_p(imports_root)
    local temporary = temporary_sibling(
        path.join(imports_root, "package")
    )
    fs.mkdir_p(temporary)
    local copied_archive = path.join(temporary, "package.sq")

    local copied, copy_error = xpcall(function()
        fs.copy_file(archive, copied_archive)
    end, debug.traceback)
    if not copied then
        fs.remove_tree(temporary)
        error(copy_error, 0)
    end

    local package, open_error = sq.open(copied_archive)
    if not package then
        fs.remove_tree(temporary)
        fail(open_error)
    end

    local committed = false
    local ok, result = xpcall(function()
        local manifest = package:manifest()
        local validation, validation_error = package:validate({
            expected_kind = manifest.kind
        })
        if not validation then
            fail(validation_error)
        end
        if not validation.valid then
            error("SQ package validation reported one or more issues", 0)
        end

        local destination =
            package_root(settings, manifest.id, manifest.version)
        if fs.exists(destination) then
            local existing =
                load_record(settings, manifest.id, manifest.version)
            if existing and
                existing.content_digest == validation.content_digest then
                existing.already_registered = true
                return existing
            end
            error(
                "package ID/version conflicts with registered content: " ..
                manifest.id .. " " .. manifest.version,
                0
            )
        end
        fs.mkdir_p(path.dirname(destination))
        local extracted, extraction_error =
            package:extract_transactionally(
                path.join(temporary, "content"),
                {expected_kind = manifest.kind}
            )
        if not extracted then
            fail(extraction_error)
        end
        local record_lines = {
            "--- Managed SQ package registry record.",
            "return {",
            "    format = 1,",
            "    id = " .. string.format("%q", manifest.id) .. ",",
            "    version = " .. string.format("%q", manifest.version) .. ",",
            "    kind = " .. string.format("%q", manifest.kind) .. ",",
            "    name = " .. string.format("%q", manifest.name) .. ",",
            "    content_digest = " ..
                string.format("%q", validation.content_digest) .. ",",
        }
        append_module_record(record_lines, manifest.module)
        append_template_record(record_lines, manifest.template)
        record_lines[#record_lines + 1] = "}"
        record_lines[#record_lines + 1] = ""
        fs.write_file(
            path.join(temporary, "record.lua"),
            table.concat(record_lines, "\n")
        )
        local renamed, rename_error = os.rename(temporary, destination)
        if not renamed then
            error(
                "cannot commit package registration: " ..
                tostring(rename_error),
                0
            )
        end
        committed = true
        return assert(load_record(
            settings,
            manifest.id,
            manifest.version
        ))
    end, debug.traceback)

    package:close()
    if not committed and fs.exists(temporary) then
        fs.remove_tree(temporary)
    end
    if not ok then
        error(result, 0)
    end
    return result
end

local function requirements_for(record)
    if record.kind == "module" and record.module then
        return record.module.requires or {}
    end
    if record.kind == "template" and record.template then
        return record.template.requires or {}
    end
    return {}
end

local function sorted_requirements(requirements)
    local result = {}
    for index, requirement in ipairs(requirements or {}) do
        result[index] = requirement
    end
    table.sort(result, function(left, right)
        if left.id ~= right.id then
            return left.id < right.id
        end
        if left.version ~= right.version then
            return left.version < right.version
        end
        return left.kind < right.kind
    end)
    return result
end

local function coordinate(identifier, version)
    return identifier .. "@" .. version
end

--- Resolve all transitive exact dependencies for one registered package.
-- Dependencies are returned before their dependents in deterministic order.
-- The root package itself is not included.
-- @param settings Resolved generator configuration.
-- @param identifier Root package identifier.
-- @param version Exact root package version.
-- @return Root package record and ordered dependency records.
function registry.resolve(settings, identifier, version)
    local root = load_record(settings, identifier, version)
    if not root then
        error(
            "required SQ package is not registered: " ..
            identifier .. " " .. version,
            0
        )
    end

    local states = {}
    local selected = {
        [root.id] = {
            version = root.version,
            kind = root.kind
        }
    }
    local stack = {}
    local stack_positions = {}
    local ordered = {}
    local node_count = 0

    local function visit(requirement, parent)
        local selected_package = selected[requirement.id]
        if selected_package then
            if selected_package.version ~= requirement.version then
                error(
                    "SQ dependency version conflict for " ..
                    requirement.id .. ": " ..
                    selected_package.version .. " and " ..
                    requirement.version,
                    0
                )
            end
            if selected_package.kind ~= requirement.kind then
                error(
                    "SQ dependency kind conflict for " ..
                    requirement.id,
                    0
                )
            end
        else
            selected[requirement.id] = {
                version = requirement.version,
                kind = requirement.kind
            }
        end

        local key = coordinate(requirement.id, requirement.version)
        if states[key] == "done" then
            return
        end
        if states[key] == "visiting" then
            local cycle = {}
            local start = assert(stack_positions[key])
            for index = start, #stack do
                cycle[#cycle + 1] = stack[index]
            end
            cycle[#cycle + 1] = key
            error(
                "SQ dependency cycle: " ..
                table.concat(cycle, " -> "),
                0
            )
        end

        local dependency =
            load_record(settings, requirement.id, requirement.version)
        if not dependency then
            error(
                "SQ dependency is not registered: " ..
                requirement.kind .. " " ..
                key .. " (required by " .. parent .. ")",
                0
            )
        end
        if dependency.kind ~= requirement.kind then
            error(
                "SQ dependency kind mismatch for " .. key ..
                ": expected " .. requirement.kind ..
                ", registered " .. dependency.kind,
                0
            )
        end

        node_count = node_count + 1
        if node_count > 256 then
            error("SQ dependency graph exceeds 256 packages", 0)
        end
        states[key] = "visiting"
        stack[#stack + 1] = key
        stack_positions[key] = #stack
        for _, child in ipairs(
                sorted_requirements(requirements_for(dependency))
            ) do
            visit(child, key)
        end
        stack_positions[key] = nil
        stack[#stack] = nil
        states[key] = "done"
        ordered[#ordered + 1] = dependency
    end

    local root_key = coordinate(root.id, root.version)
    states[root_key] = "visiting"
    stack[1] = root_key
    stack_positions[root_key] = 1
    for _, requirement in ipairs(
            sorted_requirements(requirements_for(root))
        ) do
        visit(requirement, root_key)
    end
    stack_positions[root_key] = nil
    stack[1] = nil
    states[root_key] = "done"
    return root, ordered
end

--- Resolve a registered template and all exact module requirements.
-- This performs no filesystem mutation.
-- @param settings Resolved generator configuration.
-- @param identifier Template package identifier.
-- @param version Exact template package version.
-- @return Template package record.
function registry.require_template(settings, identifier, version)
    local record, dependencies =
        registry.resolve(settings, identifier, version)
    if record.kind ~= "template" or not record.template then
        error("registered SQ package is not a template: " .. identifier, 0)
    end
    if record.template.executable_hooks then
        error("template executable hooks are disabled", 0)
    end
    return record, dependencies
end

--- Return one exact registered package version or nil.
-- @param settings Resolved generator configuration.
-- @param identifier Permanent package identifier.
-- @param version Exact package version.
function registry.find(settings, identifier, version)
    return load_record(settings, identifier, version)
end

--- Return registered package records in stable identifier/version order.
-- @param settings Resolved generator configuration.
-- @return Array of records.
function registry.list(settings)
    local records = {}
    local packages_root = path.join(settings.cache_root, "packages")
    if fs.mode(packages_root) ~= "directory" then
        return records
    end
    for _, identifier in ipairs(fs.entries(packages_root)) do
        local identifier_root = path.join(packages_root, identifier)
        if fs.mode(identifier_root) == "directory" then
            for _, version in ipairs(fs.entries(identifier_root)) do
                local record = load_record(settings, identifier, version)
                if record then
                    records[#records + 1] = record
                end
            end
        end
    end
    return records
end

--- Apply a registered module to a staged project without overwriting files.
-- @param settings Resolved generator configuration.
-- @param destination Staged project root.
-- @param identifier Module package identifier.
-- @param version Exact module package version.
-- @return Package record.
function registry.apply_module(
    settings,
    destination,
    identifier,
    version
)
    local record = load_record(settings, identifier, version)
    if not record then
        error(
            "required SQ module is not registered: " ..
            identifier .. " " .. version,
            0
        )
    end
    if record.kind ~= "module" then
        error("registered SQ package is not a module: " .. identifier, 0)
    end
    fs.merge_tree(record.content_root, destination)
    return record
end

return registry
