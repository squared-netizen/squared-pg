# package.{{project_name}}

Everything under `tree/` is copied into a workspace made with
`package.{{project_name}}`, at the same path from that workspace's root:
`tree/sq/build/package_{{project_name}}.lua` lands as
`sq/build/package_{{project_name}}.lua`, which the template's build includes
after the kits'.

This file is beside `tree/`, not in it: it documents the package without
being copied. Name what you add after the package, as these are.

`sq/build/package_{{project_name}}.lua` returns what the package adds to the
build: `{id, flags, link, srcs}`. A module of it in `srcs` can be imported by
the app and by its tests.

The ownership is `shared`: a workspace may change the package's code, and an
update says where it and the package differ rather than overwriting it.
