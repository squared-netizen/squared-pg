#!/bin/sh
# Probe: which libraries does a naive link actually bind, and where is the glue?
#
# 60 found a full NDK at $PREFIX/opt/android-sdk/ndk/, with working Khronos
# headers. Two things remain before kit.opengl's build fragment can be written
# with confidence, and both are about linking rather than compiling.
#
# 1. $PREFIX/lib/libEGL.so belongs to libglvnd -- Termux's X11 EGL, not
#    Android's. A plain `-lEGL` may bind that instead of the NDK stub, and the
#    failure would surface at runtime inside an APK, which is the worst place
#    to discover it. `-Wl,--trace` says which file the linker actually opened,
#    so this is checkable rather than arguable.
#
# 2. android_native_app_glue lives in the NDK's sources/ directory, which is
#    not on any include path. If it is there, kit.opengl can compile it from
#    the NDK instead of vendoring a copy.
#
# Read-only. Nothing installed, nothing written outside $TMPDIR.

set -u
. "$(dirname "$0")/_lib.sh"

CXX=${CXX:-clang++}
CC=${CC:-clang}
API=${API:-24}

section "locate the ndk"

NDK=""
for candidate in $(ls -d "${PREFIX:-/usr}/opt/android-sdk/ndk"/* "${ANDROID_NDK_HOME:-}" 2>/dev/null | sort -V -r); do
    [ -d "$candidate/toolchains/llvm/prebuilt" ] && { NDK="$candidate"; break; }
done
[ -n "$NDK" ] || { warn "no NDK found; nothing here applies"; exit 0; }
ok "ndk" "$NDK"

# The prebuilt directory is named for the *host*, and an arm64 Android host is
# unusual enough that guessing would be wrong half the time.
SYSROOT=""
for host in linux-arm64 linux-x86_64 darwin-x86_64; do
    candidate="$NDK/toolchains/llvm/prebuilt/$host/sysroot"
    [ -d "$candidate" ] && { SYSROOT="$candidate"; ok "sysroot" "$candidate"; break; }
done
[ -n "$SYSROOT" ] || { warn "no sysroot under $NDK"; exit 0; }

NDK_INC="$SYSROOT/usr/include"
NDK_LIB="$SYSROOT/usr/lib/aarch64-linux-android/$API"
[ -d "$NDK_LIB" ] || NDK_LIB="$SYSROOT/usr/lib/aarch64-linux-android"
note "include" "$NDK_INC"
note "lib" "$NDK_LIB"

section "ndk stub libraries"

for lib in libEGL.so libGLESv3.so libGLESv2.so libandroid.so liblog.so libm.so; do
    if [ -e "$NDK_LIB/$lib" ]; then
        ok "$lib" "$NDK_LIB/$lib"
    else
        no "$lib" "not under $NDK_LIB"
    fi
done

section "native app glue"

GLUE_DIR=""
for candidate in "$NDK/sources/android/native_app_glue" \
                 "$SYSROOT/usr/include/android/native_app_glue"; do
    if [ -f "$candidate/android_native_app_glue.c" ] || [ -f "$candidate/android_native_app_glue.h" ]; then
        GLUE_DIR="$candidate"
        ok "glue" "$candidate"
        ls "$candidate" | sed 's/^/        /'
        break
    fi
done
[ -n "$GLUE_DIR" ] || no "glue" "not in the NDK; kit.opengl will vendor it"

scratch=$(probe_scratch) || exit 0
trap 'rm -rf "$scratch"' EXIT INT TERM

if [ -n "$GLUE_DIR" ] && [ -f "$GLUE_DIR/android_native_app_glue.c" ]; then
    if "$CC" -c "$GLUE_DIR/android_native_app_glue.c" -o "$scratch/glue.o" \
            -I"$GLUE_DIR" -I"$NDK_INC" 2>"$scratch/err"; then
        ok "glue compiles from the NDK"
    else
        no "glue compiles from the NDK"
        sed 's/^/        /' "$scratch/err" | head -n 5
    fi
fi

section "which libEGL does a naive link bind?"

cat > "$scratch/p.cpp" <<'CPP'
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/log.h>
extern "C" void probe() {
    eglGetDisplay(EGL_DEFAULT_DISPLAY);
    glClearColor(0.f, 1.f, 0.f, 1.f);
    __android_log_print(ANDROID_LOG_INFO, "p", "x");
}
CPP

# --trace prints every input file the linker opened, which is the only way to
# see which of two same-named libraries won.
if "$CXX" -std=c++20 -shared -fPIC "$scratch/p.cpp" -o "$scratch/naive.so" \
        -I"$NDK_INC" -lEGL -lGLESv3 -llog -Wl,--trace >"$scratch/trace" 2>&1; then
    ok "naive link succeeds"
    bound=$(grep -iE 'libEGL|libGLESv3|liblog' "$scratch/trace" | head -n 6)
    printf '%s\n' "$bound" | sed 's/^/        /'
    if printf '%s' "$bound" | grep -q "${PREFIX:-/usr}/lib/libEGL"; then
        warn "bound \$PREFIX/lib/libEGL.so -- that is libglvnd, NOT Android's EGL"
        warn "the build fragment must pass -L for the NDK stubs"
    fi
else
    no "naive link succeeds"
    sed 's/^/        /' "$scratch/trace" | head -n 8
fi

section "explicit ndk link"

if "$CXX" -std=c++20 -shared -fPIC "$scratch/p.cpp" -o "$scratch/ndk.so" \
        -I"$NDK_INC" -L"$NDK_LIB" -lEGL -lGLESv3 -landroid -llog \
        -Wl,--trace >"$scratch/trace2" 2>&1; then
    ok "explicit -L link succeeds"
    grep -iE 'libEGL|libGLESv3|libandroid|liblog' "$scratch/trace2" | head -n 6 | sed 's/^/        /'
    if have readelf; then
        note "NEEDED" "$(readelf -d "$scratch/ndk.so" 2>/dev/null \
            | sed -n 's/.*NEEDED.*\[\(.*\)\]/\1/p' | tr '\n' ' ')"
    fi
else
    no "explicit -L link succeeds"
    sed 's/^/        /' "$scratch/trace2" | head -n 10
fi

summary "ndk link"

cat <<NOTE

  What kit.opengl's build fragment will use, if the explicit link above worked:

      SQ_NDK      := $NDK
      SQ_NDK_INC  := $NDK_INC
      SQ_NDK_LIB  := $NDK_LIB

  with -I\$(SQ_NDK_INC) and -L\$(SQ_NDK_LIB) before -lEGL -lGLESv3, so the
  Android stubs win over anything libglvnd put in \$PREFIX/lib.
NOTE
