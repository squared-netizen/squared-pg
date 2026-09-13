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
sq_app/          your application. Use SFML here.
  include/app.hpp
  src/app.cpp
sq_android/      the platform layer. Yours, rarely edited.
  main.cpp
  AndroidManifest.xml
  assets/        files packaged into the APK
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

`render(target)` hands you an `sf::RenderTarget`. Draw into it; the platform
layer presents.

## Assets

Put files in `sq_android/assets/`. They are packaged into the APK by `make apk`
and opened by **bare name relative to that directory**:

```cpp
sf::Texture texture;
texture.loadFromFile("ui/panel.png");   // sq_android/assets/ui/panel.png

sf::Font font;
font.openFromFile("font.ttf");

sf::Music music;
music.openFromFile("tune.ogg");
```

Subdirectories survive. Nothing outside `sq_android/assets/` reaches a running
app.

`make android-status` reports how many files were found. To see exactly what
shipped:

```sh
unzip -l build/<name>.apk | grep assets/
```

The generated demo loads `font.ttf` if one is present and draws the project
name with it, so dropping a font in is the quickest way to confirm the path
works end to end.

## Two rules that are not style preferences

**Use SFML in `sq_app/`; do not use `<android/...>` there.** SFML already
abstracts the platform, so reaching past it gives up portability between
devices. Anything genuinely needing the Android API belongs in
`sq_android/main.cpp`.

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

SFML's System, Window, Graphics and Audio modules: the window, a GLES 3.0
context, touch input, the lifecycle, 2D drawing, text with real fonts, and
sound.

`sf::Texture`, `sf::Sprite`, `sf::Font`, `sf::Text`, `sf::Sound` and
`sf::Music` all work. Write them in `sq_app/`.

**Network is not built.** `sf::Http`, `sf::Ftp` and the socket classes have
headers but no implementation, so code using them compiles and then fails to
link.

## If it does not link

Run `make sfml-status`. The common causes, in order:

1. **The kit payload is missing.** A fresh clone has the manifest but not the
   archives. `sfml-status` says `BUILD-INFO MISSING`.
2. **Wrong ABI.** The archives are built for one ABI at one API level;
   `sfml-status` prints which.
3. **Network symbols.** That module is not built.

## If it installs and dies at launch

Almost always one of two things, both of which the build already guards
against, so check `make` actually completed:

- `ANativeActivity_onCreate` missing from the library (`make verify-link`)
- `libc++_shared.so` not bundled — Android does not ship it, and the packaging
  step refuses to build an APK without it
