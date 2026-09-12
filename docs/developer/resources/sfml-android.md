---
title: SFML on Android — internals
tags: [developer, resources, sfml, android]
---

# SFML on Android — internals

Counterpart: [[programmer/resources/sfml]] · [[usermanual/android-sfml]]
Decisions: D-067, D-068, D-069 · Open: Q-34, Q-35

How `kit.sfml`'s archives are produced and why the build is shaped as it is.
Everything here was established empirically against SFML 3.1.0, Termux clang
21.1.8 and NDK 29.0.14206865 on aarch64.

## Why a template rather than a kit

Three blockers, each in SFML's source. See D-067 for the full entry; the short
form is that `sfml-main` defines `ANativeActivity_onCreate`, the same process
entry point `android_native_app_glue` occupies, and there is one such slot per
process. SFML is the platform layer; it does not plug into someone else's.

## The build

`tools/build-sfml.sh`. Configures System, Window and Main only — Graphics,
Audio and Network off — so none of the vendored dependencies are touched.
Output goes to `build/sfml-stage/`; `--install` copies into kit.sfml's payload
for local iteration.

Five facts the script encodes. Each cost a build to learn, and each produces a
failure that names something other than its cause.

### 1. Termux's CMake already selects the Android branch

SFML tests `if(ANDROID)` in `cmake/Config.cmake`, not the system name, and
Termux sets it. `-DANDROID=1` is therefore unnecessary — but it is passed
anyway, so the build does not depend on a Termux packaging detail that could
change.

**The failure this prevents:** a plain configure on a host where `ANDROID` is
unset selects the X11 backend, builds cleanly, and produces libraries that are
wrong for an APK with no error anywhere. The script verifies
`WindowImplAndroid` is in the archive and `WindowImplX11` is not, because the
backend is chosen at configure time and silently.

### 2. EGL and GLES must be named by absolute path

`FindEGL.cmake` does `find_library(EGL_LIBRARY NAMES EGL)`. On Termux that
finds `$PREFIX/lib/libEGL.so`, which belongs to **libglvnd — the X11 EGL, not
Android's.** The link succeeds and the APK dies inside the package installer.

Same reasoning as `mk/kit_opengl.mk`: a search order cannot go wrong when there
is no search.

### 3. The include directory must not be the NDK sysroot

SFML does `target_link_libraries(sfml-window PRIVATE EGL::EGL)`, and an
imported target propagates its includes as **`-isystem`** — which places a
second complete set of C headers ahead of libc++. `<cctype>` then finds the
NDK's `ctype.h` and the build stops with *"tried including `<ctype.h>` but
didn't find libc++'s"*.

This is the hazard `mk/kit_opengl.mk` solves with `-idirafter`. Here the fix
has to be a directory rather than a flag position, because the path arrives
through an imported target's interface. The script builds
`build/sfml-khronos/` containing symlinks to exactly five directories — `EGL`,
`GLES`, `GLES2`, `GLES3`, `KHR` — and points both finders at it.

Symlinks rather than copies, so an NDK update is picked up by rerunning the
script rather than by remembering to refresh a copy.

### 4. No cross-toolchain is used

Termux's clang reports `aarch64-unknown-linux-android24`. It already emits
Android objects. The NDK's `android.toolchain.cmake` is deliberately **not**
used: it sets `CMAKE_SYSROOT` and reintroduces hazard 3 wholesale.

The script still verifies the archives contain AArch64 objects via `readelf`,
because an archive of host objects would link and then fail on device.

### 5. Naming and link order

Static archives are suffixed `-s` — `libsfml-system-s.a`, `libsfml-window-s.a`
— **except `libsfml-main.a`, which is not.**

Link order is main, window, system, then EGL, GLESv1_CM, android, log. These
are static archives and the linker resolves left to right: a symbol needed by
an earlier archive must be defined by a later one.

`GLESv1_CM` is what SFML links (see its `FindGLES`). `GLESv3` is added by
`mk/kit_sfml.mk` for application code, which draws through GLES 3.0 and would
otherwise have no implementation to bind against.

## The entry point

`libsfml-main.a` defines `ANativeActivity_onCreate`, which Android resolves **by
name at load time**. Nothing in a generated project references it, so a static
linker drops the archive member that defines it. The link succeeds, the `.so`
is well formed, the APK installs, and the process dies at launch with nothing
in the log naming the cause.

`mk/squared_generated.mk` carries `-Wl,-u,ANativeActivity_onCreate` to force
it in. `make verify-link` confirms with `nm` that it survived, and `make`
depends on that check — this failure is too cheap to catch and too expensive to
find on a device.

## Threading

SFML's activity glue receives lifecycle and window callbacks on Android's UI
thread, records them in `ActivityStates`, and runs the project's `main()` on a
**detached thread of its own** (`std::thread(sf::priv::main, states).detach()`).

Consequences: `App` sees one consistent thread and needs no synchronisation;
blocking will not trigger an ANR the way blocking the UI thread would, but
still stops the frame loop and the event queue; JNI from that thread needs an
attached environment, and SFML attaches its own.

## No Java

SFML 3.1.0's tree contains **zero `.java` files**. Every `FindClass` names a
stock Android framework class — `android/view/MotionEvent`,
`android/os/Build$VERSION` — never an SFML one, and it reaches the activity via
`states.activity->clazz` with `GetObjectClass`, so it uses whatever activity
object it is handed. Stock `android.app.NativeActivity` satisfies it.

The `SFMLActivity.java` seen in SFML's Gradle examples exists to
`System.loadLibrary` several `.so` files in dependency order. Static archives
linked into one `.so` make that problem disappear.

## Not established

- **Graphics and Audio have not been built** (Q-34). The dependencies are
  vendored but none has been cross-compiled, and SFML's CMake patches
  FreeType's config to break a FreeType/HarfBuzz cycle — whether that survives
  outside FetchContent is unknown.
- **The release-time build has not been exercised.** During alpha the archives
  are produced on-device by `tools/build-sfml.sh`; CI running the same script
  is the intended shape but has not run.
- **Only arm64-v8a at API 24 has been produced** (Q-35). The script accepts
  `SQ_ABI` and `SQ_MIN_SDK` overrides; neither alternative has been tried.
