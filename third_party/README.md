---
title: Third-Party Dependencies
tags:
  - dependencies
  - lua
---

# Third-Party Dependencies

## Lua 5.4.8

- Upstream: <https://www.lua.org/>
- Source: <https://www.lua.org/ftp/lua-5.4.8.tar.gz>
- SHA-256: `4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae`
- License: MIT

The pristine upstream archive is stored in `third_party/cache/`.
`bootstrap.lua` verifies it using a pure Lua SHA-256 implementation before
extracting it. No network connection is used by the bootstrap process.

The extracted `third_party/lua-5.4.8/` directory is generated locally and can
be removed and recreated from the verified archive.

## LuaFileSystem 1.8.0

- Upstream: <https://github.com/lunarmodules/luafilesystem>
- Tag: `v1_8_0`
- SHA-256: `16d17c788b8093f2047325343f5e9b74cccb1ea96001e45914a58bbae8932495`
- License: MIT

LuaFileSystem is the only native Lua module in the documentation-toolchain
test. It is compiled against the pinned private runtime.

## Penlight 1.14.0

- Upstream: <https://github.com/lunarmodules/Penlight>
- Tag: `1.14.0`
- SHA-256: `2387431c0e83c4189cccb35b989141a3280d735cb5d42bacf3451af9869bebf7`
- License: MIT/X11

## LDoc 1.5.0

- Upstream: <https://github.com/lunarmodules/LDoc>
- Tag: `v1.5.0`
- SHA-256: `4469cd74c8c7f51d3b9ce802d2239ba2b09d3d3a11273c3a5abdf273a0a53531`
- License: MIT

LDoc contains a built-in Lua Markdown renderer. The private toolchain uses that
renderer and does not install the obsolete external `markdown` rock.

## yyjson 0.12.0

- Upstream: <https://github.com/ibireme/yyjson>
- Tag: `0.12.0`
- Commit: `8b4a38dc994a110abaec8a400615567bd996105f`
- Source: <https://github.com/ibireme/yyjson/archive/refs/tags/0.12.0.tar.gz>
- SHA-256: `b16246f617b2a136c78d73e5e2647c6f1de1313e46678062985bdcf1f40bb75d`
- License: MIT

The pristine tag archive is stored in `third_party/cache/`. The toolchain
verifies it before extraction. Generated Android projects receive the verified
source and the upstream license so their native JSON build stays offline and
auditable.

## Related

- [Project README](../README.md)
- [Private Lua Toolchain Test](../PRIVATE-TOOLCHAIN.md)
- [Lua license](../licenses/Lua-LICENSE.txt)
- [yyjson license](../licenses/yyjson-LICENSE.txt)
