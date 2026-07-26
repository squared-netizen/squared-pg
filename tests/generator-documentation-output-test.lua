--- Verify that LDoc generated the public generator API.
-- @script generator-documentation-output-test

local lfs = require("lfs")

local output_directory =
    assert(arg[1], "documentation output directory is required")
local index_path = output_directory .. "/index.html"

local file, open_error = io.open(index_path, "rb")

assert(
    file,
    "LDoc did not generate " ..
        index_path ..
        ": " ..
        tostring(open_error)
)

local contents = assert(file:read("*a"))
file:close()

local documentation = {index_path, "\n", contents}

local function collect_html(directory)
    for entry in lfs.dir(directory) do
        if entry ~= "." and entry ~= ".." then
            local candidate = directory .. "/" .. entry
            local mode = lfs.attributes(candidate, "mode")

            if mode == "directory" then
                collect_html(candidate)
            elseif mode == "file" and entry:match("%.html$") then
                local generated = assert(io.open(candidate, "rb"))
                documentation[#documentation + 1] = "\n"
                documentation[#documentation + 1] = candidate
                documentation[#documentation + 1] = "\n"
                documentation[#documentation + 1] =
                    assert(generated:read("*a"))
                generated:close()
            end
        end
    end
end

collect_html(output_directory)
local generated_documentation = table.concat(documentation)

assert(
    contents:find("SDL Project Generator Lua API", 1, true),
    "generated LDoc index does not contain the project title"
)

assert(
    generated_documentation:find("sdl_pg.project", 1, true),
    "generated LDoc output does not contain sdl_pg.project"
)

assert(
    generated_documentation:find("sdl_pg.kit", 1, true),
    "generated LDoc output does not contain sdl_pg.kit"
)

assert(
    generated_documentation:find("sdl_pg.android", 1, true),
    "generated LDoc output does not contain sdl_pg.android"
)

assert(
    generated_documentation:find("sdl_pg.docs", 1, true),
    "generated LDoc output does not contain sdl_pg.docs"
)

print("Generated SDL Project Generator LDoc output: OK")
