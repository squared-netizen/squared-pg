---
title: Changelog
tags:
  - release
  - scripting
---

# Changelog

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
