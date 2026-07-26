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
    contents:find("Private Toolchain Sample", 1, true),
    "generated LDoc index does not contain the project title"
)

assert(
    contents:find("toolchain_sample", 1, true),
    "generated LDoc index does not contain the sample module"
)

print("Generated LDoc output: OK")
