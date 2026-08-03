--- Trusted bootstrap for the generated application plug-in runtime.
--
-- This file alone receives the native host table. Plug-ins receive narrower
-- capability proxies created by the plug-in manager.
-- @script bootstrap

local native_host = assert(host, "native host API is unavailable")
local compile = assert(load, "Lua text loader is unavailable")

local function load_trusted(asset_path)
    local source, read_error = native_host._read_asset(asset_path)
    if not source then
        error(
            "cannot read trusted Lua asset " ..
                asset_path ..
                ": " ..
                tostring(read_error),
            0
        )
    end

    local chunk, compile_error =
        compile(source, "@" .. asset_path, "t", _ENV)
    if not chunk then
        error(
            "cannot compile trusted Lua asset " ..
                asset_path ..
                ": " ..
                tostring(compile_error),
            0
        )
    end

    local succeeded, result = pcall(chunk)
    if not succeeded then
        error(
            "cannot execute trusted Lua asset " ..
                asset_path ..
                ": " ..
                tostring(result),
            0
        )
    end

    if type(result) ~= "table" then
        error("trusted Lua asset must return a table: " .. asset_path, 0)
    end

    return result
end

local module_loader =
    load_trusted("lua/runtime/module_loader.lua")
local plugin_manager =
    load_trusted("lua/runtime/plugin_manager.lua")

local application = plugin_manager.new({
    api_version = 1,
    module_loader = module_loader,
    native_host = native_host,
    read_asset = native_host._read_asset,
    registry_path = "lua/plugins.lua"
})

-- Remove the raw native boundary from the shared global environment after all
-- trusted runtime modules have captured what they require.
host = nil
load = nil

return application
