---
title: Changelog
tags:
  - release
  - scripting
---

# Changelog

## Unreleased

- Move the canonical generator checkout and private runtime to
  `~/.squared/squared-pg`, outside the end-user `~/sandbox` and `~/projects`
  trees. Installation now refuses every non-canonical source root.
- Add a dry-run-first `squared-pg uninstall` command with explicit
  `--confirm`, canonical-root validation, legacy-state cleanup, and tests
  proving that end-user projects remain untouched.
- Advance the generator identity to `0.6.0-dev.6` and the frozen bootstrap
  Android template to `0.6.0-dev.15`, keeping generated project metadata and
  immutable template coordinates aligned with the generator release.
- Add supported `package build` and `package verify` commands so external
  repositories can produce and validate immutable `.sq` archives with an
  installed generator instead of rebuilding the generator toolchain.
- Add persistent `template use`/`template status` selection. New framework or
  frontend package versions can be registered and selected independently of
  the generator release.
- Add `verbose`, bounded `agent-feedback`, `project verify`, and `self-test`
  diagnostics. Agent feedback uses a stable line-oriented schema capped at
  32 lines and 4 KiB, excludes arbitrary environment data, and ends with an
  explicit truncation flag.
- Move the frozen bootstrap Android SDL2 Lua template to `0.6.0-dev.14` for
  the generator `0.6.0-dev.5` metadata. Framework development now validates
  against the installed package commands rather than requiring generator
  source changes.
- Permit explicitly requested package-output paths to traverse existing
  directory symlinks such as Android's `/sdcard`, while retaining the strict
  no-link default for managed project and cache trees.
- Establish `squared-pg` as the frontend-neutral generator identity while
  retaining `sdl-pg` as a compatibility launcher during the 0.6 development
  line.
- Prefer `SQUARED_PG_*`, `~/.config/squared-pg`,
  `~/.local/share/squared-pg`, and `.squared-pg.lua` while automatically
  reusing legacy configuration, caches, environment variables, and project
  markers.
- Move the Android SDL2 Lua template to `0.6.0-dev.11` with Squared Project
  Generator metadata and documentation. SDL2 remains the first frontend
  rather than the generator's permanent identity.
- Record RAM efficiency as the primary runtime constraint: frontend choice is
  resolved during generation and compilation so generated applications do not
  retain unused frontend implementations or runtime-selection state.
- Add the generator-only template provider boundary and move the Android SDL2
  Lua template to `0.6.0-dev.12`. Template profile dispatch now completes
  before project staging; generated applications remain statically specialized
  and contain no frontend registry or runtime dispatch path.
- Add the generator-only external dependency dispatcher and move the Android
  SDL2 Lua template to `0.6.0-dev.13`. The Android adapter now preflights the
  stable `android-sdl2` dependency before staging. `dependency` is the primary
  CLI while the existing `kit` command and cached kit state remain compatible.
- Begin the `.sq` format version 0 proof with a native C++20
  `squared_sq_core`.
- Vendor pinned miniz 3.1.2 source under its MIT license for Stored, Deflate,
  and ZIP64 archive mechanics without a system-library fallback.
- Add strict SQ manifest parsing, portable path and resource validation,
  exact SHA-256 inventories, deterministic package writing, and transactional
  extraction.
- Add structured non-throwing public results and Doxygen-documented SQ APIs.
- Add a native package round-trip test before extracting Squared Time from the
  Android template.
- Add the thin `squared.sq` Lua 5.4 binding, transactional local package
  registry, and offline `package add`/`package status` commands.
- Make Squared Time the first independent module package, build its `.sq`
  archive offline, and compose it into generated projects through a
  collision-safe CMake module hook.
- Add native, Lua-binding, registry, generator, and deterministic package
  validation for the Time proof.
- Keep missing or malformed SQ API documentation visible as Doxygen warnings
  without failing an otherwise valid build.
- Make the Android SDL2 Lua project tree the second independent `.sq` package.
- Add bounded template form fields, exact module requirements, explicit
  hook rejection, exact template selection, and transactional dependency
  preflight.
- Make identical package imports idempotent while rejecting conflicting
  content under an existing package ID and version.
