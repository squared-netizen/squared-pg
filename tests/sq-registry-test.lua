--- Validate offline SQ registration and collision-safe module composition.
-- @script sq-registry-test

local root = arg[1] or "."
package.path = table.concat({
    root .. "/lua/?.lua",
    root .. "/lua/?/init.lua",
    package.path
}, ";")

local fs = require("sdl_pg.fs")
local package_registry = require("sdl_pg.package_registry")
local path = require("sdl_pg.path")
local sq = require("squared.sq")

local work = path.join(root, "build/sq-registry-test")
if fs.exists(work) then
    fs.remove_tree(work)
end
fs.mkdir_p(work)

local function build_module(name, identifier, version, requires)
    local fixture_root = path.join(work, "fixtures", name .. "-" .. version)
    local module_directory = "modules/" .. name
    fs.write_file(
        path.join(fixture_root, "content", module_directory, "CMakeLists.txt"),
        "add_library(" .. name:gsub("-", "_") .. " INTERFACE)\n"
    )
    local archive_path =
        path.join(fixture_root, name .. "-" .. version .. ".sq")
    local report, package_error = sq.create_from_directory({
        manifest = {
            kind = "module",
            id = identifier,
            version = version,
            name = name,
            module = {
                directory = module_directory,
                cmake_target = name:gsub("-", "_"),
                requires = requires or {}
            }
        },
        content_directory = path.join(fixture_root, "content"),
        destination = archive_path
    })
    assert(report, package_error and package_error.message)
    return archive_path
end

local settings = {
    cache_root = path.join(work, "cache")
}
local application_archive = path.join(
    root,
    "build/packages/squared-application-0.6.0-dev.1.sq"
)
local application_record =
    package_registry.add(settings, application_archive)
