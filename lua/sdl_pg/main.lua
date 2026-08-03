--- Command-line interface for Squared Project Generator.
-- @module sdl_pg.main

local config = require("sdl_pg.config")
local dependency = require("sdl_pg.dependency")
local doctor = require("sdl_pg.doctor")
local docs = require("sdl_pg.docs")
local package_builder = require("sdl_pg.package_builder")
local package_registry = require("sdl_pg.package_registry")
local package_sync = require("sdl_pg.package_sync")
local project = require("sdl_pg.project")
local project_module = require("sdl_pg.project_module")
local report = require("sdl_pg.report")
local self_test = require("sdl_pg.self_test")
local template_selection = require("sdl_pg.template_selection")
local release = require("sdl_pg.version")
local uninstall = require("sdl_pg.uninstall")
local wrapper = require("sdl_pg.wrapper")

local main = {}

local help_text = [[
Squared Project Generator

Usage:
  squared-pg help
  squared-pg version
  squared-pg doctor
  squared-pg verbose
  squared-pg agent-feedback
  squared-pg self-test
  squared-pg docs
  squared-pg uninstall [--confirm]
  squared-pg dependency add ID ARCHIVE
  squared-pg dependency status [ID]
  squared-pg kit add ARCHIVE
  squared-pg kit status
  squared-pg package add ARCHIVE.sq
  squared-pg package build SOURCE_DIRECTORY OUTPUT.sq
  squared-pg package verify ARCHIVE.sq
  squared-pg package sync [REPOSITORY]
  squared-pg package status
  squared-pg package resolve ID@VERSION
  squared-pg template status
  squared-pg template use ID@VERSION
  squared-pg project verify [DIRECTORY]
  squared-pg project build [DIRECTORY] [--clean] [--online-once]
  squared-pg project module add ID@VERSION [DIRECTORY]
  squared-pg project module status [DIRECTORY]
  squared-pg wrapper add EXISTING_PROJECT
  squared-pg wrapper status
  squared-pg new PROJECT_NAME --package JAVA.PACKAGE
      [--template ID@VERSION] [--app-name NAME] [--base-version VERSION]
      [--manage-all-files] [--sandbox|--project]
  squared-pg new PROJECT_NAME --foundation [--sandbox|--project]
  squared-pg promote PROJECT_NAME
  squared-pg demote PROJECT_NAME

Defaults:
  new projects are offline Android projects created beneath ~/sandbox
  promote copies ~/sandbox/NAME to ~/projects/NAME
  demote copies ~/projects/NAME to ~/sandbox/NAME
]]

local function default_environment()
    return {
        HOME = os.getenv("HOME"),
        PATH = os.getenv("PATH"),
        PWD = os.getenv("PWD"),
        SQUARED_PG_CONFIG = os.getenv("SQUARED_PG_CONFIG"),
        SQUARED_PG_ROOT = rawget(_G, "SQUARED_PG_ROOT"),
        SQUARED_PG_PRIVATE_LUA = rawget(_G, "SQUARED_PG_PRIVATE_LUA"),
        SQUARED_PG_CACHE_ROOT = os.getenv("SQUARED_PG_CACHE_ROOT"),
        SQUARED_PG_SANDBOX_ROOT = os.getenv("SQUARED_PG_SANDBOX_ROOT"),
        SQUARED_PG_PROJECTS_ROOT = os.getenv("SQUARED_PG_PROJECTS_ROOT"),
        SQUARED_PG_BIN_DIR = os.getenv("SQUARED_PG_BIN_DIR"),
        SDL_PG_BIN_DIR = os.getenv("SDL_PG_BIN_DIR"),
        SDL_PG_CONFIG = os.getenv("SDL_PG_CONFIG"),
        SDL_PG_ROOT = rawget(_G, "SDL_PG_ROOT"),
        SDL_PG_CACHE_ROOT = os.getenv("SDL_PG_CACHE_ROOT"),
        SDL_PG_SANDBOX_ROOT = os.getenv("SDL_PG_SANDBOX_ROOT"),
        SDL_PG_PROJECTS_ROOT = os.getenv("SDL_PG_PROJECTS_ROOT")
    }
end

local function writer(stream)
    return function(message)
        stream:write(message, "\n")
    end
end

