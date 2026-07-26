---
title: Building
tags:
  - build
  - offline
---

# Building

The checked-in Lua builder performs the complete phone build:

```sh
lua5.4 tools/build.lua
```

It configures the native CMake project, builds `libmain.so`, stages the SDL2
shared libraries, writes `local.properties`, and launches the checked-in Gradle
Wrapper through Java. It does not use shell file-copy commands.

The first Gradle build may need network access:

```sh
lua5.4 tools/build.lua --online-once
```

After Gradle dependencies are cached, normal builds are offline.

Generate API documentation separately:

```sh
sdl-pg docs
```

This uses the generator's private LDoc toolchain plus the locally installed
Doxygen executable. See [[API|API Reference]].

Use `--clean` to move the existing native build directory aside before
configuring:

```sh
lua5.4 tools/build.lua --clean
```

The APK is produced at `app/build/outputs/apk/debug/app-debug.apk` and copied
to `/sdcard/Download/{{PROJECT_ID}}-debug.apk` when Downloads is writable.
