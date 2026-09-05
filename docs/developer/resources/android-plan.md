# Plan — `template.android.cpp` and `kit.opengl`

Written against the host capability probes in `tools/probe/`, not against
assumptions. Every path and flag below came from a run on the target device
(Samsung SM-A155M, Android 16 / SDK 36, arm64-v8a, Termux, clang 21.1.8).

Status: **design agreed, one item unverified** — see [Open](#open).

---

## What the probes settled

| Question | Answer | Source |
|---|---|---|
| Android-targeting compiler | clang 21.1.8, `aarch64-unknown-linux-android24` | `10` |
| NativeActivity headers | all present, all compile | `20`, `50` |
| Khronos headers | in the NDK, compile with `-I` | `60` |
| NDK location | `$PREFIX/opt/android-sdk/ndk/29.0.14206865` | `60`, `80` |
| `android_native_app_glue` | in the NDK's `sources/`, compiles | `80` |
| `android.jar` | android-34 and android-36 present | `30` |
| APK chain | aapt2 → zip → apksigner, builds and verifies | `70` |
| Naive `-lEGL` | **binds libglvnd, wrong** — needs explicit `-L` | `80` |
| C++ runtime | `NEEDED libc++_shared.so` | `80`, `90` |
| `zipalign` | absent — sidestepped, see below | `30` |

Nothing needs installing. The one hazard found is the libglvnd `libEGL.so` in
`$PREFIX/lib`, which a plain `-lEGL` binds in preference to the NDK stub. That
would fail at runtime inside an APK, which is the worst place to discover it,
so the build fragment passes `-L` explicitly and the `.so` target verifies
`NEEDED` after linking.

---

## Architecture

```text
AndroidManifest.xml  →  android.app.NativeActivity      framework class, no Java
                             ↓
                     android_native_app_glue            compiled from the NDK
                             ↓
                     sq_android/entry.cpp               seeded — glue → App adapter
                             ↓
                     sq_app/src/app.cpp                 yours, portable
                             ↓
                     sq::gl                             kit.opengl: EGL + GLES 3.0
```

**No Java, no Kotlin, no `classes.dex`, no Gradle.** `d8` is installed and
unused. Getting the SDK and NDK was painful once; nothing here asks for more.

**Rejected: GameActivity.** Needs AndroidX, which needs Gradle dependency
resolution, which needs the network. Disqualifying under §1.13 regardless of
its better input handling.

**Rejected: a thin Java `Activity` + JNI.** Works, but puts a `.java` and a
`classes.dex` in the critical path, so a JDK and `d8` become required to build
anything at all. NativeActivity keeps the native path buildable with clang
alone.

---

## Workspace layout

```text
ProjectDir/
  sq_app/         your portable C++              seeded, working directory
    include/
    src/app.cpp                                  the App class — no Android in it
  sq_android/     the platform workspace         seeded — the escape hatch
    AndroidManifest.xml
    entry.cpp                                    glue → App adapter
    res/
      values/strings.xml
      values/colors.xml
      drawable/ic_launcher_foreground.xml        vector, not PNG
      mipmap-anydpi-v26/ic_launcher.xml          adaptive icon
      mipmap-hdpi/ic_launcher.png                one legacy fallback
  sq_kit/         kit headers                    generated
  mk/             build fragments                generated
  Makefile        yours                          seeded
```

`sq_android/` is entirely **seeded**: you may edit the manifest, the resources
and the glue adapter, and the generator will never overwrite them. That is the
expert escape hatch. §2.7.3's promise — you never need to edit outside
`sq_app/` — stays true for the normal case, because the defaults work.

`sq_app/app.cpp` includes no Android header. The platform layer calls into it.
That is what makes the same `App` compile under `template.termux.cpp`.

---

## Making `kit.sfml` possible later

The template requires **no** rendering kit. It declares:

```json
"integration_areas": ["render.backend", "app.include",
                      "build.make.fragments", "android.manifest.features"],
"integration_arity": { "render.backend": "single",
                       "app.include": "multi",
                       "build.make.fragments": "multi",
                       "android.manifest.features": "multi" }
```

`sq_android/entry.cpp` uses `__has_include(<squared/kit/gl.hpp>)`, exactly as
`app.cpp` already does for `kit.lua`. So `kit.opengl` and a future `kit.sfml`
are interchangeable, and the engine's existing conflict detection refuses both
at once because `render.backend` is `single`.

Without a rendering kit the project still builds and runs: a black window and a
log line saying no backend was selected. That is deliberate — a template that
cannot build without a specific kit has hardcoded that kit.

**Known gap (Q-29).** The engine can express "requires kit X" but not "requires
at least one kit providing X". `__has_include` covers it in the generated
source; whether resolution should also express it is open, and not a blocker.

---

## NDK detection belongs to the template, not the kit

**Correction to an earlier draft.** I had `kit.opengl` locate the NDK and
compile `android_native_app_glue.c`. That is wrong: without a rendering kit
there would then be no glue, and the template promises to build without one.

NativeActivity is the *template's* architecture. So `mk/squared_generated.mk`
locates the NDK and compiles the glue, exporting `SQ_NDK`, `SQ_NDK_INC`,
`SQ_NDK_LIB` and `SQ_GLUE_OBJ`. `kit.opengl` consumes those and adds only
`-lEGL -lGLESv3` and its own header.

That also makes `kit.sfml` cheaper later — it inherits a working NativeActivity
build and adds a renderer, rather than re-solving NDK detection.

## SDK levels are build settings, not generation parameters

`minSdkVersion`, `targetSdkVersion` and the ABI are **not** template parameters.
Baking them at generation time would mean regenerating a project to retarget it.

They live in `mk/squared_generated.mk` as overridable variables, and reach the
APK through `aapt2 link --min-sdk-version` / `--target-sdk-version` rather than
through the manifest. The manifest therefore carries no `<uses-sdk>` at all, and
`make apk SQ_MIN_SDK=21` works without touching a generated file.

```make
SQ_MIN_SDK    ?= 24     # clang's default target is android24
SQ_TARGET_SDK ?= 34
SQ_ABI        ?= $(shell getprop ro.product.cpu.abi 2>/dev/null || echo arm64-v8a)
```

## One ABI, honestly

Termux's clang targets `aarch64` only; `ndk-multilib` is available but not
installed, and building four ABIs on a phone is not the use case.

The template builds for the **host ABI** and says so. Multi-ABI is a documented
`make` invocation for someone on a desktop NDK, not a default that quietly
produces an APK which runs on one device in three.

## `kit.opengl`

External dependency: the **NDK**, `acquisition: "system"` — but located by the
template's fragment, as above. The kit adds EGL and GLES on top.

### `mk/kit_opengl.mk`

Detection, in the `kit_lua.mk` style — probe, report honestly, never fail
cryptically:

```make
# SQ_NDK, SQ_NDK_INC and SQ_NDK_LIB come from mk/squared_generated.mk, which
# the template owns. The kit only adds what a renderer needs.
SQ_KIT_CPPFLAGS += -I$(SQ_NDK_INC) -Isq_kit/include
SQ_KIT_LDFLAGS  += -L$(SQ_NDK_LIB)
SQ_KIT_LDLIBS   += -lEGL -lGLESv3
SQ_KITS_PRESENT += kit.opengl
```

`-I$(SQ_NDK_INC)` and — the part that matters — **`-L$(SQ_NDK_LIB)` before
`-lEGL -lGLESv3`**, so the Android stubs win over libglvnd. The `.so` rule
checks `readelf -d` output afterwards and fails loudly if `libEGL.so` resolved
to `$PREFIX/lib`. A silent wrong bind is exactly the failure this kit exists to
prevent.

`make gl-status` prints the resolved paths and whether each library bound
correctly.

### Surface — `<squared/kit/gl.hpp>`

```cpp
namespace sq::gl {
  class Context;      // EGL display/config/surface/context; make_current, swap
  class Surface;      // ANativeWindow: width, height, density, rotation
  class Shader;       // compile, with the log as a string rather than a crash
  class Program;      // link, cached uniform locations
  class Buffer;       // VBO / EBO
  class VertexArray;
  class Texture2D;    // from raw bytes
  void clear(Color);
  void viewport(int, int);
  const char* error_string(GLenum);
}
```

GLES **3.0** floor (`#version 300 es`, GLSL ES 3.00). API 18+, covers
effectively every live device. 3.1 would buy compute shaders at the cost of
narrowing the field, which is the wrong trade for a "basics" kit.

Deliberately **excluded**: PNG and asset decoding (an asset concern; a decoder
would make a basics kit non-trivial to audit), math types (framework concern),
and frame-loop ownership (`sq_android/entry.cpp` owns it, so you can replace
it).

---

## Build and packaging

Two tiers, each probing for what it needs:

| Target | Requires | Produces |
|---|---|---|
| `make` | clang + NDK | `build/lib/arm64-v8a/lib<name>.so` |
| `make apk` | + aapt2, apksigner, keytool, android.jar | `build/<name>.apk` |
| `make install` | + termux-open | hands it to the package installer |
| `make gl-status` | — | resolved NDK paths and library bindings |

Packaging, verified end to end by `70-apk-smoke.sh`:

```text
aapt2 link -I android.jar --manifest ... -o base.apk
zip base.apk lib/arm64-v8a/lib<name>.so
zip base.apk lib/arm64-v8a/libc++_shared.so     # unless statically linked
apksigner sign --ks ~/.android/debug.keystore
```

**`android:extractNativeLibs="true"`**, on purpose. The alternative, `"false"`,
requires the `.so` to be page-aligned inside the APK — 16 KiB on Android 15+ —
and `zipalign` is not installed. With `"true"` the library is stored compressed
and the installer extracts it, so alignment never arises. Marginally larger on
disk, works everywhere, nothing to improvise.

**`libc++_shared.so`** ships in the APK beside the app's own library, copied
from `$SQ_NDK_LIB`. `90-libcxx.sh` checks whether `-static-libstdc++` removes
the dependency; if it does, the APK carries one file instead of two and the
fragment prefers it.

The debug keystore is generated on first `make apk` if absent — you already
have one.

---

## Icons

Adaptive icons as **XML**, not PNG:

```text
res/drawable/ic_launcher_foreground.xml     vector drawable
res/values/colors.xml                       background colour
res/mipmap-anydpi-v26/ic_launcher.xml       adaptive icon (API 26+)
res/mipmap-hdpi/ic_launcher.png             one legacy fallback
```

The default is therefore text — substitutable, reviewable in a diff, and
parameterised by project name. One small PNG is the entire binary payload.

**Nothing depends on an asset cartridge existing.** A build must never fail
because a default asset resource was missing or unmaintained. When asset
materialization lands (D-032), `asset.android.icons` becomes an *optional*
override, not a prerequisite.

---

## Delivery

Split, given that the APK install is still unconfirmed:

**Phase 1** — `template.android.cpp` plus a minimal `kit.opengl` that brings up
EGL, clears the screen green, and handles the activity lifecycle. Proves the
whole chain on-device: generate → build → package → install → run.

**Phase 2** — the shader, buffer, texture and program surface, once phase 1 is
confirmed.

Phase 1 is the risky half and it is small. Phase 2 is bulk with little
uncertainty in it.

---

## Untested engine paths this template will be the first to exercise

Not defects, but paths with no coverage until now. Each is exercised by the
template itself, so the tests belong with it rather than ahead of it:

- **a binary payload in a template** — the PNG icon fallback. `looks_textual`
  is unit-tested; no end-to-end test copies a binary file through a plan.
- **`single`-arity integration conflict** — `render.backend` is the first area
  where two kits could collide. `cross_validate` implements the check; nothing
  has ever tripped it.
- **the `java_package` parameter type** — implemented in
  `validation_service.cpp` for exactly this template, never exercised.
- **substitution inside a filename** — implemented, unused by
  `template.termux.cpp`.

Phase 1 adds a test for each, against the real Android template rather than a
fixture — matching how `test_generate.cpp` already works.

## Open

**Does `~/squared-probe.apk` install and run?** The only finding left that
would change the *approach* rather than the details.

```sh
termux-open ~/squared-probe.apk
logcat -d -s sqprobe
```

Black screen is success. Expect `onCreate`, `onStart`, `onResume` and a window
pointer. If the install is refused, NativeActivity-without-`classes.dex` is not
viable on Android 16 and the plan changes at the top rather than the edges.

**Does the resource path work?** `70-apk-smoke.sh` built an APK with **no
`res/` directory** — it went straight to `aapt2 link --manifest`. The real
template compiles resources first and references `@string/app_name` and
`@mipmap/ic_launcher` from the manifest. That two-stage path is standard and
well documented; so was linking `-lEGL`, which bound the wrong library.
`95-apk-resources.sh` builds the real thing, adaptive icon and all.

**Does `-static-libstdc++` drop the `libc++_shared.so` dependency?**
`90-libcxx.sh` answers it. Either outcome is fine; it decides one line.
