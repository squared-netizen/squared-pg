---
title: Private Lua Toolchain Test
aliases:
  - Private Lua toolchain
tags:
  - ldoc
  - lua
  - offline
  - testing
status: experimental
---

# Private Lua Toolchain Test

This private toolchain supplies the modules used by Squared Project Generator and
its documentation command.

Included dependency archives:

| Dependency | Version | Purpose |
| --- | --- | --- |
| Lua | 5.4.8 | Private interpreter and embedded runtime |
| LuaFileSystem | 1.8.0 | Native filesystem primitives |
| Penlight | 1.14.0 | Lua utility modules used by LDoc |
| LDoc | 1.5.0 | Lua API documentation |
| yyjson | 0.12.0 | Strict native JSON backend for `squared::data` |
| miniz | 3.1.2 | Private Stored, Deflate, and ZIP64 backend for `.sq` |

LDoc 1.5.0 contains its own Lua Markdown renderer. The test deliberately uses
that renderer, so no obsolete external `markdown` rock is required.

## Requirements

```bash
pkg install lua54 clang cmake ninja
```

## Run the complete test

```bash
cd "$HOME/sandbox/squared-pg"
lua5.4 toolchain.lua
```

If the Termux package exposes Lua 5.4 as `lua`:

```bash
lua toolchain.lua
```

No dependency is downloaded. Every source archive is bundled and verified
before extraction.

The CTest pass also compiles and runs the public `squared::data` JSON wrapper
from the Squared Data package source against pinned yyjson. It checks strict
parsing, duplicate-key and resource limits, number-type preservation,
deterministic writing, and UTF-8 round trips.

The native `.sq` pass creates, validates, deterministically rewrites, and
transactionally extracts a module archive. Doxygen checks the public C++ API,
and the private Lua test verifies the thin `squared.sq` binding. The toolchain
then builds `build/packages/squared-application-0.6.0-dev.1.sq`,
`build/packages/squared-graphics-0.6.0-dev.1.sq`,
`build/packages/squared-graphics2d-0.6.0-dev.1.sq`,
`build/packages/squared-scene2d-0.6.0-dev.1.sq`,
`build/packages/squared-math-0.6.0-dev.1.sq`,
`build/packages/squared-time-0.6.0-dev.1.sq`,
`build/packages/squared-data-0.6.0-dev.1.sq`, and
`build/packages/squared-messaging-0.6.0-dev.1.sq`, followed by
the self-contained
`build/packages/squared-android-template-0.6.0-dev.15.sq` and portable
`build/packages/squared-android-template-0.6.0-dev.16.sq` templates entirely
offline. The portable template package is structurally verified here; its
external `dev.2` framework graph is built and owned by the independent
`squared` repository.
The Data archive contains its private yyjson source and license. Messaging
requires exact Data and Time versions. Application proves header-only
`INTERFACE` module composition. Math proves a small compiled module can export
headers and implementation to a template-owned consumer. Graphics proves a
compiled module can consume SDL and OpenGL ES targets supplied by its selected
template. The registry test verifies idempotent import, recursive exact
dependencies, disabled hooks, and collision-safe module composition.
Scene2D proves a compiled Phase 6 module can consume Graphics2D transitively
while keeping hierarchy behavior independently testable without a GPU.

## Expected completion

```text
Private Lua runtime: OK
Squared strict JSON: OK
LuaFileSystem native module: OK
Private package.path: OK
Private package.cpath: OK
Penlight private modules: OK
LDoc generation with built-in Markdown: OK
Global LuaRocks isolation: OK
Offline toolchain test: OK
```

Run the same command again to prove the cached offline path:

```bash
lua5.4 toolchain.lua
```

The second run must complete without downloading or resolving anything.

## Isolation

`tools/private-run.lua` replaces both `package.path` and `package.cpath`.
It intentionally omits Lua's `;;` default-path expansion and does not include
global LuaRocks directories.

The documentation dependencies remain development tools. Generated
applications do not automatically receive LuaFileSystem, Penlight, or LDoc.

## Generated documentation

Successful output is written to:

```text
build/generated-ldoc/
```

## Related

- [Embedding smoke-test README](README.md)
- [Third-Party Dependencies](third_party/README.md)