- Add exact recursive module dependencies with deterministic dependency-first
  ordering, duplicate suppression, cycle detection, version-conflict
  rejection, bounded graph size, and parent-aware missing-package errors.
- Add `package resolve ID@VERSION` for a read-only view of the composition
  order before project creation.
- Make Squared Data the third independent `.sq` proof, with its strict JSON
  API, implementation, documentation, pinned yyjson 0.12.0 source, and
  license owned by one self-contained module package.
- Move the Android template to `0.6.0-dev.2` with exact Squared Data and
  Squared Time dependencies, removing its direct JSON implementation and
  yyjson build ownership.
- Make Squared Messaging the fourth independent `.sq` proof, owning its
  Telegram, Telegraph, dispatcher, provider, documentation, and native
  implementation files.
- Declare exact Squared Data and Squared Time requirements on Messaging, then
  move the Android template to `0.6.0-dev.3` with Messaging as its sole direct
  framework dependency. This proves dependency-first transitive composition
  in a generated Android project.
- Make Squared Application the fifth independent `.sq` proof, exporting the
  platform-neutral lifecycle and event contracts through a source-free CMake
  `INTERFACE` target.
- Move the Android template to `0.6.0-dev.4` with exact direct Application and
  Messaging requirements, while retaining the rendered, project-specific
  application-boundary guide in the template.
- Make Squared Math the sixth independent `.sq` proof, owning its platform-
  neutral `Vector2`, `Matrix4`, implementation, documentation, and CMake
  target.
- Move the Android template to `0.6.0-dev.5` with an exact Squared Math
  requirement. Graphics2D now links the composed `squared_math` target and no
  longer owns duplicate math source or headers.
- Make Squared Graphics the seventh independent `.sq` proof, owning `Color`,
  the SDL/OpenGL ES context, documentation, and its compiled CMake target.
- Move the Android template to `0.6.0-dev.6` with an exact Squared Graphics
  requirement. The remaining template-owned Graphics2D target now consumes
  `squared_graphics` instead of compiling the context itself.
- Move the Android template to `0.6.0-dev.7` and make broad shared-storage
  access an explicit `--manage-all-files` generation option. Enabled projects
  declare the special permission, open app-specific settings once on first
  startup when needed, and carry a prominent README warning; ordinary projects
  no longer request the permission.
- Move the Android template to `0.6.0-dev.8` and correct the generated
  first-start activity to import `ActivityNotFoundException` from
  `android.content`, restoring Android Java compilation.
- Make Squared Graphics2D the eighth independent `.sq` proof, owning textures,
  texture regions, sprites, atlases, batching, orthographic cameras,
  documentation, and its compiled CMake target with exact Graphics and Math
  requirements.
- Move the Android template to `0.6.0-dev.9`, replace its direct Graphics and
  Math requirements with Graphics2D, and remove all template-owned Graphics2D
  source, headers, documentation, and build-target ownership.
- Begin Phase 6 with Squared Scene2D as the ninth independent `.sq` proof,
  owning actor bounds, parent/child ownership, deterministic frame traversal,
  reverse-order hit testing, and the root stage with an exact Graphics2D
  requirement.
- Move the Android template to `0.6.0-dev.10`, replace its direct Graphics2D
  requirement with Scene2D, and add an on-device `SCENE2D` hierarchy
  diagnostic without changing the existing rendered sample.

## 0.5.0

- Replace the generated SDL renderer path with an OpenGL ES 2 context using
  SDL's bundled portable headers.
- Resolve GLES2 through an NDK linker stub or the Android ARM64 system library
  without packaging a host OpenGL implementation.
- Add the first `squared::graphics2d` framework vertical slice.
- Add `Texture`, `TextureRegion`, `Sprite`, and ordered `SpriteBatch`.
- Add an owning libGDX text `TextureAtlas` with multiple pages, indexed
  duplicates, rotation, trimming, nine-patch metadata, and repeat modes.
- Add a bundled blue/green atlas sentinel for observable lifecycle testing.
- Keep atlas failures visible with a red fallback square.
- Use a standard PNG atlas page compatible with the initialized SDL_image PNG
  decoder and normal libGDX TexturePacker output.
