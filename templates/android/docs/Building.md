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

## Native Downloads logging

The generated C++ application writes its bounded diagnostic report directly
with `std::filesystem` and `std::ofstream`:

```text
/sdcard/Download/{{PROJECT_ID}}-diagnostics.log
```

Android 11 and newer ignore legacy read/write permissions for unrestricted
shared-storage paths. The manifest retains those declarations for older
compatibility and declares `MANAGE_EXTERNAL_STORAGE` for modern Android.
After installing the APK, enable **Allow access to manage all files** for the
application under Android's special app-access settings. A manifest
declaration alone does not grant that access.

Until access is granted, the on-screen report explicitly displays `LOG: FAIL`
with the native open or write error. The application does not use MediaStore
or an Android Java logging bridge.
