---
title: Squared Project Generator
aliases:
  - squared-pg
  - sdl-pg
tags:
  - android
  - cpp
  - lua
  - offline
status: experimental
---

# Squared Project Generator

`squared-pg` is an offline-first project generator for applications built on
the Squared framework. Its first frontend is an Android ARM64 C++20 host using
SDL2 and a private Lua 5.4.8 scripting runtime. SDL2 is a selected frontend,
not part of the generator's permanent identity; later templates may target
SFML, PDCurses, or other libraries without shipping unused frontends in a
generated application.

The 0.6.0 development line provides:

- transactional creation beneath `~/sandbox` or `~/projects`;
- non-destructive `promote` and `demote` commands;
- offline SDL2-kit and Gradle-Wrapper registration;
- generator-only template-provider selection with no generated runtime
  dispatcher;
- generator-only external dependency dispatch with `android-sdl2` as the
  first stable provider ID;
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
- a local `squared-pg docs` command and optional manual documentation workflow.
- an OpenGL ES 2 graphics context under the `squared::` namespace;
- `Texture`, `TextureRegion`, `Sprite`, `SpriteBatch`, and
  `OrthographicCamera` graphics2d foundations;
- a transactionally loaded, libGDX-compatible multi-page `TextureAtlas`;
- a strict, deterministic native JSON API under `squared::data`, backed by
  pinned yyjson 0.12.0;
- JSON-provided text rendered through SDL_ttf with stage-colored fallbacks;
- a touch-driven blue/green atlas sentinel for visible lifecycle testing.
- a thin generated SDL adapter around a developer-owned C++ application;
- automatic developer source discovery restricted to `application/src/`.
- pausable, scaled `squared::time` domains and a bounded deterministic
  deadline queue.
- libGDX-AI-inspired Telegram/Telegraph messaging with bounded queued,
  immediate, delayed, broadcast, cancellation, and receipt behavior;
- authoritative provider-generated state for each new Telegram subscriber,
  with bounded queued delivery and no retained-message replay;
- ordered pending-message inspection and deterministic JSON snapshot/restore
  using remaining delays and stable identifiers.
- on-screen SDL_ttf `PASS`/`FAIL` diagnostics with a compact JSON Telegram
  dump and bounded native logging to `/sdcard/Download`.

The generator does not initialize Git, commit, push, publish, or contact
GitHub. Local generation and builds are offline by default.

RAM efficiency is the primary runtime engineering constraint. Frontend
selection happens during generation and compilation so an application does
not carry SDL2, SFML, PDCurses, or their adapter state unless its chosen
template needs them. Startup and screen-transition work may be staged behind
loading screens, but active execution paths must remain fast and bounded.

## Install

Install the host tools once:

```bash
pkg install lua54 clang cmake ninja
```

Clone the generator into its protected user-local tool root. Generator-only
code never belongs beneath the end-user `~/sandbox` or `~/projects` trees:

```bash
mkdir -p "$HOME/.squared"
git clone https://github.com/squared-netizen/sdl-project-generator.git \
  "$HOME/.squared/squared-pg"
cd "$HOME/.squared/squared-pg"
lua5.4 toolchain.lua
./build/private-lua/bin/lua-5.4.8 tools/private-run.lua setup.lua install
hash -r
```

Verify the installation:

```bash
squared-pg version
squared-pg doctor
squared-pg self-test
```

No LuaRocks installation or network download is required. The pinned source
archives for Lua, LuaFileSystem, Penlight, LDoc, yyjson, and miniz are
included and verified before use.

### Uninstall

Preview the complete generator-owned cleanup plan without changing anything:

```bash
squared-pg uninstall
```

To uninstall, first leave the generator repository and explicitly confirm:

```bash
cd "$HOME"
squared-pg uninstall --confirm
```

Uninstall removes the generator beneath `~/.squared`, its launchers,
configuration, local registry, and legacy `sdl-pg` state. It never removes or
traverses `~/sandbox` or `~/projects`; all generated end-user code is
preserved.

The unreleased `.sq` version 0 proof builds and locally registers Squared
Application, Graphics, Graphics2D, Scene2D, Math, Time, Data, Messaging, and
the Android SDL2 Lua project template as independent packages. Application
proves source-free, header-only `INTERFACE` modules. Graphics owns color and the
SDL/OpenGL ES context boundary. Graphics2D owns textures, sprites, atlases,
batching, and cameras while requiring exact Graphics and Math versions.
Scene2D begins Phase 6 with owned actor hierarchy, ordered traversal, bounds
hit testing, and a root stage while requiring exact Graphics2D. Data
owns its pinned yyjson source and license. Messaging declares exact Data and
Time dependencies, proving recursive composition.
Backed-up templates and modules use the same native validation path:

