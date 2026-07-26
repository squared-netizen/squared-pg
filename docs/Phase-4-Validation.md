---
title: Phase 4 Validation
tags:
  - android
  - plugins
  - release
  - testing
status: candidate
---

# Phase 4 Validation

Version 0.4.0 is a candidate until its private toolchain, generated project,
native build, APK packaging, and on-device behavior pass on Android ARM64.

## 1. Verify and test the generator

From the extracted generator directory:

```bash
sha256sum -c SHA256SUMS.txt
lua5.4 toolchain.lua
```

The toolchain test includes pure-Lua coverage for:

- deterministic registry and lifecycle order;
- reverse shutdown order;
- capability-limited host proxies;
- private module caching;
- traversal and dependency-cycle rejection;
- plug-in API-version rejection;
- isolation of a plug-in that fails during initialization.

## 2. Install the candidate

```bash
./build/private-lua/bin/lua-5.4.8 \
  tools/private-run.lua \
  setup.lua install

hash -r
sdl-pg version
sdl-pg doctor
```

Expected version output begins with:

```text
sdl-pg 0.4.0
```

Existing SDL2-kit and Gradle-Wrapper registrations are reused.

## 3. Generate and build

Use a new sandbox name so the generator's existing-destination protection
remains active:

```bash
sdl-pg new phase4-plugin-hack \
  --package dev.squarednetizen.phase4pluginhack

cd "$HOME/sandbox/phase4-plugin-hack"
lua5.4 tools/build.lua
```

The generated APK must contain:

```text
assets/lua/bootstrap.lua
assets/lua/plugins.lua
assets/lua/runtime/module_loader.lua
assets/lua/runtime/plugin_manager.lua
assets/lua/plugins/diagnostics/manifest.lua
assets/lua/plugins/diagnostics/main.lua
assets/lua/plugins/orbit/manifest.lua
assets/lua/plugins/orbit/main.lua
assets/lua/plugins/orbit/palette.lua
```

Generate all API references:

```bash
sdl-pg docs
```

Confirm these entry points exist:

```text
build/docs/lua/index.html
build/docs/cpp/html/index.html
build/docs/java/html/index.html
```

## 4. Validate on device

Install the generated debug APK. Confirm:

- the application starts without closing;
- the tile animates;
- touch moves the tile;
- releasing touch resumes automatic movement;
- Android Back exits through `host.request_quit()`;
- backgrounding and foregrounding do not break updates.

After these checks pass, the candidate is suitable for committing and running
the manually dispatched GitHub verification workflow. The generated
application's Android-build and documentation workflows remain manual.
