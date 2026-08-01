--- Build the repository-owned SQ packages with the private native binding.
-- @script build-packages

local root = arg[1] or "."
package.path = table.concat({
    root .. "/lua/?.lua",
    root .. "/lua/?/init.lua",
    package.path
}, ";")

local fs = require("sdl_pg.fs")
local path = require("sdl_pg.path")
local sq = require("squared.sq")

local output_directory = path.join(root, "build/packages")
fs.mkdir_p(output_directory)

local definitions = {
    {
        source = "packages/squared-application",
        archive = "squared-application-0.6.0-dev.1.sq",
        kind = "module",
        label = "Squared Application"
    },
    {
        source = "packages/squared-graphics",
        archive = "squared-graphics-0.6.0-dev.1.sq",
        kind = "module",
        label = "Squared Graphics"
    },
    {
        source = "packages/squared-graphics2d",
        archive = "squared-graphics2d-0.6.0-dev.1.sq",
        kind = "module",
        label = "Squared Graphics2D"
    },
    {
        source = "packages/squared-math",
        archive = "squared-math-0.6.0-dev.1.sq",
        kind = "module",
        label = "Squared Math"
    },
    {
        source = "packages/squared-scene2d",
        archive = "squared-scene2d-0.6.0-dev.1.sq",
        kind = "module",
        label = "Squared Scene2D"
    },
    {
        source = "packages/squared-time",
        archive = "squared-time-0.6.0-dev.1.sq",
        kind = "module",
        label = "Squared Time"
    },
    {
        source = "packages/squared-data",
        archive = "squared-data-0.6.0-dev.1.sq",
        kind = "module",
        label = "Squared Data"
    },
    {
        source = "packages/squared-messaging",
        archive = "squared-messaging-0.6.0-dev.1.sq",
        kind = "module",
        label = "Squared Messaging"
    },
    {
        source = "packages/squared-android-template",
        archive = "squared-android-template-0.6.0-dev.14.sq",
        kind = "template",
        label = "Squared Android template"
    }
}

for _, definition in ipairs(definitions) do
    local source = path.join(root, definition.source)
    local output = path.join(output_directory, definition.archive)
    if fs.exists(output) then
        fs.remove_tree(output)
    end

    local report, package_error = sq.create_from_directory({
        manifest_json = fs.read_file(path.join(source, "manifest.json")),
        content_directory = path.join(source, "content"),
        destination = output
    })
    assert(report, package_error and package_error.message)

    local package, open_error = sq.open(output)
    assert(package, open_error and open_error.message)
    local validation, validation_error = package:validate({
        expected_kind = definition.kind
    })
    assert(validation, validation_error and validation_error.message)
    assert(validation.valid)
    package:close()

    print("Built SQ package: " .. output)
    print(
        definition.label ..
        " content digest: " ..
        validation.content_digest
    )
end
