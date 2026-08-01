--- Bounded human and agent diagnostics.
-- @module sdl_pg.report

local lfs = require("lfs")
local dependency = require("sdl_pg.dependency")
local doctor = require("sdl_pg.doctor")
local package_registry = require("sdl_pg.package_registry")
local project = require("sdl_pg.project")
local template_selection = require("sdl_pg.template_selection")
local release = require("sdl_pg.version")
local wrapper = require("sdl_pg.wrapper")

local report = {}

local function clean(value)
    return tostring(value == nil and "none" or value):gsub("[\r\n]+", " ")
end

--- Collect a side-effect-free generator status snapshot.
function report.collect(settings, environment)
    environment = environment or {}
    local tools, tools_healthy = doctor.inspect(settings, environment)
    local packages = package_registry.list(settings)
    local dependencies = dependency.list(settings)
    local template = template_selection.status(settings)
    local wrapper_status = wrapper.status(settings)
    local cwd = environment.PWD or lfs.currentdir()
    local project_root = project.find_root(cwd)
    local issues = {}
    for _, tool in ipairs(tools) do
        if tool.required and not tool.ok then
            issues[#issues + 1] = "missing required environment item: " ..
                tool.label
        end
    end
    for _, item in ipairs(dependencies) do
        if not item.configured then
            issues[#issues + 1] = "dependency not configured: " .. item.id
        elseif item.valid == false then
            issues[#issues + 1] = "dependency registration is invalid: " ..
                item.id
        end
    end
    if not wrapper_status.configured then
        issues[#issues + 1] = "Gradle Wrapper is not configured"
    elseif wrapper_status.valid == false then
        issues[#issues + 1] = "Gradle Wrapper registration is invalid"
    end
    if not template.resolvable then
        issues[#issues + 1] = "active template is not resolvable"
    end
    return {
        version = release.version,
        private_lua = release.private_lua,
        settings = settings,
        tools = tools,
        tools_healthy = tools_healthy,
        dependencies = dependencies,
        wrapper = wrapper_status,
        packages = packages,
        template = template,
        project_root = project_root,
        issues = issues,
        healthy = #issues == 0
    }
end

--- Render complete human-readable diagnostic lines.
function report.verbose_lines(value)
    local lines = {
        "Squared Project Generator " .. value.version,
        "Private Lua: " .. value.private_lua,
        "Status: " .. (value.healthy and "OK" or "ATTENTION"),
        "Generator root: " .. clean(value.settings.generator_root),
        "Configuration: " .. clean(value.settings.config_file),
        "Cache root: " .. clean(value.settings.cache_root),
        "Sandbox root: " .. clean(value.settings.sandbox_root),
        "Projects root: " .. clean(value.settings.projects_root),
        "Current project: " .. clean(value.project_root),
        "",
        "Tools:"
    }
    for _, item in ipairs(value.tools) do
        lines[#lines + 1] = string.format(
            "  %-9s %-16s %s",
            item.ok and "OK" or (item.required and "MISSING" or "OPTIONAL"),
            item.label,
            clean(item.value)
        )
    end
    lines[#lines + 1] = ""
    lines[#lines + 1] = "Dependencies:"
    for _, item in ipairs(value.dependencies) do
        lines[#lines + 1] = "  " .. item.id .. ": " ..
            (item.configured and item.valid and
                ("configured (" .. clean(item.root) .. ")") or
                (item.configured and
                    ("invalid (" .. clean(item.root) .. ")") or
                    "not configured"))
    end
    lines[#lines + 1] = "Gradle Wrapper: " ..
        (value.wrapper.configured and value.wrapper.valid and
            clean(value.wrapper.root) or
            (value.wrapper.configured and "invalid" or "not configured"))
    lines[#lines + 1] = "Active template: " .. value.template.coordinate ..
        " (" .. value.template.source .. ", " ..
        (value.template.resolvable and "resolvable" or "unresolved") .. ")"
    lines[#lines + 1] = ""
    lines[#lines + 1] = "Registered packages (" .. #value.packages .. "):"
    for _, item in ipairs(value.packages) do
        lines[#lines + 1] = string.format(
            "  %-9s %s@%s  %s",
            item.kind,
            item.id,
            item.version,
            item.content_digest
        )
    end
    lines[#lines + 1] = ""
    lines[#lines + 1] = "Issues (" .. #value.issues .. "):"
    if #value.issues == 0 then
        lines[#lines + 1] = "  none"
    else
        for _, item in ipairs(value.issues) do
            lines[#lines + 1] = "  " .. item
        end
    end
    return lines
end

local function next_action(value)
    for _, item in ipairs(value.dependencies) do
        if not item.configured or item.valid == false then
            return "squared-pg dependency add " .. item.id .. " ARCHIVE"
        end
    end
    if not value.wrapper.configured or value.wrapper.valid == false then
        return "squared-pg wrapper add PROJECT_DIRECTORY"
    end
    if not value.template.resolvable then
        return "squared-pg template status"
    end
    return "squared-pg self-test"
end

--- Render a deterministic LLM-focused report capped at 32 lines and 4 KiB.
function report.agent_lines(value)
    local candidates = {
        "SQUARED_PG_AGENT_FEEDBACK_V1",
        "version=" .. clean(value.version),
        "status=" .. (value.healthy and "ok" or "attention"),
        "generator_root=" .. clean(value.settings.generator_root),
        "config_file=" .. clean(value.settings.config_file),
        "cache_root=" .. clean(value.settings.cache_root),
        "project_root=" .. clean(value.project_root),
        "wrapper=" .. (value.wrapper.configured and value.wrapper.valid and
            "configured" or
            (value.wrapper.configured and "invalid" or "missing")),
        "package_count=" .. tostring(#value.packages),
        "template=" .. clean(value.template.coordinate),
        "template_source=" .. clean(value.template.source),
        "template_resolvable=" .. tostring(value.template.resolvable)
    }
    for _, item in ipairs(value.dependencies) do
        candidates[#candidates + 1] = "dependency." .. clean(item.id) .. "=" ..
            (item.configured and item.valid and "configured" or
                (item.configured and "invalid" or "missing"))
    end
    for index, item in ipairs(value.issues) do
        candidates[#candidates + 1] = "issue." .. index .. "=" .. clean(item)
    end
    candidates[#candidates + 1] = "next_action=" .. clean(next_action(value))

    local lines, bytes, truncated = {}, 0, false
    for _, line in ipairs(candidates) do
        local additional = #line + (#lines > 0 and 1 or 0)
        if #lines >= 31 or bytes + additional + 16 > 4096 then
            truncated = true
            break
        end
        lines[#lines + 1] = line
        bytes = bytes + additional
    end
    lines[#lines + 1] = "truncated=" .. tostring(truncated)
    return lines
end

return report
