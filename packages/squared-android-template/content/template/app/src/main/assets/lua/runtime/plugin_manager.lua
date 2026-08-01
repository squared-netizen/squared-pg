--- Versioned, capability-limited application plug-in manager.
--
-- Registry order defines load and lifecycle order. A plug-in cannot access the
-- raw asset reader or capabilities it did not declare.
-- @module runtime.plugin_manager

local plugin_manager = {}

local capability_functions = {
    application = {
        "request_quit"
    },
    display = {
        "logical_size",
        "set_background",
        "set_tile"
    },
    log = {
        "log"
    }
}

local safe_globals = {
    "assert",
    "error",
    "ipairs",
    "next",
    "pairs",
    "pcall",
    "select",
    "tonumber",
    "tostring",
    "type",
    "xpcall"
}

local safe_libraries = {
    "coroutine",
    "math",
    "string",
    "table",
    "utf8"
}

local function readonly(values, label)
    return setmetatable({}, {
        __index = values,
        __newindex = function()
            error(label .. " is read-only", 2)
        end,
        __metatable = false
    })
end

local function load_table(read_asset, asset_path, environment)
    local source, read_error = read_asset(asset_path)
    if type(source) ~= "string" then
        error(
            "cannot read " ..
                asset_path ..
                ": " ..
                tostring(read_error),
            0
        )
    end

    local chunk, compile_error =
        load(source, "@" .. asset_path, "t", environment)
    if not chunk then
        error(
            "cannot compile " ..
                asset_path ..
                ": " ..
                tostring(compile_error),
            0
        )
    end

    local succeeded, result = pcall(chunk)
    if not succeeded then
        error(
            "cannot execute " ..
                asset_path ..
                ": " ..
                tostring(result),
            0
        )
    end
    if type(result) ~= "table" then
        error(asset_path .. " must return a table", 0)
    end
    return result
end

local function validate_plugin_id(value)
    if type(value) ~= "string" or
        not value:match("^[a-z][a-z0-9_]*$") then
        error("invalid plug-in id: " .. tostring(value), 0)
    end
end

local function validate_manifest(manifest, expected_id, api_version)
    if manifest.id ~= expected_id then
        error(
            "manifest id mismatch: expected " ..
                expected_id ..
                ", found " ..
                tostring(manifest.id),
            0
        )
    end
    if type(manifest.version) ~= "string" or
        not manifest.version:match("^%d+%.%d+%.%d+$") then
        error("invalid plug-in version: " .. tostring(manifest.version), 0)
    end
    if manifest.api_version ~= api_version then
        error(
            "unsupported plug-in API version " ..
                tostring(manifest.api_version) ..
                "; expected " ..
                tostring(api_version),
            0
        )
    end
    if manifest.entry ~= nil and
        (type(manifest.entry) ~= "string" or manifest.entry == "") then
        error("plug-in entry must be a non-empty module name", 0)
    end
    if type(manifest.capabilities) ~= "table" then
        error("plug-in capabilities must be an array", 0)
    end
end

local function make_host_proxy(native_host, manifest, api_version)
    local exposed = {
        api_version = api_version,
        plugin_id = manifest.id,
        plugin_version = manifest.version
    }
    local declared = {}

    for index, capability in ipairs(manifest.capabilities) do
        if type(capability) ~= "string" or
            not capability_functions[capability] then
            error("unknown plug-in capability: " .. tostring(capability), 0)
        end
        if declared[capability] then
            error("duplicate plug-in capability: " .. capability, 0)
        end
        declared[capability] = true

        for _, function_name in ipairs(capability_functions[capability]) do
            local callback = native_host[function_name]
            if type(callback) ~= "function" then
                error(
                    "native host function is unavailable: " .. function_name,
                    0
                )
            end
            exposed[function_name] = callback
        end
    end

    return readonly(exposed, "plug-in host API")
end

local function make_environment(host_proxy)
    local environment = {
        _VERSION = _VERSION,
        host = host_proxy
    }

    for _, name in ipairs(safe_globals) do
        environment[name] = _G[name]
    end
    for _, name in ipairs(safe_libraries) do
        environment[name] = readonly(_G[name], "Lua " .. name .. " library")
    end

    environment._G = readonly(environment, "plug-in global environment")
    return environment
