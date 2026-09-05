#!/bin/sh
# Probe: does it actually compile and link?
#
# The header checks in 20-ndk-headers.sh answer "is the file there". This
# answers "does the toolchain accept it", which is the question that matters --
# a header present but unusable (wrong API level, missing transitive include,
# stub library without the symbol) looks identical to a working one until you
# try.
#
# Every check builds a real translation unit and throws it away. Nothing is
# left behind and nothing outside $TMPDIR is touched.

set -u
. "$(dirname "$0")/_lib.sh"

CXX=${CXX:-clang++}
CC=${CC:-clang}

have "$CXX" || { warn "$CXX not found; set CXX and re-run"; exit 0; }

scratch=$(probe_scratch) || { warn "could not create a scratch directory in ${TMPDIR:-/tmp}"; exit 0; }
trap 'rm -rf "$scratch"' EXIT INT TERM
note "scratch" "$scratch"

# try_compile <label> <source> [extra flags...]
try_compile() {
    _label=$1; shift
    _source=$1; shift
    printf '%s\n' "$_source" > "$scratch/probe.cpp"
    if "$CXX" -std=c++20 -c "$scratch/probe.cpp" -o "$scratch/probe.o" "$@" 2>"$scratch/err"; then
        ok "$_label"
        return 0
    fi
    no "$_label"
    sed 's/^/        /' "$scratch/err" | head -n 4
    return 1
}

# try_link <label> <source> [extra flags...]
try_link() {
    _label=$1; shift
    _source=$1; shift
    printf '%s\n' "$_source" > "$scratch/probe.cpp"
    if "$CXX" -std=c++20 -shared -fPIC "$scratch/probe.cpp" -o "$scratch/libprobe.so" "$@" 2>"$scratch/err"; then
        ok "$_label"
        return 0
    fi
    no "$_label"
    sed 's/^/        /' "$scratch/err" | head -n 6
    return 1
}

section "baseline"

try_compile "C++20 compiles" '
#include <version>
#include <filesystem>
#include <span>
int main() { return 0; }
'

try_compile "std::expected available" '
#include <version>
#if !defined(__cpp_lib_expected)
#error no <expected>
#endif
#include <expected>
int main() { return 0; }
'
# Not fatal: error.hpp falls back to sqcart::expected. Reported so we know
# which path a build on this host takes.

section "android platform"

try_compile "android/native_activity.h" '
#include <android/native_activity.h>
extern "C" void ANativeActivity_onCreate(ANativeActivity*, void*, size_t) {}
'

try_compile "android/log.h" '
#include <android/log.h>
void probe() { __android_log_print(ANDROID_LOG_INFO, "probe", "%s", "x"); }
'

try_compile "android/native_window.h" '
#include <android/native_window.h>
int probe(ANativeWindow* w) { return ANativeWindow_getWidth(w); }
'

try_compile "android/asset_manager.h" '
#include <android/asset_manager.h>
AAsset* probe(AAssetManager* m) { return AAssetManager_open(m, "x", AASSET_MODE_BUFFER); }
'

section "graphics"

try_compile "EGL/egl.h" '
#include <EGL/egl.h>
EGLDisplay probe() { return eglGetDisplay(EGL_DEFAULT_DISPLAY); }
'

try_compile "GLES3/gl3.h" '
#include <GLES3/gl3.h>
void probe() { glClearColor(0.f, 1.f, 0.f, 1.f); glClear(GL_COLOR_BUFFER_BIT); }
'

# GLES 3.0 is the floor kit.opengl targets. If GL_ES_VERSION_3_0 is absent the
# sysroot is older than expected and the kit needs a lower floor.
try_compile "GLES 3.0 macros present" '
#include <GLES3/gl3.h>
#if !defined(GL_ES_VERSION_3_0)
#error GLES 3.0 not declared
#endif
void probe() {}
'

