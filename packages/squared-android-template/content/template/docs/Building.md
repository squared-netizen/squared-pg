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
squared-pg docs
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
shared-storage paths. Generate the project with `squared-pg new ...
--manage-all-files` only when unrestricted shared-storage access is a genuine
application requirement. The switch declares `MANAGE_EXTERNAL_STORAGE` and
opens the app-specific **All files access** settings once on first startup when
access is missing. A manifest declaration alone does not grant that access.

Without the switch, the broad permission is absent and the settings screen is
not opened. The older bounded read/write declarations remain for compatibility
with Android versions where they still apply.

Until access is granted, the on-screen report explicitly displays `LOG: FAIL`
with the native open or write error. The application does not use MediaStore
or an Android Java logging bridge.
