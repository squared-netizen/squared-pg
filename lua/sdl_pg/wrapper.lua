--- Gradle Wrapper cache registration.
-- @module sdl_pg.wrapper

local fs = require("sdl_pg.fs")
local path = require("sdl_pg.path")
local state = require("sdl_pg.state")

local wrapper = {}

local required_files = {
    "gradlew",
    "gradlew.bat",
    "gradle/wrapper/gradle-wrapper.jar",
    "gradle/wrapper/gradle-wrapper.properties"
}

local function verify(root)
    for _, filename in ipairs(required_files) do
        if fs.mode(path.join(root, filename)) ~= "file" then
            error("Gradle Wrapper source is missing " .. filename, 0)
        end
    end
end

--- Register a Gradle Wrapper from an existing working project.
-- @param settings Resolved configuration.
-- @param source_project Project containing `gradlew`.
-- @return Cached wrapper directory.
function wrapper.add(settings, source_project)
    if fs.mode(source_project) ~= "directory" then
        error("wrapper source project not found: " ..
            tostring(source_project), 0)
    end

    verify(source_project)
    local wrapper_root =
        path.join(settings.cache_root, "gradle-wrapper")
    local staged_root =
        path.join(
            settings.cache_root,
            "gradle-wrapper-stage-" .. tostring(os.time())
        )

    if fs.exists(staged_root) then
        error("wrapper staging path already exists: " .. staged_root, 0)
    end

    fs.mkdir_p(staged_root)

    local ok, message = xpcall(function()
        for _, filename in ipairs(required_files) do
            fs.copy_file(
                path.join(source_project, filename),
                path.join(staged_root, filename)
            )
        end
        verify(staged_root)
    end, debug.traceback)

    if not ok then
        fs.remove_tree(staged_root)
        error(message, 0)
    end

    local previous = wrapper_root .. ".previous"
    if fs.exists(previous) then
        fs.remove_tree(previous)
    end
    if fs.exists(wrapper_root) then
        local moved, move_message = os.rename(wrapper_root, previous)
        if not moved then
            fs.remove_tree(staged_root)
            error("cannot preserve previous wrapper: " ..
                tostring(move_message), 0)
        end
    end

    local installed, install_message =
        os.rename(staged_root, wrapper_root)
    if not installed then
        if fs.exists(previous) then
            os.rename(previous, wrapper_root)
        end
        fs.remove_tree(staged_root)
        error("cannot install wrapper: " ..
            tostring(install_message), 0)
    end

    if fs.exists(previous) then
        fs.remove_tree(previous)
    end

    local values = state.load(settings)
    values.wrapper_root = wrapper_root
    state.save(settings, values)

    return wrapper_root
end

--- Get and validate the active Gradle Wrapper.
-- @param settings Resolved configuration.
-- @return Wrapper root.
function wrapper.require_active(settings)
    local values = state.load(settings)

    if not values.wrapper_root then
        error(
            "no Gradle Wrapper is registered; " ..
            "run sdl-pg wrapper add PROJECT_DIRECTORY",
            0
        )
    end

    verify(values.wrapper_root)
    return values.wrapper_root
end

--- Read wrapper status.
-- @param settings Resolved configuration.
-- @return Status table.
function wrapper.status(settings)
    local values = state.load(settings)
    return {
        configured = values.wrapper_root ~= nil,
        root = values.wrapper_root
    }
end

return wrapper