local function command_new(settings, arguments, out)
    local name = arguments[2]
    local location = "sandbox"
    local options = {
        profile = "template"
    }
    local index = 3

    while arguments[index] do
        local argument = arguments[index]
        if argument == "--project" then
            location = "project"
        elseif argument == "--sandbox" then
            location = "sandbox"
        elseif argument == "--foundation" then
            options.profile = "foundation"
        elseif argument == "--manage-all-files" then
            options.manage_all_files = true
        elseif argument == "--package" then
            index = index + 1
            options.package_name = arguments[index]
            if not options.package_name then
                error("--package requires a value", 0)
            end
        elseif argument == "--template" then
            index = index + 1
            local coordinate = arguments[index]
            if not coordinate then
                error("--template requires ID@VERSION", 0)
            end
            options.template_id, options.template_version =
                coordinate:match("^([^@]+)@([^@]+)$")
            if not options.template_id then
                error("--template requires ID@VERSION", 0)
            end
        elseif argument == "--app-name" then
            index = index + 1
            options.application_name = arguments[index]
            if not options.application_name then
                error("--app-name requires a value", 0)
            end
        elseif argument == "--base-version" then
            index = index + 1
            options.base_version = arguments[index]
            if not options.base_version then
                error("--base-version requires a value", 0)
            end
        else
            error("unknown new option: " .. tostring(argument), 0)
        end
        index = index + 1
    end

    if options.profile == "foundation" and options.manage_all_files then
        error("--manage-all-files is only valid for Android projects", 0)
    end

    local destination = project.create(settings, name, location, options)
    out("Created project: " .. destination)
end

local function command_kit(settings, arguments, out)
    if arguments[2] == "add" and arguments[3] and not arguments[4] then
        local record = dependency.add(
            settings,
            "android-sdl2",
            arguments[3]
        )
        out("Registered SDL2 kit: " .. record.root)
    elseif arguments[2] == "status" and not arguments[3] then
        local value = dependency.status(settings, "android-sdl2")
        out(value.configured and value.valid and
            ("SDL2 kit: " .. value.root) or
            (value.configured and ("SDL2 kit: invalid: " .. value.root) or
                "SDL2 kit: not configured"))
    else
        error(
            "Usage: squared-pg kit add ARCHIVE | squared-pg kit status",
            0
        )
    end
end

local function write_dependency_status(record, out)
    out(
        record.id ..
        (record.configured and record.valid and
            ("  configured  " .. record.root) or
            (record.configured and ("  invalid     " .. record.root) or
                "  not configured"))
    )
end

local function command_dependency(settings, arguments, out)
    if arguments[2] == "add" and arguments[3] and arguments[4] and
        not arguments[5] then
        local record = dependency.add(settings, arguments[3], arguments[4])
        out("Registered dependency " .. record.id .. ": " .. record.root)
    elseif arguments[2] == "status" and not arguments[4] then
        if arguments[3] then
            write_dependency_status(
                dependency.status(settings, arguments[3]),
                out
            )
        else
            for _, record in ipairs(dependency.list(settings)) do
                write_dependency_status(record, out)
            end
        end
    else
        error(
            "Usage: squared-pg dependency add ID ARCHIVE | " ..
            "squared-pg dependency status [ID]",
            0
        )
    end
end

local function command_wrapper(settings, arguments, out)
    if arguments[2] == "add" and arguments[3] and not arguments[4] then
        out("Registered Gradle Wrapper: " ..
            wrapper.add(settings, arguments[3]))
    elseif arguments[2] == "status" and not arguments[3] then
        local value = wrapper.status(settings)
        out(value.configured and value.valid and
            ("Gradle Wrapper: " .. value.root) or
            (value.configured and
                ("Gradle Wrapper: invalid: " .. value.root) or
                "Gradle Wrapper: not configured"))
    else
        error(
            "Usage: squared-pg wrapper add PROJECT_DIRECTORY | " ..
            "squared-pg wrapper status",
            0
        )
    end
end

