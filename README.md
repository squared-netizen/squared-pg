---
title: SDL Project Generator
aliases:
  - sdl-pg
tags:
  - android
  - cpp
  - lua
  - offline
status: experimental
---

# SDL Project Generator

`sdl-pg` creates offline-first Android ARM64 projects with a C++20 host, a
private Lua 5.4.8 scripting runtime, and a validated SDL2 dependency kit.

Version 0.4.0 provides:

- transactional creation beneath `~/sandbox` or `~/projects`;
- non-destructive `promote` and `demote` commands;
- offline SDL2-kit and Gradle-Wrapper registration;
- a Lua-driven local Android build command;
- an optional, manually dispatched GitHub Actions build;
- protected `init`, `update`, `event`, and `shutdown` Lua callbacks;
- a deliberately narrow, data-oriented native `host` API;
- touch, lifecycle, and Android-back event translation;
- deterministic APK-asset Lua module loading;
- versioned, capability-limited application plug-ins;
- per-plug-in load and callback error isolation;
- Obsidian-compatible Markdown documentation;
- Doxygen comments for public C++ APIs and LDoc comments for Lua APIs.
- a local `sdl-pg docs` command and optional manual documentation workflow.

The generator does not initialize Git, commit, push, publish, or contact
GitHub. Local generation and builds are offline by default.

## Install

Install the host tools once:

```bash
pkg install lua54 clang cmake ninja
```

Build the bundled private toolchain and install the commands:

```bash
cd "$HOME/sandbox/sdl-project-generator"
lua5.4 toolchain.lua
./build/private-lua/bin/lua-5.4.8 tools/private-run.lua setup.lua install
hash -r
```

Verify the installation:

```bash
sdl-pg version
sdl-pg doctor
```

No LuaRocks installation or network download is required. The pinned source
archives for Lua, LuaFileSystem, Penlight, and LDoc are included and verified
before use.

## Register offline dependencies

Existing registrations beneath `~/.local/share/sdl-pg` are reused. On a clean
installation:

```bash
sdl-pg kit add \
  /sdcard/Download/offline-deps/SDL2-2.32.10-TTF-2.24.0-MIXER-2.8.2-IMAGE-2.8.12-NET-2.4.0-android-arm64.zip

sdl-pg wrapper add \
  "$HOME/sandbox/sdl2-smoke-test/phone-project"
```

Check the registered inputs:

```bash
sdl-pg kit status
sdl-pg wrapper status
```

## Create and build

Create disposable code beneath `~/sandbox`:

```bash
sdl-pg new lua-rogue \
  --package dev.example.luarogue

cd "$HOME/sandbox/lua-rogue"
lua5.4 tools/build.lua
```

Use `--project` to create serious publishing code beneath `~/projects`.
Use `--online-once` with the generated build command only when the Gradle
dependency cache is incomplete.

Generate recursive Lua, C++, and SDL Java-wrapper API references from the
project root or any child directory:

```bash
sdl-pg docs
```

Generated projects retain an Obsidian-compatible `docs/API.md` index. LDoc
and Doxygen write generated HTML beneath `build/docs/`; Doxygen also preserves
XML for future exporters.

## Promote and demote

```bash
sdl-pg promote lua-rogue
sdl-pg demote published-project
```

Both operations preserve their source directory. Promotion refuses source Git
metadata so repository history is never copied accidentally. Demotion omits
Git metadata from the sandbox copy.

## Scripting boundary

Generated projects document their API in `docs/Scripting.md`. Lua scripts
receive a controlled global `host` table instead of SDL or Android objects.

The default runtime omits `io`, `os`, `package`, and `debug`, and removes
`dofile` and `loadfile`. This is a capability boundary for application
scripting, not a hardened hostile-code sandbox.

The Lua runtime, plug-in manager, and C++ bridge currently remain versioned
with the generator. They may become a separately released runtime after the
plug-in API has stabilized through real projects.

## Documentation

- [[CHANGELOG|Changelog]]
- [[docs/Commands|Commands]]
- [[docs/Architecture|Architecture]]
- [[docs/Repository-Model|Repository Model]]
- [[docs/Safety-Model|Safety Model]]
- [[docs/Phase-4-Validation|Phase 4 Validation]]
- [[docs/Releasing|Releasing]]
- [[PRIVATE-TOOLCHAIN|Private Lua Toolchain]]
- [[CONTRIBUTING|Contributing]]

## License

SDL Project Generator is available under the MIT License. Bundled third-party
components retain their own licenses and notices.
