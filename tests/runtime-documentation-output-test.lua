--- Verify LDoc output for the generated application runtime.
-- @script runtime-documentation-output-test

local lfs = require("lfs")

local output_directory =
    assert(arg[1], "runtime documentation output directory is required")
local documentation = {}

local function collect_html(directory)
    for entry in lfs.dir(directory) do
        if entry ~= "." and entry ~= ".." then
            local candidate = directory .. "/" .. entry
            local mode = lfs.attributes(candidate, "mode")

            if mode == "directory" then
                collect_html(candidate)
            elseif mode == "file" and entry:match("%.html$") then
                local generated = assert(io.open(candidate, "rb"))
                documentation[#documentation + 1] = candidate
                documentation[#documentation + 1] = "\n"
                documentation[#documentation + 1] =
                    assert(generated:read("*a"))
                documentation[#documentation + 1] = "\n"
                generated:close()
            end
        end
    end
end

collect_html(output_directory)
local generated_documentation = table.concat(documentation)

assert(
    generated_documentation:find(
        "Generated Application Lua API",
        1,
        true
    ),
    "generated runtime LDoc output does not contain the project title"
)

for _, module_name in ipairs({
    "runtime.module_loader",
    "runtime.plugin_manager",
    "plugins.diagnostics",
    "plugins.orbit",
    "plugins.orbit.palette"
}) do
    assert(
        generated_documentation:find(module_name, 1, true),
        "generated runtime LDoc output does not contain " .. module_name
    )
end

print("Generated application runtime LDoc output: OK")
