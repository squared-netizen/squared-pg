# Android projects

`template.android.cpp` produces a plain NDK workspace: C++20, NativeActivity,
**no Java, no Gradle, no `classes.dex`**. It builds a signed, installable APK
with a compiler and `make`, on a phone.

```sh
sqpg new myapp --template template.android.cpp --kit kit.opengl \
    --platform android --param package_name=com.example.myapp

cd myapp
make            # lib myapp.so
make apk        # packaged and signed
make install    # handed to the package installer
make logcat
```

## Parameters

| Parameter | Required | Meaning |
|---|---|---|
| `package_name` | **yes** | Android application id, e.g. `com.example.myapp` |
| `app_label` | no | launcher label; defaults to the project name |
| `description` | no | one line for the README |

`project_name` comes from the `new` command and becomes the library name, the
C++ namespace and the log tag.

## Layout

```text
sq_app/        your code — portable C++, no Android headers
sq_android/    the platform layer — manifest, resources, entry point
sq_kit/        kit headers (generator-owned)
mk/            build fragments (generator-owned)
Makefile       yours
```

`sq_app/` is the working directory: nothing generator-owned is written there,
ever. `sq_android/` is **seeded** — the generator writes it once and never
again, so the manifest, the resources and the entry point are all yours. That
is the expert escape hatch, and you should not normally need it.

## Keeping Android out of `sq_app/`

`sq_app/` includes no `<android/...>`, no EGL, no GLES. The platform layer
calls into your `App` through `app.hpp`, so the same class compiles under
another template with a different platform layer.

It is easy to lose one include at a time. Worth checking as you add code.

## Rendering is optional and swappable

The template requires **no** rendering kit. `sq_android/entry.cpp` and
`sq_app/src/app.cpp` both guard their include with `__has_include`, so the
project builds and runs with none — a black window and a log line saying so.

That is what makes `kit.opengl` and a future `kit.sfml` alternatives rather
than one being baked in. They both claim the `render.backend` integration area,
which has arity `single`, so selecting two is refused before anything is
written.

### `kit.opengl`

`<squared/kit/gl.hpp>` gives you `sq::gl`:

| | |
|---|---|
| `Context` | EGL display, config, surface and context, bound to one window |
| `Color`, `clear`, `viewport` | immediate operations |
| `Result` | failures as values — a lost surface is ordinary, not exceptional |
| `error_string`, `check_errors` | GL errors are sticky; something has to ask |

GLES **3.0**, GLSL ES 3.00 (`#version 300 es`). Fragment shaders need explicit
precision qualifiers — the first thing that catches people coming from desktop
GLSL.

`Context::present()` returns false when the surface is lost, which happens
every time the app is backgrounded. Stop drawing and wait for a new window;
ignoring it spins, rendering into nothing.

Shaders, buffers and textures are not in this release.

## SDK levels are build settings

`AndroidManifest.xml` carries no `<uses-sdk>`. The levels reach `aapt2` from
the build, so retargeting never means editing a file:

```sh
make apk SQ_MIN_SDK=21 SQ_TARGET_SDK=35
```

## What the host needs

```sh
make android-status
```

| For | Needs |
|---|---|
| `make` | clang targeting Android, the NDK sysroot, native-app-glue |
| `make apk` | + `aapt2`, `apksigner`, `keytool`, an `android.jar` |
| `make install` | + `termux-open` or `adb` |

On Termux: `pkg install clang make ndk-sysroot aapt2 apksigner openjdk-17`.
The debug keystore is generated on first `make apk`.

## Three things that will bite you

**The NDK's include directory must not go on `-I`.** Termux's clang already
ships every `android/*` header; the NDK is needed only for the Khronos headers
Termux lacks. Putting its sysroot on `-I` places a second complete set of C
headers ahead of libc++, and `<cctype>` then finds the NDK's `ctype.h` instead
of libc++'s wrapper:

```
<cctype> tried including <ctype.h> but didn't find libc++'s <ctype.h>
```

`kit.opengl` uses `-idirafter`, which appends to the very end of the search
path, so the NDK supplies only what nothing else provides.

**`$PREFIX/lib/libEGL.so` is not Android's EGL.** It belongs to `libglvnd` and
is for X11. A plain `-lEGL` binds it in preference to the NDK stub; the link
succeeds and the APK dies at runtime. `kit.opengl` names the NDK's `libEGL.so`
and `libGLESv3.so` by absolute path rather than as `-l` names — a search order
cannot go wrong when there is no search. `-L` would have worked too, but it
also puts the NDK's `libc` and `libm` stubs ahead of Termux's for every
implicit `-l` the driver adds.

**`libc++_shared.so` must be in the APK.** Termux's clang links the shared C++
runtime unconditionally — `-static-libstdc++` fails outright — and Android does
not supply it. `make apk` bundles it, stripped, from the NDK. Without it the
APK installs and then dies on launch before any of your code runs.

## One ABI

The template builds for the host ABI. Termux's clang targets aarch64 only, and
building four ABIs on a phone is not the use case. `make android-help` explains
how a desktop NDK does more.
