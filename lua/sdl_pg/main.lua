--- Command-line interface for SDL Project Generator.
-- @module sdl_pg.main

local config = require("sdl_pg.config")
local doctor = require("sdl_pg.doctor")
local docs = require("sdl_pg.docs")
local kit = require("sdl_pg.kit")
local project = require("sdl_pg.project")
local wrapper = require("sdl_pg.wrapper")

local main = {}
local version = "0.4.0"

local help_text = [[
SDL Project Generator

Usage:
  sdl-pg help
  sdl-pg version
  sdl-pg doctor
  sdl-pg docs
  sdl-pg kit add ARCHIVE
  sdl-pg kit status
  sdl-pg wrapper add EXISTING_PROJECT
  sdl-pg wrapper status
  sdl-pg new PROJECT_NAME --package JAVA.PACKAGE [--sandbox|--project]
  sdl-pg new PROJECT_NAME --foundation [--sandbox|--project]
  sdl-pg promote PROJECT_NAME
  sdl-pg demote PROJECT_NAME

Defaults:
  new projects are offline Android projects created beneath ~/sandbox
  promote copies ~/sandbox/NAME to ~/projects/NAME
  demote copies ~/projects/NAME to ~/sandbox/NAME
]]

local function default_environment()
    return {
        HOME = os.getenv("HOME"),
        PATH = os.getenv("PATH"),
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
        profile = "android"
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
        elseif argument == "--package" then
            index = index + 1
            options.package_name = arguments[index]
            if not options.package_name then
                error("--package requires a value", 0)
            end
        else
            error("unknown new option: " .. tostring(argument), 0)
        end
        index = index + 1
    end

    local destination = project.create(settings, name, location, options)
    out("Created project: " .. destination)
end

local function command_kit(settings, arguments, out)
    if arguments[2] == "add" and arguments[3] and not arguments[4] then
        out("Registered SDL2 kit: " .. kit.add(settings, arguments[3]))
    elseif arguments[2] == "status" and not arguments[3] then
        local value = kit.status(settings)
        out(value.configured and ("SDL2 kit: " .. value.root) or
            "SDL2 kit: not configured")
    else
        error("Usage: sdl-pg kit add ARCHIVE | sdl-pg kit status", 0)
    end
end

local function command_wrapper(settings, arguments, out)
    if arguments[2] == "add" and arguments[3] and not arguments[4] then
        out("Registered Gradle Wrapper: " ..
            wrapper.add(settings, arguments[3]))
    elseif arguments[2] == "status" and not arguments[3] then
        local value = wrapper.status(settings)
        out(value.configured and ("Gradle Wrapper: " .. value.root) or
            "Gradle Wrapper: not configured")
    else
        error(
            "Usage: sdl-pg wrapper add PROJECT_DIRECTORY | " ..
            "sdl-pg wrapper status",
            0
        )
    end
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
            stdout("sdl-pg " .. version .. " (private Lua 5.4.8)")
        else
            local settings = config.load(environment)

            if command == "doctor" then
                command_doctor(settings, environment, stdout)
            elseif command == "docs" then
                if arguments[2] then
                    error("too many arguments for docs", 0)
                end

                local outputs = docs.build(settings)
                stdout("Lua API: " .. outputs.lua)
                stdout("C++ API: " .. outputs.cpp)
                stdout("Java wrapper API: " .. outputs.java)
            elseif command == "kit" then
                command_kit(settings, arguments, stdout)
            elseif command == "wrapper" then
                command_wrapper(settings, arguments, stdout)
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