end

--- Create the application lifecycle dispatcher.
-- @tparam table options Manager options supplied by the trusted bootstrap.
-- @tparam number options.api_version Supported plug-in API version.
-- @tparam table options.module_loader Trusted module-loader implementation.
-- @tparam table options.native_host Complete native host boundary.
-- @tparam function options.read_asset Trusted APK-asset reader.
-- @tparam string options.registry_path Deterministic registry asset.
-- @treturn table Application callback table consumed by C++.
function plugin_manager.new(options)
    assert(type(options) == "table", "plug-in manager options are required")
    assert(type(options.api_version) == "number", "api_version is required")
    assert(
        type(options.module_loader) == "table",
        "module_loader is required"
    )
    assert(type(options.native_host) == "table", "native_host is required")
    assert(type(options.read_asset) == "function", "read_asset is required")
    assert(
        type(options.registry_path) == "string",
        "registry_path is required"
    )

    local native_host = options.native_host
    local plugins = {}

    local function report(message)
        native_host.log("[plug-ins] " .. tostring(message))
    end

    local registry =
        load_table(options.read_asset, options.registry_path, {})
    local seen = {}

    for index, plugin_id in ipairs(registry) do
        validate_plugin_id(plugin_id)
        if seen[plugin_id] then
            error("duplicate plug-in registry entry: " .. plugin_id, 0)
        end
        seen[plugin_id] = true

        local succeeded, plugin_or_error = pcall(function()
            local root = "lua/plugins/" .. plugin_id
            local manifest =
                load_table(
                    options.read_asset,
                    root .. "/manifest.lua",
                    {}
                )
            validate_manifest(
                manifest,
                plugin_id,
                options.api_version
            )

            local environment =
                make_environment(
                    make_host_proxy(
                        native_host,
                        manifest,
                        options.api_version
                    )
                )
            local loader = options.module_loader.new({
                read_asset = options.read_asset,
                environment = environment,
                root = root
            })
            environment.require = loader.require

            local callbacks = loader.require(manifest.entry or "main")
            if type(callbacks) ~= "table" then
                error("plug-in entry module must return a table", 0)
            end

            return {
                callbacks = callbacks,
                disabled = false,
                disabled_callbacks = {},
                id = plugin_id,
                index = index,
                initialized = false,
                version = manifest.version
            }
        end)

        if succeeded then
            plugins[#plugins + 1] = plugin_or_error
            report(
                "loaded " ..
                    plugin_or_error.id ..
                    " " ..
                    plugin_or_error.version
            )
        else
            report(
                "disabled " ..
                    plugin_id ..
                    " during load: " ..
                    tostring(plugin_or_error)
            )
        end
    end

    local function invoke(plugin, callback_name, ...)
        if plugin.disabled or plugin.disabled_callbacks[callback_name] then
            return
        end

        local callback = plugin.callbacks[callback_name]
        if callback == nil then
            return
        end
        if type(callback) ~= "function" then
            plugin.disabled_callbacks[callback_name] = true
            report(
                plugin.id ..
                    "." ..
                    callback_name ..
                    " is not a function; callback disabled"
            )
            return
        end

        local succeeded, message = pcall(callback, ...)
        if not succeeded then
            plugin.disabled_callbacks[callback_name] = true
            if callback_name == "init" then
                plugin.disabled = true
            end
            report(
                plugin.id ..
                    "." ..
                    callback_name ..
                    " failed: " ..
                    tostring(message)
            )
            return
        end

        if callback_name == "init" then
            plugin.initialized = true
        end
    end

    return {
        init = function()
            for _, plugin in ipairs(plugins) do
                invoke(plugin, "init")
            end
        end,
        update = function(delta_seconds)
            for _, plugin in ipairs(plugins) do
                invoke(plugin, "update", delta_seconds)
            end
        end,
        event = function(kind, first, second)
            for _, plugin in ipairs(plugins) do
                invoke(plugin, "event", kind, first, second)
            end
        end,
        shutdown = function()
            for index = #plugins, 1, -1 do
                local plugin = plugins[index]
                if plugin.initialized then
                    invoke(plugin, "shutdown")
                end
            end
        end
    }
end

return plugin_manager