local function command_package(settings, arguments, environment, out)
    if arguments[2] == "add" and arguments[3] and not arguments[4] then
        local record = package_registry.add(settings, arguments[3])
        out(string.format(
            "%s SQ %s: %s %s",
            record.already_registered and
                "Already registered" or
                "Registered",
            record.kind,
            record.id,
            record.version
        ))
    elseif arguments[2] == "build" and arguments[3] and arguments[4] and
        not arguments[5] then
        local record = package_builder.build(arguments[3], arguments[4])
        out("Built SQ " .. record.kind .. ": " ..
            record.id .. " " .. record.version)
        out("Archive: " .. record.archive)
        out("SHA-256: " .. record.archive_sha256)
        out("Content digest: " .. record.content_digest)
    elseif arguments[2] == "verify" and arguments[3] and
        not arguments[4] then
        local record = package_builder.verify(arguments[3])
        out("SQ package: OK")
        out("Coordinate: " .. record.id .. "@" .. record.version)
        out("Kind: " .. record.kind)
        out("Content digest: " .. record.content_digest)
    elseif arguments[2] == "sync" and not arguments[4] then
        local repository = arguments[3] or environment.PWD or "."
        local records = package_sync.sync(settings, repository)
        for _, record in ipairs(records) do
            out(string.format(
                "%s  %s@%s  %s",
                record.archive_reused and "REUSED" or "BUILT ",
                record.id,
                record.version,
                record.already_registered and
                    "already registered" or "registered"
            ))
        end
        out("Synchronized SQ packages: " .. tostring(#records))
    elseif arguments[2] == "status" and not arguments[3] then
        local records = package_registry.list(settings)
        if #records == 0 then
            out("SQ packages: none registered")
        else
            for _, record in ipairs(records) do
                out(string.format(
                    "%-9s %-42s %s",
                    record.kind,
                    record.id,
                    record.version
                ))
            end
        end
    elseif arguments[2] == "resolve" and
        arguments[3] and not arguments[4] then
        local identifier, version =
            arguments[3]:match("^([^@]+)@([^@]+)$")
        if not identifier then
            error("package resolve requires ID@VERSION", 0)
        end
        local record, dependencies =
            package_registry.resolve(settings, identifier, version)
        for index, dependency in ipairs(dependencies) do
            out(string.format(
                "%d  %-9s %s@%s",
                index,
                dependency.kind,
                dependency.id,
                dependency.version
            ))
        end
        out(string.format(
            "%d  %-9s %s@%s (root)",
            #dependencies + 1,
            record.kind,
            record.id,
            record.version
        ))
    else
        error(
            "Usage: squared-pg package add ARCHIVE.sq | " ..
            "squared-pg package build SOURCE_DIRECTORY OUTPUT.sq | " ..
            "squared-pg package verify ARCHIVE.sq | " ..
            "squared-pg package sync [REPOSITORY] | " ..
            "squared-pg package status | " ..
            "squared-pg package resolve ID@VERSION",
            0
        )
    end
end

local function command_template(settings, arguments, out)
    if arguments[2] == "status" and not arguments[3] then
        local value = template_selection.status(settings)
        out("Active template: " .. value.coordinate)
        out("Selection: " .. value.source)
        out("Registered: " .. tostring(value.registered))
        out("Resolvable: " .. tostring(value.resolvable))
        if value.error then out("Issue: " .. value.error) end
    elseif arguments[2] == "use" and arguments[3] and not arguments[4] then
        local identifier, version = arguments[3]:match("^([^@]+)@([^@]+)$")
        if not identifier then
            error("template use requires ID@VERSION", 0)
        end
        local record, dependencies = template_selection.use(
            settings,
            identifier,
            version
        )
        out("Selected template: " .. record.id .. "@" .. record.version)
        out("Resolved modules: " .. tostring(#dependencies))
    else
        error(
            "Usage: squared-pg template status | " ..
            "squared-pg template use ID@VERSION",
            0
        )
    end
end

local function write_lines(lines, out)
    for _, line in ipairs(lines) do out(line) end
end

local function command_project(settings, arguments, environment, out)
    if arguments[2] == "verify" and not arguments[4] then
        local value = project.verify(arguments[3] or environment.PWD or ".")
        out("Project: " .. value.root)
        out("Marker: " .. value.marker)
        out("Verification: " .. (value.valid and "OK" or "FAILED"))
        for _, issue in ipairs(value.issues) do out("Issue: " .. issue) end
        if not value.valid then error("project verification failed", 0) end
        return
    end

    if arguments[2] == "build" then
        local directory
        local options = {
            lua = environment.SQUARED_PG_PRIVATE_LUA or "lua5.4"
        }
        for index = 3, #arguments do
            local argument = arguments[index]
            if argument == "--clean" then
                options.clean = true
            elseif argument == "--online-once" then
                options.online_once = true
            elseif argument:sub(1, 2) == "--" then
                error("unknown project build option: " .. argument, 0)
            elseif directory then
                error("project build accepts at most one directory", 0)
            else
                directory = argument
            end
        end
        local result = project.build(
            directory or environment.PWD or ".",
            options
        )
        out("Project: " .. result.root)
        out("Build: OK")
        return
    end

    if arguments[2] == "module" and arguments[3] == "add" and
        arguments[4] and not arguments[6] then
        local identifier, version = arguments[4]:match("^([^@]+)@([^@]+)$")
        if not identifier then
            error("project module add requires ID@VERSION", 0)
        end
        local result = project_module.add(
            settings,
            arguments[5] or environment.PWD or ".",
            identifier,
            version
        )
        out("Project: " .. result.root)
        out((result.already_enabled and "Already enabled: " or "Enabled module: ") ..
            result.coordinate)
        out("Added modules: " .. tostring(#result.added))
        return
    end

    if arguments[2] == "module" and arguments[3] == "status" and
        not arguments[5] then
        local root, modules = project_module.status(
            arguments[4] or environment.PWD or "."
        )
        out("Project: " .. root)
        if #modules == 0 then
            out("Optional modules: none enabled")
        else
            for _, record in ipairs(modules) do
                out(record.id .. "@" .. record.version .. "  " .. record.target)
            end
        end
        return
    end

    error(
        "Usage: squared-pg project verify [DIRECTORY] | " ..
        "squared-pg project build [DIRECTORY] " ..
        "[--clean] [--online-once] | " ..
        "squared-pg project module add ID@VERSION [DIRECTORY] | " ..
        "squared-pg project module status [DIRECTORY]",
        0
    )
end

local function command_self_test(settings, environment, out)
    local checks, healthy = self_test.run(settings, environment)
    for _, check in ipairs(checks) do
        out(string.format(
            "%-5s %-28s %s",
            check.ok and "OK" or "FAIL",
            check.label,
            tostring(check.detail)
        ))
    end
    if not healthy then error("self-test failed", 0) end
end

local function command_doctor(settings, environment, out)
    local records, healthy = doctor.inspect(settings, environment)

    for _, record in ipairs(records) do
        local status =
            record.ok and "OK" or
            (record.required and "MISSING" or "OPTIONAL")

        out(string.format(
            "%-9s %-16s %s",
            status,
            record.label,
            record.value
        ))
    end

    if not healthy then
        error("required environment items are missing", 0)
    end
end

--- Execute one command.
-- @param arguments Array of command arguments without the executable name.
-- @param environment Optional environment table for testing.
-- @param stdout Optional line-writer callback.
-- @param stderr Optional line-writer callback.
-- @return Process-style result code.
function main.run(arguments, environment, stdout, stderr)
    arguments = arguments or {}
    environment = environment or default_environment()
    stdout = stdout or writer(io.stdout)
    stderr = stderr or writer(io.stderr)

    local ok, message = xpcall(function()
        local command = arguments[1] or "help"

        if command == "help" or command == "--help" or command == "-h" then
            stdout(help_text:gsub("\n$", ""))
        elseif command == "version" or command == "--version" then
            stdout("squared-pg " .. release.version ..
                " (private Lua " .. release.private_lua .. ")")
        elseif command == "uninstall" then
            if arguments[2] and arguments[2] ~= "--confirm" or
                arguments[3] then
                error("Usage: squared-pg uninstall [--confirm]", 0)
            end
            uninstall.run(environment, arguments[2] == "--confirm", stdout)
        else
            local settings = config.load(environment)

            if command == "doctor" then
                command_doctor(settings, environment, stdout)
            elseif command == "verbose" then
                if arguments[2] then error("too many arguments for verbose", 0) end
                write_lines(
                    report.verbose_lines(report.collect(settings, environment)),
                    stdout
                )
            elseif command == "agent-feedback" then
                if arguments[2] then
                    error("too many arguments for agent-feedback", 0)
                end
                write_lines(
                    report.agent_lines(report.collect(settings, environment)),
                    stdout
                )
            elseif command == "self-test" then
                if arguments[2] then error("too many arguments for self-test", 0) end
                command_self_test(settings, environment, stdout)
            elseif command == "docs" then
                if arguments[2] then
                    error("too many arguments for docs", 0)
                end

                local outputs = docs.build(settings)
                stdout("Lua API: " .. outputs.lua)
                stdout("C++ API: " .. outputs.cpp)
                stdout("Java wrapper API: " .. outputs.java)
            elseif command == "dependency" then
                command_dependency(settings, arguments, stdout)
            elseif command == "kit" then
                command_kit(settings, arguments, stdout)
            elseif command == "wrapper" then
                command_wrapper(settings, arguments, stdout)
            elseif command == "package" then
                command_package(settings, arguments, environment, stdout)
            elseif command == "template" then
                command_template(settings, arguments, stdout)
            elseif command == "project" then
                command_project(settings, arguments, environment, stdout)
            elseif command == "new" then
                command_new(settings, arguments, stdout)
            elseif command == "promote" then
                if arguments[3] then
                    error("too many arguments for promote", 0)
                end

                stdout(
                    "Promoted project: " ..
                    project.promote(settings, arguments[2])
                )
            elseif command == "demote" then
                if arguments[3] then
                    error("too many arguments for demote", 0)
                end

                stdout(
                    "Demoted project: " ..
                    project.demote(settings, arguments[2])
                )
            else
                error("unknown command: " .. tostring(command), 0)
            end
        end
    end, function(value)
        return tostring(value)
    end)

    if not ok then
        stderr("ERROR: " .. message)
        return 1
    end

    return 0
end

return main
