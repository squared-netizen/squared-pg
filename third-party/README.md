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

## miniz 3.1.2

- Upstream: <https://github.com/richgel999/miniz>
- Tag: `3.1.2`
- Source: <https://github.com/richgel999/miniz/archive/refs/tags/3.1.2.tar.gz>
- SHA-256: `98468f8924934b723276680f85238b6c78bf1f8b49b4459cc9b7214a20e2e9fb`
- License: MIT

The pristine tag archive is stored in `third_party/cache/`. The toolchain
verifies and extracts it without network access. miniz remains a private
static implementation dependency of `squared_sq_core`; no miniz types appear
in the public Squared API.

## Related

- [Project README](../README.md)
- [Private Lua Toolchain Test](../PRIVATE-TOOLCHAIN.md)
- [Lua license](../licenses/Lua-LICENSE.txt)
- [yyjson license](../licenses/yyjson-LICENSE.txt)
- [miniz license](../licenses/miniz-LICENSE.txt)

## SFML 3.1.0

- Upstream: <https://www.sfml-dev.org/>
- Repository: <https://github.com/SFML/SFML>
- Tag: `3.1.0`
- License: zlib/png

Vendored as extracted source in `third-party/SFML-3.1.0/`. Consumed by
`kit.sfml`, which ships the built static archives; see
[[developer/resources/sfml-android]] for how they are produced.

**Trimmed.** `test/`, `examples/` and `doc/` are removed, along with
`extlibs/headers/mingw/` and `extlibs/headers/wepoll/`, which are Windows-only.
The remaining `extlibs/headers/` are all reachable: `cpp-unicodelib` and `glad`
for Graphics, `miniaudio` and `dr_mp3` for Audio, `stb_image` and `qoi` for
image loading, `vulkan` because `Vulkan.cpp` compiles unconditionally.

**No SHA-256.** The tree was taken from a git checkout rather than a release
archive, so there is no upstream digest to verify against. Re-fetching means
cloning the tag again.

## SFML's dependencies

In `third-party/sfml-deps/`, needed by SFML's Graphics and Audio modules.

**The versions are SFML's own, not chosen independently.** Each is the tag
named in SFML's `FetchContent_Declare` blocks
(`src/SFML/Graphics/CMakeLists.txt`, `src/SFML/Audio/CMakeLists.txt`), so the
combination is one upstream tests. Changing SFML's version means re-reading
those blocks, not bumping these by preference.

| Library | Tag | Used by | License |
|---|---|---|---|
| FreeType | `VER-2-14-3` | Graphics — glyph rasterisation | FTL / GPLv2 |
| HarfBuzz | `14.1.0` | Graphics — text shaping | MIT |
| SheenBidi | `v3.0.0` | Graphics — bidirectional text | Apache-2.0 |
| Ogg | `v1.3.6` | Audio — container | BSD-3-Clause |
| Vorbis | `v1.3.7` | Audio — codec | BSD-3-Clause |
| FLAC | `1.5.0` | Audio — codec | BSD-3-Clause / GPLv2 |

Each was cloned at its tag with `.git` removed. **No SHA-256 for the same
reason as SFML.**

### These trees are patched

SFML applies a `PATCH_COMMAND` to every one of these — edits to their
`CMakeLists.txt` that are not cosmetic. FreeType's breaks a FreeType/HarfBuzz
cycle; HarfBuzz's adds a missing `PUBLIC`; SheenBidi's makes it an OBJECT
library; ogg, vorbis and flac get their `cmake_minimum_required` ceilings
raised and install rules removed.

Feeding the build from these vendored trees uses
`FETCHCONTENT_SOURCE_DIR_<NAME>`, and **that override skips the update stage,
which is where `PATCH_COMMAND` lives.** So the patches are applied by hand and
the patched trees committed:

```sh
tools/build-sfml.sh --patch
```

Idempotent, and `build-sfml.sh` asserts each patch's post-condition before
every build — these are version-pinned literal matches, and a patch that
silently does nothing leaves a tree indistinguishable from an unpatched one.

**Six files therefore differ from their upstream tag**: each dependency's
`CMakeLists.txt`, plus `vorbis/lib/CMakeLists.txt`. That is deliberate, not
corruption.

### Trimmed

`harfbuzz/test/` (87 MB of font fixtures), `harfbuzz/docs/`,
`SheenBidi/Tools/Unicode/` (19 MB of Unicode Character Database),
`freetype/docs/`, `freetype/tests/`, and the `doc/`, `examples/` and `test/`
directories of ogg, vorbis and flac. 147 MB to 34 MB.

SheenBidi's `Tools/Generator`, `Tools/Parser` and `Tests/` are kept: they are
small, and they are what would regenerate the lookup tables if a future Unicode
version mattered. Only `Tools/Unicode`, the input data, was removed — safe
because everything referencing it sits behind `BUILD_GENERATOR` or
`BUILD_TESTS`, neither of which SFML sets.

### Not vendored

mbedTLS and libssh2, which SFML's Network module needs. Network is off:
`SFML_BUILD_NETWORK=OFF`. Both also carry patches, one of which fixes libssh2
returning a pointer to stack memory, so enabling Network means vendoring and
patching them too.
