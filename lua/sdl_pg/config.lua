--- Configuration loader for Squared Project Generator.
-- @module sdl_pg.config

local fs = require("sdl_pg.fs")
local path = require("sdl_pg.path")

local config = {}

local function read_user_config(filename)
    if not fs.exists(filename) then
        return {}
    end

    local chunk, message = loadfile(filename, "t", {})
    if not chunk then
        error(
            "cannot load configuration " ..
            filename ..
            ": " ..
            tostring(message),
            0
        )
    end

    local ok, result = pcall(chunk)
    if not ok then
        error("configuration failed: " .. tostring(result), 0)
    end

    if type(result) ~= "table" then
        error("configuration must return a table: " .. filename, 0)
    end

    return result
end

--- Load workspace configuration.
-- Environment values override the user configuration file.
-- @param environment Environment table, normally derived from `os.getenv`.
-- @return Resolved configuration table.
function config.load(environment)
    environment = environment or {}
    local home = environment.HOME

    if not home or home == "" then
        error("HOME is not set", 0)
    end

    local primary_filename =
        path.join(home, ".config/squared-pg/config.lua")
    local legacy_filename =
        path.join(home, ".config/sdl-pg/config.lua")
    local filename =
        environment.SQUARED_PG_CONFIG or
        environment.SDL_PG_CONFIG or
        (fs.exists(primary_filename) and primary_filename) or
        (fs.exists(legacy_filename) and legacy_filename) or
        primary_filename
    local user = read_user_config(filename)

    local primary_cache = path.join(home, ".local/share/squared-pg")
    local legacy_cache = path.join(home, ".local/share/sdl-pg")
    local default_cache =
        fs.exists(primary_cache) and primary_cache or
        fs.exists(legacy_cache) and legacy_cache or
        primary_cache

    return {
        home = home,
        generator_root =
            environment.SQUARED_PG_ROOT or environment.SDL_PG_ROOT,
        config_file = filename,
        cache_root =
            environment.SQUARED_PG_CACHE_ROOT or
            environment.SDL_PG_CACHE_ROOT or
            user.cache_root or
            default_cache,
        sandbox_root =
            environment.SQUARED_PG_SANDBOX_ROOT or
            environment.SDL_PG_SANDBOX_ROOT or
            user.sandbox_root or
            path.join(home, "sandbox"),
        projects_root =
            environment.SQUARED_PG_PROJECTS_ROOT or
            environment.SDL_PG_PROJECTS_ROOT or
            user.projects_root or
            path.join(home, "projects")
    }
end

return config
