# kit.{{project_name}}

Everything under `tree/` is copied into a workspace made with
`kit.{{project_name}}`, at the same path from that workspace's root: so
`tree/sq/build/kit_{{project_name}}.lua` lands as
`sq/build/kit_{{project_name}}.lua`, which the template's build includes.

This file is beside `tree/`, not in it: it documents the kit without being
copied. Name what you add after the kit, as these are, so two kits never
add the same path.

`sq/build/kit_{{project_name}}.lua` returns what the kit adds to the build:
`{id, flags, link, srcs}`. Its module is a source; add `link` for a library
the kit wraps, `flags` for its headers (`-idirafter`, never `-I`, for a
sysroot of its own).
