---
title: Changelog
tags:
  - release
  - scripting
---

# Changelog

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
