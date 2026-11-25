# template.{{project_name}}

A workspace whose deliverable is a template: the exploded cartridge in
`cartridge/`, not a program.

- Work in `cartridge/`: `SQ-INF/manifest.json` says what it is, `tree/` is
  what it adds to a workspace (paths relative to that workspace's root).
- `sqmake` packs it into `dist/` with sqdrive; a manifest sqdrive refuses
  fails the build. Run it before saying a change works.
- `sqmake install` puts it, exploded, where sqpg and square find it.
- `build.lua` is yours; `sq/build/cartridge.lua` is the template's.
- Nothing here needs the network.
