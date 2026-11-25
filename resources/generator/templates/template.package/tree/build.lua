-- build.lua: package.{{project_name}}'s build, for sqmake. This file is yours; what the
-- template knows is in sq/build/cartridge.lua (generated).
--
--   sqmake          pack cartridge/ into dist/package.{{project_name}}.sq (and check it)
--   sqmake install  install it, exploded, where sqpg and square find it
cartridge = {kind = "package", id = "package.{{project_name}}"}
include("sq/build/cartridge.lua")