- Add a top-left or bottom-left `OrthographicCamera` and core math types.
- Add strict RFC 8259 JSON parsing and deterministic writing under
  `squared::data`, backed by pinned yyjson 0.12.0.
- Preserve signed, unsigned, and real number categories; reject duplicate
  keys by default; and enforce configurable input and nesting limits.
- Add an on-device JSON round-trip diagnostic that renders JSON-provided text
  through SDL_ttf with stage-specific color fallbacks.
- Bundle DejaVu Sans Mono and its permissive license for offline text
  diagnostics.
- Separate the generated SDL adapter from developer-owned application code
  through a stable `create_application()` factory.
- Add platform-neutral application lifecycle and input events.
- Discover developer `.c` and `.cpp` files automatically beneath the
  application source boundary while retaining explicit framework and platform
  source lists.
- Add pausable, scaled application time domains with read-only clock views.
- Add a bounded deterministic deadline queue with cancellation, explicit
  overflow results, and per-update delivery limits.
- Add a magenta/cyan on-device timing sentinel that freezes while backgrounded.
- Add independently implemented, libGDX-AI-inspired Telegram/Telegraph
  messaging with stable namespaced IDs and owned JSON payloads.
- Add bounded queued, immediate, delayed, directed, and broadcast delivery.
- Add scoped subscriptions, cancellation, handled results, correlation IDs,
  and separate queued return receipts.
- Add scoped authoritative Telegram providers that generate fresh JSON state
  for each new subscriber without retained-message replay.
- Reject provider failures and initial-state queue exhaustion atomically, and
  add a red/green on-device provider-delivery sentinel.
- Add allocation-bounded pending-message inspection without payload copies.
- Add deterministic versioned JSON snapshots using remaining delays and stable
  IDs, plus transactional restoration into empty queues.
- Explicitly reject persistence of process-local provider-targeted deliveries,
  and add an orange/green snapshot round-trip sentinel.
- Replace color-only diagnostics with an SDL_ttf report containing explicit
  `PASS`, `FAIL`, and `PENDING` values plus a compact JSON Telegram dump.
- Write the same bounded report natively to the public Android Download
  directory with explicit permission and I/O failure reporting.
- Declare legacy storage compatibility permissions and modern Android
  all-files access without granting Lua plug-ins an implicit filesystem API.
- Drive the timing sentinel through delayed directed Telegrams.
- Preserve the Lua-driven sample through the new rendering API.
- Define scene graph and UI types as Phase 6 work.

## 0.4.0

- Add deterministic Lua module loading from APK assets.
- Add versioned plug-in manifests and an explicit runtime API version.
- Add capability-limited, read-only `host` proxies for plug-ins.
- Add plug-in-local module caches with missing-module and cycle diagnostics.
- Add deterministic lifecycle fan-out in registry order.
- Isolate plug-in load and callback failures from other plug-ins.
- Replace the single application script with documented example plug-ins.
- Add pure-Lua plug-in runtime tests and generated-project assertions.
- Add `sdl-pg docs` for recursive Lua, C++, and Java API generation.
- Add separate Doxygen references for authored C++ and vendored SDL Java.
- Add an Obsidian-compatible API index and a manual GitHub docs workflow.

## 0.3.0

- Publish the first repository-ready SDL Project Generator release.
- Add transactional creation beneath `~/sandbox` and `~/projects`.
- Add non-destructive `promote` and `demote` operations.
- Add verified SDL2-kit and Gradle-Wrapper registration.
- Generate complete offline-first Android ARM64 projects.
- Add protected `init`, `update`, `event`, and `shutdown` Lua callbacks.
- Add a small data-oriented native `host` API.
- Restrict the default Lua library profile.
- Disable a failing callback after reporting its protected-call error.
- Translate touch, background, foreground, and back events for scripts.
- Make the generated example animation and touch behavior Lua-driven.
- Add Obsidian-compatible architecture, command, safety, and scripting docs.
- Add Doxygen public C++ API comments and LDoc Lua API comments.
- Include a pinned private Lua 5.4.8 documentation toolchain.
