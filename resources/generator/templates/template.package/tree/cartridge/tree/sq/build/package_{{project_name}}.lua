-- sq/build/package_{{project_name}}.lua: package.{{project_name}}'s part of the build, for
-- sqmake. GENERATED: package.{{project_name}} owns this file and replaces it on update.
--
-- The template's build (sq/build/template.lua) includes every
-- sq/build/package_*.lua, after the kits', and adds what each returns.
return {
  id = "package.{{project_name}}",
  srcs = {"sq_pkg/{{project_name}}/{{project_name}}.cppm"},
}
