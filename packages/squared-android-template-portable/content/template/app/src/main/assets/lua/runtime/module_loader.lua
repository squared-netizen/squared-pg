--- APK-asset Lua module loader.
--
-- Each loader instance is rooted beneath one validated asset directory and
-- owns a private cache. Module names use dotted Lua identifiers and cannot
-- escape that directory.
-- @module runtime.module_loader

local compile = assert(load)
local module_loader = {}
local loading = {}

local function validate_module_name(name)
    if type(name) ~= "string" or name == "" then
        error("module name must be a non-empty string", 0)
    end

    if name:sub(1, 1) == "." or
        name:sub(-1) == "." or
        name:find("..", 1, true) then
        error("invalid module name: " .. name, 0)
    end

    local segments = 0
    for segment in name:gmatch("[^.]+") do
        if not segment:match("^[a-zA-Z_][a-zA-Z0-9_]*$") then
            error("invalid module name: " .. name, 0)
        end
        segments = segments + 1
    end
    if segments == 0 then
        error("invalid module name: " .. name, 0)
    end
end

local function validate_root(root)
    if type(root) ~= "string" or
        not root:match("^lua/plugins/[a-z][a-z0-9_]*$") then
        error("invalid plug-in module root: " .. tostring(root), 0)
    end
end

--- Create a plug-in-local module loader.
-- @tparam table options Loader options.
-- @tparam function options.read_asset Trusted asset reader.
-- @tparam table options.environment Plug-in environment.
-- @tparam string options.root Validated plug-in asset root.
-- @treturn table Loader exposing `require(name)`.
function module_loader.new(options)
    assert(type(options) == "table", "module loader options are required")
    assert(
        type(options.read_asset) == "function",
        "module loader read_asset must be a function"
    )
    assert(
        type(options.environment) == "table",
        "module loader environment must be a table"
    )
    validate_root(options.root)

    local cache = {}
    local loader = {}

    --- Load one module relative to the plug-in root.
    -- @tparam string name Dotted local module name.
    -- @return Module export, or `true` when the chunk returns `nil`.
    function loader.require(name)
        validate_module_name(name)

        if cache[name] == loading then
            error("cyclic plug-in module dependency: " .. name, 0)
        end
        if cache[name] ~= nil then
            return cache[name]
        end

        local asset_path =
            options.root .. "/" .. name:gsub("%.", "/") .. ".lua"
        local source, read_error = options.read_asset(asset_path)
        if type(source) ~= "string" then
            error(
                "cannot read plug-in module " ..
                    name ..
                    ": " ..
                    tostring(read_error),
                0
            )
        end

        local chunk, compile_error =
            compile(
                source,
                "@" .. asset_path,
                "t",
                options.environment
            )
        if not chunk then
            error(
                "cannot compile plug-in module " ..
                    name ..
                    ": " ..
                    tostring(compile_error),
                0
            )
        end

        cache[name] = loading
        local succeeded, result = pcall(chunk)
        if not succeeded then
            cache[name] = nil
            error(
                "cannot execute plug-in module " ..
                    name ..
                    ": " ..
                    tostring(result),
                0
            )
        end

        if result == nil then
            result = true
        end
        cache[name] = result
        return result
    end

    return loader
end

return module_loader
