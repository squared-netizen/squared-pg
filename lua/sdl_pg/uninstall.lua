--- Guarded user-local uninstallation for Squared Project Generator.
-- @module sdl_pg.uninstall

local lfs = require("lfs")
local fs = require("sdl_pg.fs")
local path = require("sdl_pg.path")

local uninstall = {}

local launcher_names = {
    "squared-pg",
    "squared-project-generator",
    "sdl-pg",
    "sdl-project-generator"
}

local function require_safe_home(environment)
    local home = environment.HOME

    if type(home) ~= "string" or home == "" or home == "/" or
        home:sub(1, 1) ~= "/" then
        error("uninstall requires an absolute, non-root HOME", 0)
    end

    for component in home:gmatch("[^/]+") do
        if component == "." or component == ".." then
            error("uninstall refuses HOME containing '.' or '..'", 0)
        end
    end

    return home
end

local function is_within(value, root)
    return value == root or value:sub(1, #root + 1) == root .. "/"
end

--- Return the explicit generator-owned removal plan.
-- @param environment Process environment table.
-- @return Ordered target records with the generator root last.
function uninstall.plan(environment)
    environment = environment or {}
    local home = require_safe_home(environment)
    local canonical_root = path.join(home, ".squared/squared-pg")
    local generator_root =
        environment.SQUARED_PG_ROOT or
        environment.SDL_PG_ROOT or
        canonical_root

    if generator_root ~= canonical_root then
        error(
            "uninstall refuses non-canonical generator root: " ..
            tostring(generator_root),
            0
        )
    end

    local bin_directory =
        environment.SQUARED_PG_BIN_DIR or
        environment.SDL_PG_BIN_DIR or
        path.join(home, ".bin")
    local targets = {}

    for _, name in ipairs(launcher_names) do
        targets[#targets + 1] = {
            kind = "launcher",
            value = path.join(bin_directory, name)
        }
    end

    for _, value in ipairs({
        path.join(home, ".config/squared-pg"),
        path.join(home, ".config/sdl-pg"),
        path.join(home, ".local/share/squared-pg"),
        path.join(home, ".local/share/sdl-pg")
    }) do
        targets[#targets + 1] = {
            kind = "state",
            value = value
        }
    end

    targets[#targets + 1] = {
        kind = "generator",
        value = canonical_root
    }

    return targets, {
        path.join(home, "sandbox"),
        path.join(home, "projects")
    }, canonical_root
end

--- Print a plan or execute a confirmed uninstall.
-- @param environment Process environment table.
-- @param confirmed Whether destructive removal was explicitly confirmed.
-- @param out Line-writer callback.
function uninstall.run(environment, confirmed, out)
    local targets, preserved, generator_root =
        uninstall.plan(environment)

    if not confirmed then
        out("Squared Project Generator uninstall plan:")
        for _, target in ipairs(targets) do
            out("  remove " .. target.value)
        end
        out("Preserved end-user roots:")
        for _, value in ipairs(preserved) do
            out("  preserve " .. value)
        end
        out("No files were removed.")
        out("Run squared-pg uninstall --confirm to apply this plan.")
        return
    end

    local current_directory = environment.PWD or lfs.currentdir()
    if is_within(current_directory, generator_root) then
        error(
            "change directory outside " .. generator_root ..
            " before confirming uninstall",
            0
        )
    end

    for _, target in ipairs(targets) do
        if fs.exists(target.value) then
            out("Removing: " .. target.value)
            fs.remove_tree(target.value)
        else
            out("Already absent: " .. target.value)
        end
    end

    out("Squared Project Generator uninstalled.")
    out("End-user projects were preserved.")
end

return uninstall
