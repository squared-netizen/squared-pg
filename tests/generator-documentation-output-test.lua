--- Verify that LDoc generated the public generator API.
-- @script generator-documentation-output-test

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

assert(
    contents:find("SDL Project Generator Lua API", 1, true),
    "generated LDoc index does not contain the project title"
)

assert(
    contents:find("sdl_pg.project", 1, true),
    "generated LDoc index does not contain sdl_pg.project"
)

assert(
    contents:find("sdl_pg.kit", 1, true),
    "generated LDoc index does not contain sdl_pg.kit"
)

assert(
    contents:find("sdl_pg.android", 1, true),
    "generated LDoc index does not contain sdl_pg.android"
)

print("Generated SDL Project Generator LDoc output: OK")
