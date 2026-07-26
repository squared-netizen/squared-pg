--- Native filesystem operations used by SDL Project Generator.
-- @module sdl_pg.fs

local lfs = require("lfs")
local path = require("sdl_pg.path")

local fs = {}
local temporary_counter = 0

--- Return the filesystem mode for a path.
-- @param value Filesystem path.
-- @return Mode string or nil.
function fs.mode(value)
    local attributes = lfs.symlinkattributes(value)
    return attributes and attributes.mode or nil
end

--- Return whether a path exists.
-- @param value Filesystem path.
-- @return Boolean.
function fs.exists(value)
    return fs.mode(value) ~= nil
end

--- Create a directory and all missing parents.
-- @param value Directory path.
function fs.mkdir_p(value)
    local current = value:sub(1, 1) == "/" and "/" or ""

    for component in value:gmatch("[^/]+") do
        if component == ".." then
            error("mkdir_p refuses '..' path components", 0)
        elseif component ~= "." then
            current = path.join(current, component)
            local mode = fs.mode(current)

            if not mode then
                local ok, message = lfs.mkdir(current)

                if not ok then
                    error(
                        "cannot create directory " ..
                        current ..
                        ": " ..
                        tostring(message),
                        0
                    )
                end
            elseif mode ~= "directory" then
                error(current .. " exists and is not a directory", 0)
            end
        end
    end
end

--- Read an entire binary or text file.
-- @param filename File path.
-- @return File contents.
function fs.read_file(filename)
    local file, message = io.open(filename, "rb")

    if not file then
        error("cannot open " .. filename .. ": " .. tostring(message), 0)
    end

    local contents = file:read("*a")
    file:close()
    return contents
end

--- Write a complete file, creating parent directories.
-- @param filename Destination path.
-- @param contents String contents.
function fs.write_file(filename, contents)
    fs.mkdir_p(path.dirname(filename))
    local file, message = io.open(filename, "wb")

    if not file then
        error("cannot write " .. filename .. ": " .. tostring(message), 0)
    end

    local ok, write_message = file:write(contents)
    local close_ok, close_message = file:close()

    if not ok then
        error(
            "cannot write " ..
            filename ..
            ": " ..
            tostring(write_message),
            0
        )
    end

    if not close_ok then
        error(
            "cannot close " ..
            filename ..
            ": " ..
            tostring(close_message),
            0
        )
    end
end

--- Copy one regular file without invoking a shell command.
-- @param source Source file.
-- @param destination Destination file.
function fs.copy_file(source, destination)
    if fs.mode(source) ~= "file" then
        error("source is not a regular file: " .. source, 0)
    end

    fs.mkdir_p(path.dirname(destination))

    local input, input_message = io.open(source, "rb")
    if not input then
        error("cannot open " .. source .. ": " .. tostring(input_message), 0)
    end

    local output, output_message = io.open(destination, "wb")
    if not output then
        input:close()
        error(
            "cannot create " ..
            destination ..
            ": " ..
            tostring(output_message),
            0
        )
    end

    while true do
        local block = input:read(1024 * 128)
        if not block then
            break
        end

        local ok, message = output:write(block)
        if not ok then
            input:close()
            output:close()
            error(
                "cannot copy to " ..
                destination ..
                ": " ..
                tostring(message),
                0
            )
        end
    end

    input:close()
    output:close()
end

local function sorted_entries(directory)
    local entries = {}

    for entry in lfs.dir(directory) do
        if entry ~= "." and entry ~= ".." then
            entries[#entries + 1] = entry
        end
    end

    table.sort(entries)
    return entries
end

--- Return sorted child names for a directory.
-- @param directory Directory path.
-- @return Sorted array of basenames.
function fs.entries(directory)
    if fs.mode(directory) ~= "directory" then
        error("directory not found: " .. directory, 0)
    end

    return sorted_entries(directory)
end

local function copy_tree(source, destination, excluded_names)
    fs.mkdir_p(destination)

    for _, entry in ipairs(sorted_entries(source)) do
        if not excluded_names or not excluded_names[entry] then
            local source_entry = path.join(source, entry)
            local destination_entry = path.join(destination, entry)
            local mode = fs.mode(source_entry)

            if mode == "directory" then
                copy_tree(source_entry, destination_entry, excluded_names)
            elseif mode == "file" then
                fs.copy_file(source_entry, destination_entry)
            elseif mode == "link" then
                error("symbolic links are not copied: " .. source_entry, 0)
            else
                error(
                    "special filesystem entry is not copied: " ..
                    source_entry,
                    0
                )
            end
        end
    end
end

--- Remove a tree.
-- This is intended for transaction rollback and isolated tests.
-- @param value Directory or regular-file path.
function fs.remove_tree(value)
    local mode = fs.mode(value)

    if not mode then
        return
    elseif mode == "directory" then
        for _, entry in ipairs(sorted_entries(value)) do
            fs.remove_tree(path.join(value, entry))
        end

        local ok, message = lfs.rmdir(value)
        if not ok then
            error(
                "cannot remove directory " ..
                value ..
                ": " ..
                tostring(message),
                0
            )
        end
    elseif mode == "file" or mode == "link" then
        local ok, message = os.remove(value)
        if not ok then
            error(
                "cannot remove file " ..
                value ..
                ": " ..
                tostring(message),
                0
            )
        end
    else
        error("refusing to remove special entry: " .. value, 0)
    end
end

local function temporary_sibling(destination)
    repeat
        temporary_counter = temporary_counter + 1
        local candidate =
            destination ..
            ".sdl-pg-tmp-" ..
            tostring(os.time()) ..
            "-" ..
            tostring(temporary_counter)

        if not fs.exists(candidate) then
            return candidate
        end
    until false
end

--- Copy a directory transactionally to a new destination.
-- The source remains intact and the destination must not exist.
-- @param source Source directory.
-- @param destination Destination directory.
-- @param[opt] excluded_names Set of entry basenames to omit.
function fs.copy_tree_transactional(source, destination, excluded_names)
    if fs.mode(source) ~= "directory" then
        error("source directory not found: " .. source, 0)
    end

    if fs.exists(destination) then
        error("destination already exists: " .. destination, 0)
    end

    fs.mkdir_p(path.dirname(destination))
    local temporary = temporary_sibling(destination)

    local ok, message = xpcall(function()
        copy_tree(source, temporary, excluded_names)
    end, debug.traceback)

    if not ok then
        if fs.exists(temporary) then
            fs.remove_tree(temporary)
        end
        error(message, 0)
    end

    local renamed, rename_message = os.rename(temporary, destination)

    if not renamed then
        fs.remove_tree(temporary)
        error(
            "cannot finalize destination " ..
            destination ..
            ": " ..
            tostring(rename_message),
            0
        )
    end
end

return fs
