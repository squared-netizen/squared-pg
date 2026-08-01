--- Creation and verification of standalone SQ archives.
-- @module sdl_pg.package_builder

local fs = require("sdl_pg.fs")
local path = require("sdl_pg.path")
local sq = require("squared.sq")

local builder = {}

local function fail(value)
    if type(value) == "table" then
        error(string.format(
            "%s [%s/%s]",
            tostring(value.message),
            tostring(value.phase),
            tostring(value.code)
        ), 0)
    end
    error(tostring(value), 0)
end

local function inspect(archive)
    local package, open_error = sq.open(archive)
    if not package then
        fail(open_error)
    end
    local ok, result = xpcall(function()
        local manifest = package:manifest()
        local validation, validation_error = package:validate({
            expected_kind = manifest.kind
        })
        if not validation then
            fail(validation_error)
        end
        if not validation.valid then
            error("SQ package validation reported one or more issues", 0)
        end
        return {
            archive = archive,
            id = manifest.id,
            version = manifest.version,
            kind = manifest.kind,
            name = manifest.name,
            content_digest = validation.content_digest
        }
    end, debug.traceback)
    package:close()
    if not ok then
        error(result, 0)
    end
    return result
end

--- Build a package source directory into a new immutable SQ archive.
-- @param source Directory containing manifest.json and content/.
-- @param destination New .sq archive path.
-- @return Package summary.
function builder.build(source, destination)
    if fs.mode(source) ~= "directory" then
        error("package source directory not found: " .. tostring(source), 0)
    end
    if fs.mode(path.join(source, "manifest.json")) ~= "file" then
        error("package source is missing manifest.json: " .. source, 0)
    end
    if fs.mode(path.join(source, "content")) ~= "directory" then
        error("package source is missing content/: " .. source, 0)
    end
    if type(destination) ~= "string" or not destination:match("%.sq$") then
        error("package output must end in .sq", 0)
    end
    if fs.exists(destination) then
        error("package output already exists: " .. destination, 0)
    end
    fs.mkdir_p(path.dirname(destination), {
        follow_directory_links = true
    })
    local ok, result = xpcall(function()
        local created, create_error = sq.create_from_directory({
            manifest_json = fs.read_file(path.join(source, "manifest.json")),
            content_directory = path.join(source, "content"),
            destination = destination
        })
        if not created then fail(create_error) end
        local summary = inspect(destination)
        summary.file_count = created.file_count
        summary.archive_sha256 = created.archive_sha256
        return summary
    end, debug.traceback)
    if not ok then
        if fs.exists(destination) then fs.remove_tree(destination) end
        error(result, 0)
    end
    return result
end

--- Verify an existing SQ archive without modifying it.
-- @param archive Existing .sq archive.
-- @return Package summary.
function builder.verify(archive)
    if fs.mode(archive) ~= "file" then
        error("SQ package is not a regular file: " .. tostring(archive), 0)
    end
    return inspect(archive)
end

return builder
