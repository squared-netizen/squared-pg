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

This private toolchain supplies the modules used by SDL Project Generator and
its documentation command.

Included dependency archives:

| Dependency | Version | Purpose |
| --- | --- | --- |
| Lua | 5.4.8 | Private interpreter and embedded runtime |
| LuaFileSystem | 1.8.0 | Native filesystem primitives |
| Penlight | 1.14.0 | Lua utility modules used by LDoc |
| LDoc | 1.5.0 | Lua API documentation |

LDoc 1.5.0 contains its own Lua Markdown renderer. The test deliberately uses
that renderer, so no obsolete external `markdown` rock is required.

## Requirements

```bash
pkg install lua54 clang cmake ninja
```

## Run the complete test

```bash
cd "$HOME/sandbox/sdl-project-generator"
lua5.4 toolchain.lua
```

If the Termux package exposes Lua 5.4 as `lua`:

```bash
lua toolchain.lua
```

No dependency is downloaded. Every source archive is bundled and verified
before extraction.

## Expected completion

```text
Private Lua runtime: OK
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
