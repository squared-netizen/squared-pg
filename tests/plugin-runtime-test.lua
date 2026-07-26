--- Exercise the generated APK-asset plug-in runtime without Android.
-- @module tests.plugin-runtime-test

local plugin_runtime_test = {}

local function load_module(filename)
    local file = assert(io.open(filename, "rb"))
    local source = assert(file:read("*a"))
    file:close()

    local chunk = assert(load(source, "@" .. filename, "t", _G))
    local result = chunk()
    assert(type(result) == "table", filename .. " must return a table")
    return result
end

local function find_record(records, expected)
    for index, record in ipairs(records) do
        if record == expected then
            return index
        end
    end
    return nil
end

--- Run deterministic plug-in runtime tests.
-- @tparam string root Generator repository root.
-- @tparam table test Test assertion helpers.
function plugin_runtime_test.run(root, test)
    local template_root =
        root .. "/templates/android/app/src/main/assets/lua"
    local module_loader =
        load_module(template_root .. "/runtime/module_loader.lua")
    local plugin_manager =
        load_module(template_root .. "/runtime/plugin_manager.lua")

    local records = {}
    local assets = {
        ["lua/plugins.lua"] = [[
            return {"first", "broken", "second", "future"}
        ]],
        ["lua/plugins/first/manifest.lua"] = [[
            return {
                id = "first",
                version = "1.2.3",
                api_version = 1,
                entry = "main",
                capabilities = {"log"}
            }
        ]],
        ["lua/plugins/first/helper.lua"] = [[
            return {value = "cached"}
        ]],
        ["lua/plugins/first/main.lua"] = [[
            assert(host._read_asset == nil)
            assert(host.set_tile == nil)
            assert(io == nil and os == nil and package == nil)
            assert(debug == nil and load == nil)

            local changed_host =
                pcall(function() host.extra = true end)
            assert(not changed_host)

            local changed_math =
                pcall(function() math.pi = 0 end)
            assert(not changed_math)

            local helper = require("helper")
            assert(helper == require("helper"))

            return {
                init = function()
                    host.log("first:init:" .. helper.value)
                end,
                update = function(delta_seconds)
                    host.log("first:update:" .. tostring(delta_seconds))
                end,
                shutdown = function()
                    host.log("first:shutdown")
                end
            }
        ]],
        ["lua/plugins/broken/manifest.lua"] = [[
            return {
                id = "broken",
                version = "1.0.0",
                api_version = 1,
                entry = "main",
                capabilities = {"log"}
            }
        ]],
        ["lua/plugins/broken/main.lua"] = [[
            return {
                init = function()
                    error("intentional init failure")
                end,
                update = function()
                    host.log("broken:update")
                end,
                shutdown = function()
                    host.log("broken:shutdown")
                end
            }
        ]],
        ["lua/plugins/second/manifest.lua"] = [[
            return {
                id = "second",
                version = "2.0.0",
                api_version = 1,
                entry = "main",
                capabilities = {"log", "display"}
            }
        ]],
        ["lua/plugins/second/main.lua"] = [[
            assert(type(host.set_tile) == "function")
            return {
                init = function()
                    host.log("second:init")
                end,
                update = function(delta_seconds)
                    host.log("second:update:" .. tostring(delta_seconds))
                end,
                shutdown = function()
                    host.log("second:shutdown")
                end
            }
        ]],
        ["lua/plugins/future/manifest.lua"] = [[
            return {
                id = "future",
                version = "1.0.0",
                api_version = 2,
                entry = "main",
                capabilities = {"log"}
            }
        ]],
        ["lua/plugins/future/main.lua"] = [[
            error("unsupported plug-in must not execute")
        ]]
    }

    local function read_asset(asset_path)
        local source = assets[asset_path]
        if source then
            return source
        end
        return nil, "fixture asset is missing: " .. asset_path
    end

    local native_host = {
        _read_asset = read_asset,
        logical_size = function()
            return 960, 540
        end,
        log = function(message)
            records[#records + 1] = message
        end,
        request_quit = function()
        end,
        runtime_version = function()
            return "Lua 5.4"
        end,
        set_background = function()
        end,
        set_tile = function()
        end
    }

    local application = plugin_manager.new({
        api_version = 1,
        module_loader = module_loader,
        native_host = native_host,
        read_asset = read_asset,
        registry_path = "lua/plugins.lua"
    })

    application.init()
    application.update(0.25)
    application.shutdown()

    local first_init = assert(find_record(records, "first:init:cached"))
    local second_init = assert(find_record(records, "second:init"))
    test.truthy(first_init < second_init, "registry-order initialization")

    local first_update =
        assert(find_record(records, "first:update:0.25"))
    local second_update =
        assert(find_record(records, "second:update:0.25"))
    test.truthy(first_update < second_update, "registry-order update")

    local second_shutdown =
        assert(find_record(records, "second:shutdown"))
    local first_shutdown =
        assert(find_record(records, "first:shutdown"))
    test.truthy(
        second_shutdown < first_shutdown,
        "reverse-order shutdown"
    )
    test.truthy(
        find_record(records, "broken:update") == nil and
            find_record(records, "broken:shutdown") == nil,
        "failed initialization disables one plug-in"
    )

    local saw_init_failure = false
    local saw_api_rejection = false
    for _, record in ipairs(records) do
        if record:find("intentional init failure", 1, true) then
            saw_init_failure = true
        end
        if record:find(
                "unsupported plug-in API version 2",
                1,
                true
            ) then
            saw_api_rejection = true
        end
    end
    test.truthy(saw_init_failure, "plug-in init failure reporting")
    test.truthy(saw_api_rejection, "plug-in API version rejection")

    local cycle_assets = {
        ["lua/plugins/cycle/a.lua"] = [[return require("b")]],
        ["lua/plugins/cycle/b.lua"] = [[return require("a")]]
    }
    local cycle_environment = {
        assert = assert,
        error = error,
        pcall = pcall,
        type = type
    }
    local cycle_loader = module_loader.new({
        read_asset = function(asset_path)
            return cycle_assets[asset_path]
        end,
        environment = cycle_environment,
        root = "lua/plugins/cycle"
    })
    cycle_environment.require = cycle_loader.require

    test.fails(function()
        cycle_loader.require("a")
    end, "cyclic plug-in module dependency")
    test.fails(function()
        cycle_loader.require("../escape")
    end, "invalid module name")
end

return plugin_runtime_test
