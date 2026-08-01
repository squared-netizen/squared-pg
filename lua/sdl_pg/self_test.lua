--- Installed-generator consistency checks.
-- @module sdl_pg.self_test

local fs = require("sdl_pg.fs")
local path = require("sdl_pg.path")
local report = require("sdl_pg.report")

local self_test = {}

--- Run bounded, non-mutating installed-generator checks.
function self_test.run(settings, environment)
    local checks = {}
    local function add(label, ok, detail)
        checks[#checks + 1] = {label = label, ok = ok, detail = detail}
    end
    add(
        "generator root",
        settings.generator_root ~= nil and
            fs.mode(settings.generator_root) == "directory",
        settings.generator_root or "not configured"
    )
    if settings.generator_root then
        add(
            "CLI module",
            fs.mode(path.join(settings.generator_root, "lua/sdl_pg/main.lua")) ==
                "file",
            path.join(settings.generator_root, "lua/sdl_pg/main.lua")
        )
    end
    local snapshot = report.collect(settings, environment)
    add("required tools", snapshot.tools_healthy, "offline PATH inspection")
    for _, item in ipairs(snapshot.dependencies) do
        add(
            "dependency " .. item.id,
            item.configured and item.valid,
            item.error or item.root or "missing"
        )
    end
    add(
        "Gradle Wrapper",
        snapshot.wrapper.configured and snapshot.wrapper.valid,
        snapshot.wrapper.error or snapshot.wrapper.root or "missing"
    )
    add(
        "active template",
        snapshot.template.resolvable,
        snapshot.template.error or snapshot.template.coordinate
    )
    local agent = report.agent_lines(snapshot)
    local rendered = table.concat(agent, "\n") .. "\n"
    add("agent feedback line bound", #agent <= 32, tostring(#agent) .. " lines")
    add("agent feedback byte bound", #rendered <= 4096, tostring(#rendered) .. " bytes")
    local healthy = true
    for _, check in ipairs(checks) do
        healthy = healthy and check.ok
    end
    return checks, healthy
end

return self_test
