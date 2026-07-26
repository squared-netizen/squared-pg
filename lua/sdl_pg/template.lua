--- Text-template rendering for generated projects.
-- @module sdl_pg.template

local fs = require("sdl_pg.fs")
local path = require("sdl_pg.path")

local template = {}

--- Render `{{NAME}}` placeholders.
-- @param contents Template text.
-- @param variables Placeholder-value table.
-- @return Rendered text.
function template.render(contents, variables)
    return (contents:gsub("{{([A-Z0-9_]+)}}", function(key)
        local value = variables[key]
        if value == nil then
            error("template variable is not defined: " .. key, 0)
        end

        return tostring(value)
    end))
end

local function render_tree(source, destination, variables)
    fs.mkdir_p(destination)

    for _, entry in ipairs(fs.entries(source)) do
        local rendered_entry = template.render(entry, variables)
        local source_entry = path.join(source, entry)
        local destination_entry = path.join(destination, rendered_entry)
        local mode = fs.mode(source_entry)

        if mode == "directory" then
            render_tree(source_entry, destination_entry, variables)
        elseif mode == "file" then
            fs.write_file(
                destination_entry,
                template.render(fs.read_file(source_entry), variables)
            )
        else
            error("template contains unsupported entry: " ..
                source_entry, 0)
        end
    end
end

--- Render an entire text-template directory.
-- @param source Template root.
-- @param destination New destination root.
-- @param variables Placeholder-value table.
function template.render_tree(source, destination, variables)
    if fs.mode(source) ~= "directory" then
        error("template directory not found: " .. source, 0)
    end

    render_tree(source, destination, variables)
end

return template
