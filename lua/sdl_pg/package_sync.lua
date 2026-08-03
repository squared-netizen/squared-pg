--- Repository-wide SQ package build, verification, and registration.
-- @module sdl_pg.package_sync

local fs = require("sdl_pg.fs")
local package_builder = require("sdl_pg.package_builder")
local package_registry = require("sdl_pg.package_registry")
local path = require("sdl_pg.path")

local package_sync = {}
local temporary_counter = 0

local function temporary_root(repository)
    repeat
        temporary_counter = temporary_counter + 1
        local candidate = path.join(
            repository,
            ".squared-pg-sync-" ..
                tostring(os.time()) .. "-" ..
                tostring(temporary_counter)
        )
        if not fs.exists(candidate) then return candidate end
    until false
end

local function same_package(left, right)
    return left.id == right.id and
        left.version == right.version and
        left.kind == right.kind and
        left.content_digest == right.content_digest
end

--- Build and register every package beneath REPOSITORY/packages.
-- Existing identical archives and registry records are reused. Changed
-- content under an existing ID/version is rejected.
-- @param settings Resolved generator configuration.
-- @param repository Framework or package repository root.
-- @return Stable array of synchronized package summaries.
function package_sync.sync(settings, repository)
    repository = tostring(repository or ".")
    if fs.mode(repository) ~= "directory" then
        error("package repository not found: " .. repository, 0)
    end
    local packages_root = path.join(repository, "packages")
    if fs.mode(packages_root) ~= "directory" then
        error("package repository is missing packages/: " .. repository, 0)
    end

    local sources = {}
    for _, name in ipairs(fs.entries(packages_root)) do
        local source = path.join(packages_root, name)
        if fs.mode(source) == "directory" then
            local has_manifest = fs.exists(path.join(source, "manifest.json"))
            local has_content = fs.exists(path.join(source, "content"))
            if has_manifest or has_content then
                sources[#sources + 1] = {name = name, source = source}
            end
        end
    end
    if #sources == 0 then
        error("no SQ package sources found beneath: " .. packages_root, 0)
    end

    local staging = temporary_root(repository)
    fs.mkdir_p(staging)
    local ok, result = xpcall(function()
        local records = {}
        local coordinates = {}
        local dist = path.join(repository, "dist")
        for index, source in ipairs(sources) do
            local temporary_archive = path.join(
                staging,
                string.format("%03d-%s.sq", index, source.name)
            )
            local summary = package_builder.build(
                source.source,
                temporary_archive
            )
            local coordinate = summary.id .. "@" .. summary.version
            if coordinates[coordinate] then
                error(
                    "duplicate SQ package coordinate in repository: " ..
                    coordinate,
                    0
                )
            end
            coordinates[coordinate] = true
            summary.source = source.source
            summary.final_archive = path.join(
                dist,
                source.name .. "-" .. summary.version .. ".sq"
            )
            records[#records + 1] = summary
        end

        -- Preflight every immutable output and registry coordinate before
        -- committing any newly built archive.
        for _, summary in ipairs(records) do
            if fs.exists(summary.final_archive) then
                local existing = package_builder.verify(
                    summary.final_archive
                )
                if not same_package(existing, summary) then
                    error(
                        "SQ archive conflicts with package source; " ..
                        "bump the package version: " .. summary.final_archive,
                        0
                    )
                end
                summary.archive_reused = true
            end
            local registered = package_registry.find(
                settings,
                summary.id,
                summary.version
            )
            if registered and not same_package(registered, summary) then
                error(
                    "registered SQ package conflicts with source; " ..
                    "bump the package version: " ..
                    summary.id .. " " .. summary.version,
                    0
                )
            end
            summary.already_registered = registered ~= nil
        end

        fs.mkdir_p(dist)
        for _, summary in ipairs(records) do
            if not summary.archive_reused then
                local renamed, rename_error = os.rename(
                    summary.archive,
                    summary.final_archive
                )
                if not renamed then
                    error(
                        "cannot commit SQ archive: " ..
                        tostring(rename_error),
                        0
                    )
                end
            end
            summary.archive = summary.final_archive
        end
        for _, summary in ipairs(records) do
            local registered = package_registry.add(settings, summary.archive)
            summary.already_registered = registered.already_registered == true
        end
        return records
    end, debug.traceback)

    fs.remove_tree(staging)
    if not ok then error(result, 0) end
    return result
end

return package_sync