try_compile "GLSL ES 3.00 shader path" '
#include <GLES3/gl3.h>
// The shader source itself is a runtime string; this only confirms the API
// surface kit.opengl needs to compile one exists.
unsigned probe() {
    unsigned s = glCreateShader(GL_VERTEX_SHADER);
    const char* src = "#version 300 es\nvoid main(){gl_Position=vec4(0.0);}\n";
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    int ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    return s;
}
'

section "linking a shared object"

# This is the real target: template.android.cpp produces a lib<name>.so with
# exactly these dependencies.
try_link "lib<name>.so links" '
#include <android/native_activity.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
extern "C" void ANativeActivity_onCreate(ANativeActivity*, void*, size_t) {
    __android_log_print(ANDROID_LOG_INFO, "probe", "up");
    eglGetDisplay(EGL_DEFAULT_DISPLAY);
    glClear(GL_COLOR_BUFFER_BIT);
}
' -landroid -llog -lEGL -lGLESv3

if [ -f "$scratch/libprobe.so" ]; then
    if have file; then
        note "produced" "$(file "$scratch/libprobe.so" 2>/dev/null | cut -d: -f2-)"
    fi
    if have readelf; then
        note "NEEDED" "$(readelf -d "$scratch/libprobe.so" 2>/dev/null \
            | sed -n 's/.*NEEDED.*\[\(.*\)\]/\1/p' | tr '\n' ' ')"
    fi
    note "size" "$(wc -c < "$scratch/libprobe.so") bytes"
fi

section "native app glue"

# If the NDK ships the glue source, confirm it builds here. If not, that is
# expected -- kit.opengl vendors it -- and this check is skipped rather than
# failed.
glue_c=""
for candidate in \
    "${ANDROID_NDK_HOME:-}/sources/android/native_app_glue/android_native_app_glue.c" \
    "${PREFIX:-/usr}/share/ndk-sysroot/sources/android/native_app_glue/android_native_app_glue.c"
do
    [ -n "$candidate" ] && [ -f "$candidate" ] && glue_c="$candidate" && break
done

if [ -n "$glue_c" ]; then
    if "$CC" -c "$glue_c" -o "$scratch/glue.o" -I"$(dirname "$glue_c")" 2>"$scratch/err"; then
        ok "android_native_app_glue.c compiles" "$glue_c"
    else
        no "android_native_app_glue.c compiles"
        sed 's/^/        /' "$scratch/err" | head -n 4
    fi
else
    note "glue source" "not shipped; kit.opengl will vendor it (expected)"
fi

section "aapt2 without android.jar"

# The question from 30-apk-tools.sh: can aapt2 link a manifest that references
# no framework resources, with no android.jar? If yes, packaging is
# dependency-free and the SDK can stay uninstalled.
if have aapt2; then
    mkdir -p "$scratch/res/values"
    cat > "$scratch/AndroidManifest.xml" <<'XML'
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="com.example.probe">
  <uses-sdk android:minSdkVersion="24" android:targetSdkVersion="34" />
  <application android:label="probe" android:hasCode="false">
    <activity android:name="android.app.NativeActivity" android:exported="true">
      <meta-data android:name="android.app.lib_name" android:value="probe" />
      <intent-filter>
        <action android:name="android.intent.action.MAIN" />
        <category android:name="android.intent.category.LAUNCHER" />
      </intent-filter>
    </activity>
  </application>
</manifest>
XML
    if aapt2 link -o "$scratch/probe.apk" --manifest "$scratch/AndroidManifest.xml" \
            --auto-add-overlay 2>"$scratch/err"; then
        ok "aapt2 link with no android.jar" "packaging can be dependency-free"
        note "apk size" "$(wc -c < "$scratch/probe.apk") bytes"
    else
        no "aapt2 link with no android.jar"
        sed 's/^/        /' "$scratch/err" | head -n 6
        warn "an android.jar is required; see 30-apk-tools.sh for options"
    fi
else
    note "aapt2" "absent; this check skipped"
fi

summary "compile checks"