```bash
squared-pg package build /path/to/package-source /path/to/package.sq
squared-pg package verify /path/to/package.sq
squared-pg package add /path/to/package.sq
squared-pg package sync /path/to/framework-repository
squared-pg package status
squared-pg package resolve \
  dev.squarednetizen.template.android-sdl2-lua@0.6.0-dev.15
```

Importing a local package does not contact a network. Project composition
refuses file collisions instead of overwriting template or developer files.
Repeated imports of the same ID, version, and content are harmless; conflicting
content under an existing ID and version is rejected.
Exact module dependencies are resolved recursively before project mutation.
The resolver produces dependency-first deterministic order, suppresses
diamonds, and rejects missing packages, cycles, and conflicting versions.
Broad Android shared-storage access is opt-in through `squared-pg new
--manage-all-files`; enabled projects document the risk and open their
app-specific special-access settings once on first startup when needed.

The packages currently stored in this repository are a frozen offline
bootstrap catalog and integration fixture for the 0.6 transition. Ongoing
Squared framework development belongs in the separate `squared` repository:
it builds new immutable package versions with the installed `squared-pg`, adds
them to the local registry, and selects a template with `squared-pg template
use ID@VERSION`. Generator source and its private toolchain do not need to be
rebuilt for those framework changes.

From a framework repository whose package sources live beneath `packages/`,
`squared-pg package sync` builds deterministic archives beneath `dist/`,
verifies them, and registers them. Repeated synchronization reuses identical
archives and registrations. Changed content under an existing package version
is rejected and requires a version bump.

The self-contained Android template `0.6.0-dev.15` remains the default for a
clean generator installation. Android template `0.6.0-dev.16` is the portable
framework integration target. It requires Graphics, Graphics2D, and Scene2D
`dev.2` plus `dev.squarednetizen.squared.backend.sdl2-opengl@0.6.0-dev.1`, all
built and registered from the independent `squared` repository. The generator
contains the template and compile-time wiring, but does not duplicate those
framework implementations.

## Diagnostics for people and agents

```bash
squared-pg verbose
squared-pg agent-feedback
squared-pg project verify .
squared-pg project build .
```

`project build` locates the generated project root and dispatches its existing
`tools/build.lua` through the generator's private Lua. Add `--clean` for a
clean rebuild or `--online-once` for the project's explicit Gradle bootstrap
path.

`verbose` is the complete human-readable status report. `agent-feedback` is a
stable, allowlisted subset designed for an AI coding agent: at most 32 lines
and 4 KiB, package counts instead of inventories, actionable issues and a next
command, no arbitrary environment dump, and an explicit `truncated` field.
These commands run only in the generator process and add no runtime code or
memory use to generated applications.

## Register offline dependencies

Existing `sdl-pg` configuration and registrations are reused during the
naming transition. Clean installations use `~/.config/squared-pg` and
`~/.local/share/squared-pg`:

```bash
squared-pg dependency add android-sdl2 \
  /sdcard/Download/offline-deps/SDL2-2.32.10-TTF-2.24.0-MIXER-2.8.2-IMAGE-2.8.12-NET-2.4.0-android-arm64.zip

squared-pg wrapper add \
  "$HOME/sandbox/sdl2-smoke-test/phone-project"
```

Check the registered inputs:

```bash
squared-pg dependency status android-sdl2
squared-pg wrapper status
```

The older `squared-pg kit add` and `kit status` forms remain aliases for the
same `android-sdl2` provider and reuse existing cached state.

## Create and build

Create disposable code beneath `~/sandbox`:

```bash
squared-pg new lua-rogue \
  --package dev.example.luarogue

cd "$HOME/sandbox/lua-rogue"
squared-pg project build
```

Use `--project` to create serious publishing code beneath `~/projects`.
Use `--online-once` with `squared-pg project build` only when the Gradle
dependency cache is incomplete.

Generate recursive Lua, C++, and SDL Java-wrapper API references from the
project root or any child directory:

```bash
squared-pg docs
```

Generated projects retain an Obsidian-compatible `docs/API.md` index. LDoc
and Doxygen write generated HTML beneath `build/docs/`; Doxygen also preserves
XML for future exporters.

## Promote and demote

```bash
squared-pg promote lua-rogue
squared-pg demote published-project
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
- [[docs/Phase-5-Design|Phase 5 Design]]
- [[docs/Phase-5-MVP-Validation|Phase 5 MVP Validation]]
- [[docs/Releasing|Releasing]]
- [[PRIVATE-TOOLCHAIN|Private Lua Toolchain]]
- [[CONTRIBUTING|Contributing]]

## License

Squared Project Generator is available under the MIT License. Bundled third-party
components retain their own licenses and notices.
