--- SDL2 offline-kit registration and verification.
-- @module sdl_pg.kit

local fs = require("sdl_pg.fs")
local path = require("sdl_pg.path")
local process = require("sdl_pg.process")
local sha256 = require("sdl_pg.sha256")
local state = require("sdl_pg.state")

local kit = {}

local expected_archive =
    "SDL2-2.32.10-TTF-2.24.0-MIXER-2.8.2-" ..
    "IMAGE-2.8.12-NET-2.4.0-android-arm64.zip"

local required_files = {
    "BUILD-INFO.txt",
    "FEATURES.txt",
    "include/SDL2/SDL.h",
    "include/SDL2/SDL_ttf.h",
    "include/SDL2/SDL_mixer.h",
    "include/SDL2/SDL_image.h",
    "include/SDL2/SDL_net.h",
    "lib/arm64-v8a/libSDL2.so",
    "lib/arm64-v8a/libSDL2_ttf.so",
    "lib/arm64-v8a/libSDL2_mixer.so",
    "lib/arm64-v8a/libSDL2_image.so",
    "lib/arm64-v8a/libSDL2_net.so",
    "java/org/libsdl/app/SDLActivity.java"
}

local function verify_root(root)
    for _, filename in ipairs(required_files) do
        if fs.mode(path.join(root, filename)) ~= "file" then
            error("SDL2 kit is missing " .. filename, 0)
        end
    end
end

local function read_checksum(checksum_file, archive_name)
    local contents = fs.read_file(checksum_file)
    local digest, listed_name =
        contents:match("^([0-9a-fA-F]+)%s+%*?([^%s]+)")

    if not digest or #digest ~= 64 then
        error("invalid checksum file: " .. checksum_file, 0)
    end

    if listed_name ~= archive_name then
        error(
            "checksum names " ..
            tostring(listed_name) ..
            " instead of " ..
            archive_name,
            0
        )
    end

    return digest:lower()
end

--- Return the exact supported kit archive filename.
-- @return Archive basename.
function kit.expected_archive()
    return expected_archive
end

--- Register and extract a verified SDL2 offline kit.
-- @param settings Resolved configuration.
-- @param archive Absolute or relative archive path.
-- @return Cached kit directory.
function kit.add(settings, archive)
    if fs.mode(archive) ~= "file" then
        error("kit archive not found: " .. tostring(archive), 0)
    end

    local archive_name = path.basename(archive)
    if archive_name ~= expected_archive then
        error(
            "unsupported kit archive\nexpected: " ..
            expected_archive ..
            "\nreceived: " ..
            archive_name,
            0
        )
    end

    local checksum_file = archive .. ".sha256"
    if fs.mode(checksum_file) ~= "file" then
        error("matching checksum file not found: " .. checksum_file, 0)
    end

    local expected = read_checksum(checksum_file, archive_name)
    local actual = sha256.file(archive)
    if actual ~= expected then
        error(
            "kit checksum mismatch\nexpected: " ..
            expected ..
            "\nactual:   " ..
            actual,
            0
        )
    end

    local archive_cache = path.join(settings.cache_root, "archives")
    local kit_cache = path.join(settings.cache_root, "kits")
    fs.mkdir_p(archive_cache)
    fs.mkdir_p(kit_cache)

    local cached_archive = path.join(archive_cache, archive_name)
    if fs.exists(cached_archive) then
        if sha256.file(cached_archive) ~= actual then
            error("cached archive has conflicting contents: " ..
                cached_archive, 0)
        end
    else
        fs.copy_file(archive, cached_archive)
        fs.copy_file(checksum_file, cached_archive .. ".sha256")
    end

    local bundle_name = archive_name:gsub("%.zip$", "")
    local final_root = path.join(kit_cache, bundle_name)

    if fs.exists(final_root) then
        verify_root(final_root)
    else
        local stage =
            path.join(
                settings.cache_root,
                "kit-stage-" .. tostring(os.time())
            )
        local counter = 0

        while fs.exists(stage) do
            counter = counter + 1
            stage =
                path.join(
                    settings.cache_root,
                    "kit-stage-" ..
                    tostring(os.time()) ..
                    "-" ..
                    tostring(counter)
                )
        end

        fs.mkdir_p(stage)

        local ok, message = xpcall(function()
            process.run({
                "cmake",
                "-E",
                "tar",
                "xvf",
                cached_archive
            }, stage)

            local extracted_root = path.join(stage, bundle_name)
            verify_root(extracted_root)

            local renamed, rename_message =
                os.rename(extracted_root, final_root)
            if not renamed then
                error(
                    "cannot finalize cached kit: " ..
                    tostring(rename_message),
                    0
                )
            end
        end, debug.traceback)

        if fs.exists(stage) then
            fs.remove_tree(stage)
        end

        if not ok then
            error(message, 0)
        end
    end

    local values = state.load(settings)
    values.kit_root = final_root
    values.kit_archive = cached_archive
    values.kit_sha256 = actual
    state.save(settings, values)

    return final_root
end

--- Get and validate the active kit.
-- @param settings Resolved configuration.
-- @return Kit root.
function kit.require_active(settings)
    local values = state.load(settings)

    if not values.kit_root then
        error("no SDL2 kit is registered; run sdl-pg kit add ARCHIVE", 0)
    end

    verify_root(values.kit_root)
    return values.kit_root, values
end

--- Read active-kit status.
-- @param settings Resolved configuration.
-- @return Status table.
function kit.status(settings)
    local values = state.load(settings)

    return {
        configured = values.kit_root ~= nil,
        root = values.kit_root,
        archive = values.kit_archive,
        sha256 = values.kit_sha256
    }
end

return kit