assert(application_record.kind == "module")
assert(
    application_record.id ==
        "dev.squarednetizen.squared.application"
)
assert(
    application_record.module.cmake_target ==
        "squared_application"
)
assert(#application_record.module.requires == 0)

local graphics_archive = path.join(
    root,
    "build/packages/squared-graphics-0.6.0-dev.1.sq"
)
local graphics_record = package_registry.add(settings, graphics_archive)
assert(graphics_record.kind == "module")
assert(graphics_record.id == "dev.squarednetizen.squared.graphics")
assert(graphics_record.module.cmake_target == "squared_graphics")
assert(#graphics_record.module.requires == 0)

local math_archive = path.join(
    root,
    "build/packages/squared-math-0.6.0-dev.1.sq"
)
local math_record = package_registry.add(settings, math_archive)
assert(math_record.kind == "module")
assert(math_record.id == "dev.squarednetizen.squared.math")
assert(math_record.module.cmake_target == "squared_math")
assert(#math_record.module.requires == 0)

local graphics2d_archive = path.join(
    root,
    "build/packages/squared-graphics2d-0.6.0-dev.1.sq"
)
local graphics2d_record =
    package_registry.add(settings, graphics2d_archive)
assert(graphics2d_record.kind == "module")
assert(
    graphics2d_record.id ==
        "dev.squarednetizen.squared.graphics2d"
)
assert(graphics2d_record.module.cmake_target == "squared_graphics2d")
assert(#graphics2d_record.module.requires == 2)
assert(
    graphics2d_record.module.requires[1].id ==
        "dev.squarednetizen.squared.graphics"
)
assert(
    graphics2d_record.module.requires[2].id ==
        "dev.squarednetizen.squared.math"
)

local scene2d_archive = path.join(
    root,
    "build/packages/squared-scene2d-0.6.0-dev.1.sq"
)
local scene2d_record =
    package_registry.add(settings, scene2d_archive)
assert(scene2d_record.kind == "module")
assert(scene2d_record.id == "dev.squarednetizen.squared.scene2d")
assert(scene2d_record.module.cmake_target == "squared_scene2d")
assert(#scene2d_record.module.requires == 1)
assert(
    scene2d_record.module.requires[1].id ==
        "dev.squarednetizen.squared.graphics2d"
)

local archive = path.join(
    root,
    "build/packages/squared-time-0.6.0-dev.1.sq"
)
local record = package_registry.add(settings, archive)
assert(record.kind == "module")
assert(record.id == "dev.squarednetizen.squared.time")
assert(record.version == "0.6.0-dev.1")
assert(record.module.cmake_target == "squared_time")
assert(#record.module.requires == 0)
assert(fs.mode(record.archive) == "file")
assert(fs.mode(record.content_root) == "directory")

local repeated_record = package_registry.add(settings, archive)
assert(repeated_record.already_registered)
assert(repeated_record.content_digest == record.content_digest)

local data_archive = path.join(
    root,
    "build/packages/squared-data-0.6.0-dev.1.sq"
)
local data_record = package_registry.add(settings, data_archive)
assert(data_record.kind == "module")
assert(data_record.id == "dev.squarednetizen.squared.data")
assert(data_record.module.cmake_target == "squared_data")
assert(#data_record.module.requires == 0)

local messaging_archive = path.join(
    root,
    "build/packages/squared-messaging-0.6.0-dev.1.sq"
)
local messaging_record =
    package_registry.add(settings, messaging_archive)
assert(messaging_record.kind == "module")
assert(messaging_record.id == "dev.squarednetizen.squared.messaging")
assert(messaging_record.module.cmake_target == "squared_messaging")
assert(#messaging_record.module.requires == 2)
assert(
    messaging_record.module.requires[1].id ==
        "dev.squarednetizen.squared.data"
)
assert(
    messaging_record.module.requires[2].id ==
        "dev.squarednetizen.squared.time"
)

local template_archive = path.join(
    root,
    "build/packages/squared-android-template-0.6.0-dev.14.sq"
)
local template_record =
    package_registry.add(settings, template_archive)
assert(template_record.kind == "template")
assert(template_record.template.profile == "android_sdl2_lua")
assert(template_record.template.directory == "template")
assert(#template_record.template.fields == 4)
assert(#template_record.template.requires == 3)
assert(
    template_record.template.requires[1].id ==
        "dev.squarednetizen.squared.application"
)
assert(
    template_record.template.requires[2].id ==
        "dev.squarednetizen.squared.scene2d"
)
assert(
    template_record.template.requires[3].id ==
        "dev.squarednetizen.squared.messaging"
)
assert(
    select(1, package_registry.require_template(
        settings,
        template_record.id,
        template_record.version
    )).id == template_record.id
)

local records = package_registry.list(settings)
assert(#records == 9)

local project = path.join(work, "project")
fs.mkdir_p(path.join(project, "docs"))
package_registry.apply_module(
    settings,
    project,
    application_record.id,
    application_record.version
)
package_registry.apply_module(
    settings,
    project,
    graphics_record.id,
    graphics_record.version
)
package_registry.apply_module(
    settings,
    project,
    math_record.id,
    math_record.version
)
package_registry.apply_module(
    settings,
    project,
    graphics2d_record.id,
    graphics2d_record.version
)
package_registry.apply_module(
    settings,
    project,
    scene2d_record.id,
    scene2d_record.version
)
package_registry.apply_module(
    settings,
    project,
    record.id,
    record.version
)
package_registry.apply_module(
    settings,
    project,
    data_record.id,
    data_record.version
)
package_registry.apply_module(
    settings,
    project,
    messaging_record.id,
    messaging_record.version
)
assert(fs.mode(path.join(
    project,
    "modules/squared-time/CMakeLists.txt"
)) == "file")
assert(fs.mode(path.join(project, "docs/Time.md")) == "file")
assert(fs.mode(path.join(
    project,
    "modules/squared-data/CMakeLists.txt"
)) == "file")
assert(fs.mode(path.join(
    project,
    "modules/squared-data/include/squared/data/json.hpp"
)) == "file")
assert(fs.mode(path.join(
    project,
    "modules/squared-data/src/json.cpp"
)) == "file")
assert(fs.mode(path.join(
    project,
    "third_party/yyjson-0.12.0/src/yyjson.c"
)) == "file")
assert(fs.mode(path.join(
    project,
    "third_party/yyjson-0.12.0/src/yyjson.h"
)) == "file")
assert(fs.mode(path.join(
    project,
    "licenses/yyjson-LICENSE.txt"
)) == "file")
assert(fs.mode(path.join(
    project,
    "modules/squared-messaging/CMakeLists.txt"
)) == "file")
assert(fs.mode(path.join(
    project,
    "modules/squared-messaging/src/message_dispatcher.cpp"
)) == "file")
assert(fs.mode(path.join(
    project,
    "modules/squared-messaging/include/squared/messaging/telegram.hpp"
)) == "file")
assert(fs.mode(path.join(project, "docs/Messaging.md")) == "file")
assert(fs.mode(path.join(
    project,
    "modules/squared-application/CMakeLists.txt"
)) == "file")
assert(fs.mode(path.join(
    project,
    "modules/squared-application/include/squared/application/application.hpp"
)) == "file")
assert(fs.mode(path.join(
    project,
    "modules/squared-application/include/squared/application/event.hpp"
)) == "file")
assert(
    fs.mode(path.join(project, "docs/Squared-Application.md")) ==
        "file"
)
assert(fs.mode(path.join(
    project,
    "modules/squared-math/CMakeLists.txt"
)) == "file")
assert(fs.mode(path.join(
    project,
    "modules/squared-math/include/squared/math/matrix4.hpp"
)) == "file")
assert(fs.mode(path.join(
    project,
    "modules/squared-math/include/squared/math/vector2.hpp"
)) == "file")
assert(fs.mode(path.join(
    project,
    "modules/squared-math/src/matrix4.cpp"
)) == "file")
assert(fs.mode(path.join(project, "docs/Math.md")) == "file")
assert(fs.mode(path.join(
    project,
    "modules/squared-graphics/CMakeLists.txt"
)) == "file")
assert(fs.mode(path.join(
    project,
    "modules/squared-graphics/include/squared/graphics/color.hpp"
)) == "file")
assert(fs.mode(path.join(
    project,
    "modules/squared-graphics/include/squared/graphics/context.hpp"
)) == "file")
assert(fs.mode(path.join(
    project,
    "modules/squared-graphics/src/context.cpp"
)) == "file")
assert(fs.mode(path.join(project, "docs/Graphics.md")) == "file")
assert(fs.mode(path.join(
    project,
    "modules/squared-graphics2d/CMakeLists.txt"
)) == "file")
assert(fs.mode(path.join(
    project,
    "modules/squared-graphics2d/src/sprite_batch.cpp"
)) == "file")
assert(fs.mode(path.join(
    project,
    "modules/squared-graphics2d/include/squared/graphics2d/texture_atlas.hpp"
)) == "file")
assert(fs.mode(path.join(project, "docs/Graphics2D.md")) == "file")
assert(fs.mode(path.join(
    project,
    "modules/squared-scene2d/CMakeLists.txt"
)) == "file")
assert(fs.mode(path.join(
    project,
    "modules/squared-scene2d/src/group.cpp"
)) == "file")
assert(fs.mode(path.join(
    project,
    "modules/squared-scene2d/include/squared/scene2d/stage.hpp"
)) == "file")
assert(fs.mode(path.join(project, "docs/Scene2D.md")) == "file")

local repeated = pcall(function()
    package_registry.apply_module(
        settings,
        project,
        record.id,
        record.version
    )
end)
assert(not repeated)

local missing_settings = {
    cache_root = path.join(work, "missing-dependency-cache")
}
local missing_template =
    package_registry.add(missing_settings, template_archive)
local dependency_preflight = pcall(function()
    package_registry.require_template(
        missing_settings,
        missing_template.id,
        missing_template.version
    )
end)
assert(not dependency_preflight)

local graph_settings = {
    cache_root = path.join(work, "graph-cache")
}
local graph_packages = {
    build_module(
        "graph-leaf",
        "dev.squarednetizen.graph.leaf",
        "1.0.0"
    ),
    build_module(
        "graph-left",
        "dev.squarednetizen.graph.left",
        "1.0.0",
        {
            {
                kind = "module",
                id = "dev.squarednetizen.graph.leaf",
                version = "1.0.0"
            }
        }
    ),
    build_module(
        "graph-right",
        "dev.squarednetizen.graph.right",
        "1.0.0",
        {
            {
                kind = "module",
                id = "dev.squarednetizen.graph.leaf",
                version = "1.0.0"
            }
        }
    ),
    build_module(
        "graph-root",
        "dev.squarednetizen.graph.root",
        "1.0.0",
        {
            {
                kind = "module",
                id = "dev.squarednetizen.graph.right",
                version = "1.0.0"
            },
            {
                kind = "module",
                id = "dev.squarednetizen.graph.left",
                version = "1.0.0"
            }
        }
    )
}
for _, graph_archive in ipairs(graph_packages) do
    package_registry.add(graph_settings, graph_archive)
end
local graph_root, graph_order = package_registry.resolve(
    graph_settings,
    "dev.squarednetizen.graph.root",
    "1.0.0"
)
assert(graph_root.id == "dev.squarednetizen.graph.root")
assert(#graph_order == 3)
assert(graph_order[1].id == "dev.squarednetizen.graph.leaf")
assert(graph_order[2].id == "dev.squarednetizen.graph.left")
assert(graph_order[3].id == "dev.squarednetizen.graph.right")
local graph_project = path.join(work, "graph-project")
fs.mkdir_p(graph_project)
for _, dependency in ipairs(graph_order) do
    package_registry.apply_module(
        graph_settings,
        graph_project,
        dependency.id,
        dependency.version
    )
end
package_registry.apply_module(
    graph_settings,
    graph_project,
    graph_root.id,
    graph_root.version
)
for _, module_name in ipairs({
    "graph-leaf",
    "graph-left",
    "graph-right",
    "graph-root"
}) do
    assert(fs.mode(path.join(
        graph_project,
        "modules",
        module_name,
        "CMakeLists.txt"
    )) == "file")
end

local cycle_settings = {
    cache_root = path.join(work, "cycle-cache")
}
for _, cycle_archive in ipairs({
    build_module(
        "cycle-a",
        "dev.squarednetizen.cycle.a",
        "1.0.0",
        {
            {
                kind = "module",
                id = "dev.squarednetizen.cycle.b",
                version = "1.0.0"
            }
        }
    ),
    build_module(
        "cycle-b",
        "dev.squarednetizen.cycle.b",
        "1.0.0",
        {
            {
                kind = "module",
                id = "dev.squarednetizen.cycle.a",
                version = "1.0.0"
            }
        }
    )
}) do
    package_registry.add(cycle_settings, cycle_archive)
end
local cycle_ok, cycle_error = pcall(function()
    package_registry.resolve(
        cycle_settings,
        "dev.squarednetizen.cycle.a",
        "1.0.0"
    )
end)
assert(not cycle_ok)
assert(tostring(cycle_error):find("dependency cycle", 1, true))

local unresolved_settings = {
    cache_root = path.join(work, "unresolved-cache")
}
local unresolved_archive = build_module(
    "unresolved-root",
    "dev.squarednetizen.unresolved.root",
    "1.0.0",
    {
        {
            kind = "module",
            id = "dev.squarednetizen.unresolved.missing",
            version = "1.0.0"
        }
    }
)
package_registry.add(unresolved_settings, unresolved_archive)
local unresolved_ok, unresolved_error = pcall(function()
    package_registry.resolve(
        unresolved_settings,
        "dev.squarednetizen.unresolved.root",
        "1.0.0"
    )
end)
assert(not unresolved_ok)
assert(tostring(unresolved_error):find(
    "dev.squarednetizen.unresolved.missing@1.0.0",
    1,
    true
))
assert(tostring(unresolved_error):find("required by", 1, true))

local conflict_settings = {
    cache_root = path.join(work, "conflict-cache")
}
for _, conflict_archive in ipairs({
    build_module(
        "common-v1",
        "dev.squarednetizen.conflict.common",
        "1.0.0"
    ),
    build_module(
        "common-v2",
        "dev.squarednetizen.conflict.common",
        "2.0.0"
    ),
    build_module(
        "conflict-left",
        "dev.squarednetizen.conflict.left",
        "1.0.0",
        {
            {
                kind = "module",
                id = "dev.squarednetizen.conflict.common",
                version = "1.0.0"
            }
        }
    ),
    build_module(
        "conflict-right",
        "dev.squarednetizen.conflict.right",
        "1.0.0",
        {
            {
                kind = "module",
                id = "dev.squarednetizen.conflict.common",
                version = "2.0.0"
            }
        }
    ),
    build_module(
        "conflict-root",
        "dev.squarednetizen.conflict.root",
        "1.0.0",
        {
            {
                kind = "module",
                id = "dev.squarednetizen.conflict.left",
                version = "1.0.0"
            },
            {
                kind = "module",
                id = "dev.squarednetizen.conflict.right",
                version = "1.0.0"
            }
        }
    )
}) do
    package_registry.add(conflict_settings, conflict_archive)
end
local conflict_ok, conflict_error = pcall(function()
    package_registry.resolve(
        conflict_settings,
        "dev.squarednetizen.conflict.root",
        "1.0.0"
    )
end)
assert(not conflict_ok)
assert(tostring(conflict_error):find(
    "dependency version conflict",
    1,
    true
))

fs.remove_tree(work)
print("SQ recursive dependency resolution and composition: OK")
