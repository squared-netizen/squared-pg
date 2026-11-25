-- sq/build/kit_{{project_name}}.lua: kit.{{project_name}}'s part of the build, for sqmake.
-- GENERATED: kit.{{project_name}} owns this file and replaces it on update.
--
-- The template's build (sq/build/template.lua) includes every
-- sq/build/kit_*.lua and adds what each returns.
return {
  id = "kit.{{project_name}}",
  srcs = {"sq_kit/{{project_name}}/{{project_name}}.cppm"},
}
