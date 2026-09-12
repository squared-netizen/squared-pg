---
title: Android apps with SFML
tags: [usermanual, android, sfml]
---

# Android apps with SFML

An Android application in C++20, with SFML as the platform layer. No Java, no
Gradle, no Android Studio — `make apk` produces a signed, installable APK using
clang, aapt2 and apksigner.

## Generate

```sh
cd ~/sqsysroot/sandbox
sqpg new droid -t template.android.sfml -k kit.sfml \
  -p project_name=droid -p package_name=com.example.droid
cd droid
```

`project_name` becomes the library name, the C++ namespace and the log tag, so
it must be a C identifier. `package_name` is the application id and must be
unique on the device.

## Check before building

```sh
make sfml-status      # the SFML payload: archives, entry point, NDK stubs
make android-status   # the toolchain: NDK, aapt2, apksigner, keystore
```

Run these first if anything goes wrong. Between them they report every external
thing the build depends on.

## Build, package, install

```sh
make          # build/libdroid.so
make apk      # package and sign
make install  # hand it to the package installer
make logcat   # follow this app's log
```

First `make apk` generates a debug keystore at `~/.android/debug.keystore` if
you have none.

## Where your code goes

```
sq_app/          your application. No Android header, no SFML header.
  include/app.hpp
  src/app.cpp
sq_android/      the platform layer. Yours, rarely edited.
  main.cpp
  AndroidManifest.xml
  res/
```

**Write code in `sq_app/`.** The generated `App` class has seven methods the
platform layer calls:

| Method | When |
|---|---|
| `start()` | once, after the window exists |
| `stop()` | once, at shutdown |
| `resume()` / `pause()` | foreground / background |
| `resize(w, h)` | at startup and on every change |
| `render()` | every frame |
| `touch(phase, x, y)` | on touch |

You get a live GLES 3.0 context, already current. `render()` draws; the
platform layer presents.

## Two rules that are not style preferences

**Do not include `<SFML/...>` from `sq_app/`.** The application layer compiles
unchanged under `template.android.cpp`, which reaches the same GLES through
NativeActivity instead. An SFML include ends that. If you need SFML, the code
belongs in `sq_android/main.cpp`.

**Do not remove `-Wl,-u,ANativeActivity_onCreate` from the link.** Nothing in
your project references that symbol — Android looks it up by name when it loads
the library — so without `-u` the linker drops it. Everything still builds and
the app dies at launch with nothing in the log. `make` checks for it and fails
the build if it is missing.

## The thread you are on

Your code does not run on Android's UI thread. SFML receives the lifecycle
callbacks there and runs your `main()` on a thread it owns.

In practice: `App` sees one consistent thread and needs no locking. Blocking
will not trigger an ANR the way blocking the UI thread would — but it still
stops drawing and stops draining events, so do not block.

## Retargeting without regenerating

SDK levels and ABI are build settings, not generation parameters:

```sh
make SQ_MIN_SDK=21 apk
make SQ_ABI=armeabi-v7a        # needs a toolchain that targets it
```

`AndroidManifest.xml` deliberately carries no `<uses-sdk>`; the levels reach
aapt2 through flags. See `make android-help` for every override.

## What works today

Window, GLES 3.0 context, touch input, the lifecycle. That is SFML's System and
Window modules.

**`sf::Texture`, `sf::Sprite`, `sf::Font`, `sf::Text` and all audio classes do
not work yet.** Their headers ship with the kit, so code using them compiles
and then fails to link. Graphics and Audio have not been built for Android (see
Q-34). Draw through GLES directly for now.

## If it does not link

Run `make sfml-status`. The common causes, in order:

1. **The kit payload is missing.** A fresh clone has the manifest but not the
   archives. `sfml-status` says `BUILD-INFO MISSING`.
2. **Wrong ABI.** The archives are built for one ABI at one API level;
   `sfml-status` prints which.
3. **Graphics or Audio symbols.** See above — those modules are not built.

## If it installs and dies at launch

Almost always one of two things, both of which the build already guards
against, so check `make` actually completed:

- `ANativeActivity_onCreate` missing from the library (`make verify-link`)
- `libc++_shared.so` not bundled — Android does not ship it, and the packaging
  step refuses to build an APK without it
