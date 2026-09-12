#!/usr/bin/env bash
# build-sfml.sh -- build SFML's static archives for Android, from the vendored
# source in third-party/SFML-3.1.0.
#
#   tools/build-sfml.sh              build, stage into build/sfml-stage/
#   tools/build-sfml.sh --install    also copy into kit.sfml's payload
#   tools/build-sfml.sh --clean      remove the build and stage directories
#
# Output by default goes to build/, which is gitignored: these are aarch64
# binaries and they do not belong in the source repository. --install copies
# them into resources/kits/kit.sfml/tree/sq_kit/ for local iteration, which is
# convenient while the kit is being debugged and must not be committed; the
# .gitignore entries that keep it that way are listed at the end of this file.
#
# At release time CI runs this and the packaging step in sequence, and the
# archives reach users inside the cartridge rather than through git.
#
# ## Five things this script encodes, each of which cost a build to learn
#
# 1. Termux's CMake already selects SFML's Android branch. SFML tests
#    `if(ANDROID)` (cmake/Config.cmake), not the system name, and Termux sets
#    it. -DANDROID=1 is therefore unnecessary here -- but harmless, and passed
#    anyway so this script does not depend on a Termux packaging detail.
#
# 2. EGL and GLES must be named explicitly. SFML's FindEGL does
#    `find_library(EGL_LIBRARY NAMES EGL)`, and on Termux that finds
#    $PREFIX/lib/libEGL.so, which belongs to libglvnd -- the X11 EGL, not
#    Android's. The link succeeds and the APK dies inside the package
#    installer. Naming the NDK stub by absolute path removes the search, and a
#    search that does not happen cannot go wrong.
#
# 3. The include directory handed to EGL_INCLUDE_DIR must NOT be the NDK
#    sysroot. SFML does target_link_libraries(sfml-window PRIVATE EGL::EGL),
#    and an imported target propagates its includes as -isystem -- which puts
#    a second complete set of C headers ahead of libc++. <cctype> then finds
#    the NDK's ctype.h and libc++ stops the build with "tried including
#    <ctype.h> but didn't find libc++'s". This is the same hazard
#    mk/kit_opengl.mk solves with -idirafter; here the fix has to be a
#    directory rather than a flag position, so we build one containing only
#    the five Khronos directories Termux lacks.
#
# 4. Termux's clang targets aarch64-linux-android already. No cross-toolchain
#    is needed and the NDK's android.toolchain.cmake is deliberately not used:
#    it would set CMAKE_SYSROOT and reintroduce hazard 3 wholesale.
#
# 5. The static archives are suffixed -s, except sfml-main, which is not.
#    Link order is main, window, system, then EGL, GLESv1_CM, android, log.

set -eu

MODE=build
INSTALL=0

while [ $# -gt 0 ]; do
    case "$1" in
        --install) INSTALL=1; shift ;;
        --clean)   MODE=clean; shift ;;
        -h|--help) sed -n '2,8p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) printf 'unknown option: %s\n' "$1" >&2; exit 2 ;;
    esac
done

say()  { printf '%s\n' "$*"; }
step() { printf '\n== %s\n' "$*"; }
die()  { printf 'build-sfml: %s\n' "$*" >&2; exit 1; }

# ---------------------------------------------------------------- locations

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$here"

SFML_SRC="$here/third-party/SFML-3.1.0"
BUILD_DIR="$here/build/sfml-build"
STAGE="$here/build/sfml-stage"
KHRONOS="$here/build/sfml-khronos"
KIT="$here/resources/kits/kit.sfml/tree/sq_kit"

if [ "$MODE" = clean ]; then
    rm -rf "$BUILD_DIR" "$STAGE" "$KHRONOS"
    say "removed build/sfml-build, build/sfml-stage, build/sfml-khronos"
    say "kit payload left alone; remove it by hand if you installed one"
    exit 0
fi

[ -d "$SFML_SRC" ] || die "no SFML source at third-party/SFML-3.1.0"
[ -f "$SFML_SRC/CMakeLists.txt" ] || die "third-party/SFML-3.1.0 has no CMakeLists.txt"
command -v cmake >/dev/null 2>&1 || die "cmake is not installed"

# --------------------------------------------------------------------- NDK
#
# Same search order as mk/squared_generated.mk, and for the same reason:
# ANDROID_NDK_HOME wins when set, because someone who set it meant it.

NDK="${ANDROID_NDK_HOME:-}"
if [ -z "$NDK" ]; then
    NDK="$(ls -d "${PREFIX:-/usr}/opt/android-sdk/ndk"/* \
                 "${ANDROID_HOME:-/nonexistent}/ndk"/* \
                 "${ANDROID_SDK_ROOT:-/nonexistent}/ndk"/* 2>/dev/null | sort -Vr | head -1)"
fi
[ -n "$NDK" ] && [ -d "$NDK" ] || die "no Android NDK found; set ANDROID_NDK_HOME"

HOSTDIR="$(ls -d "$NDK"/toolchains/llvm/prebuilt/* 2>/dev/null | head -1)"
[ -n "$HOSTDIR" ] || die "$NDK has no toolchains/llvm/prebuilt"
SYSROOT="$HOSTDIR/sysroot"

API="${SQ_MIN_SDK:-24}"
ABI="${SQ_ABI:-arm64-v8a}"
case "$ABI" in
    arm64-v8a)   LIBTRIPLE=aarch64-linux-android ;;
    armeabi-v7a) LIBTRIPLE=arm-linux-androideabi ;;
    x86_64)      LIBTRIPLE=x86_64-linux-android ;;
    *) die "unsupported SQ_ABI: $ABI" ;;
esac
NDKLIB="$SYSROOT/usr/lib/$LIBTRIPLE/$API"

[ -f "$NDKLIB/libEGL.so" ] || die "no libEGL.so at $NDKLIB (wrong API level?)"
[ -f "$NDKLIB/libGLESv1_CM.so" ] || die "no libGLESv1_CM.so at $NDKLIB"

step "toolchain"
say "  ndk       $NDK"
say "  sysroot   $SYSROOT"
say "  abi/api   $ABI / android-$API"
say "  clang     $(clang++ -dumpmachine 2>/dev/null || echo '?')"

# ------------------------------------------------------------- Khronos dir
#
# Reason 3 above. Only these five: Termux's clang ships every android/* header
# in its own sysroot, so the NDK is needed for the Khronos headers and nothing
# else. Symlinks rather than copies so an NDK update is picked up by rerunning
# this script rather than by remembering to refresh a copy.

step "Khronos include directory"
rm -rf "$KHRONOS"
mkdir -p "$KHRONOS"
for d in EGL GLES GLES2 GLES3 KHR; do
    [ -d "$SYSROOT/usr/include/$d" ] || die "NDK sysroot has no $d/"
    ln -s "$SYSROOT/usr/include/$d" "$KHRONOS/$d"
done
say "  $KHRONOS -> 5 directories"

# ----------------------------------------------------------------- configure

step "configure"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cmake -S "$SFML_SRC" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DANDROID=1 \
    -DSFML_BUILD_GRAPHICS=OFF \
    -DSFML_BUILD_AUDIO=OFF \
    -DSFML_BUILD_NETWORK=OFF \
    -DSFML_BUILD_EXAMPLES=OFF \
    -DSFML_BUILD_TEST_SUITE=OFF \
    -DEGL_INCLUDE_DIR="$KHRONOS" \
    -DEGL_LIBRARY="$NDKLIB/libEGL.so" \
    -DGLES_INCLUDE_DIR="$KHRONOS" \
    -DGLES_LIBRARY="$NDKLIB/libGLESv1_CM.so" \
    -Wno-dev \
    > "$BUILD_DIR/configure.log" 2>&1 \
  || { tail -30 "$BUILD_DIR/configure.log" >&2; die "configure failed; full log at $BUILD_DIR/configure.log"; }
say "  ok (log: build/sfml-build/configure.log)"

# --------------------------------------------------------------------- build

#step "build"
#cmake --build "$BUILD_DIR" --target sfml-system sfml-window sfml-main \
 #     -j"$(nproc 2>/dev/null || echo 2)" \
#     > "$BUILD_DIR/build.log" 2>&1 \
 # || { tail -40 "$BUILD_DIR/build.log" >&2; die "build failed; full log at $BUILD_DIR/build.log"; }
#
# Progress is streamed rather than hidden: this takes minutes on a phone, and
# a silent script that long is indistinguishable from a hung one. The full log
# still goes to build.log; only CMake's own [ nn%] lines reach the terminal,
# rewritten in place so the output stays one line.
#
# PIPESTATUS rather than $? because the pipeline's status is the filter's.
cols="$( { tput cols; } 2>/dev/null || echo 60 )"
width=$(( cols > 20 ? cols - 4 : 56 ))

set +e
cmake --build "$BUILD_DIR" --target sfml-system sfml-window sfml-main \
      -j"$(nproc 2>/dev/null || echo 2)" 2>&1 \
  | tee "$BUILD_DIR/build.log" \
  | while IFS= read -r line; do
        case "$line" in
            \[*%\]*) printf '\r  %-*.*s' "$width" "$width" "$line" ;;
        esac
    done
rc=${PIPESTATUS[0]}
set -e
printf '\r%*s\r' "$cols" ''

[ "$rc" -eq 0 ] || { tail -40 "$BUILD_DIR/build.log" >&2
                     die "build failed; full log at $BUILD_DIR/build.log"; }

for lib in libsfml-main.a libsfml-window-s.a libsfml-system-s.a; do
    [ -f "$BUILD_DIR/lib/$lib" ] || die "expected $lib was not produced"
    say "  $(printf '%-22s %8s bytes' "$lib" "$(wc -c < "$BUILD_DIR/lib/$lib")")"
done

# ------------------------------------------------------------------- verify
#
# Two checks, both cheap, both catching a failure that otherwise appears only
# when an APK is installed on a device and dies before any of its code runs.

step "verify"

# The symbol Android resolves by name at load time. If it is absent the link
# still succeeds later and the app dies at launch with nothing in the log.
nm --defined-only "$BUILD_DIR/lib/libsfml-main.a" 2>/dev/null \
    | grep -q 'T ANativeActivity_onCreate' \
  || die "libsfml-main.a does not define ANativeActivity_onCreate"
say "  ANativeActivity_onCreate  defined"

# An archive of host objects would link and then fail on device.
member="$(ar t "$BUILD_DIR/lib/libsfml-window-s.a" | head -1)"
( cd "$BUILD_DIR" && ar x "lib/libsfml-window-s.a" "$member" )
machine="$(readelf -h "$BUILD_DIR/$member" 2>/dev/null | awk -F: '/Machine/{print $2}' | xargs)"
rm -f "$BUILD_DIR/$member"
case "$machine" in
    AArch64|*aarch64*) say "  object machine            $machine" ;;
    *) die "archives contain $machine objects, not AArch64" ;;
esac

# The backend actually compiled, rather than merely selected at configure time.
if ar t "$BUILD_DIR/lib/libsfml-window-s.a" | grep -q 'WindowImplX11'; then
    die "libsfml-window-s.a contains the X11 backend"
fi
ar t "$BUILD_DIR/lib/libsfml-window-s.a" | grep -q 'WindowImplAndroid' \
  || die "libsfml-window-s.a has no WindowImplAndroid; wrong backend"
say "  backend                   Android"

# --------------------------------------------------------------------- stage

step "stage"
rm -rf "$STAGE"
mkdir -p "$STAGE/include" "$STAGE/lib"
cp -r "$SFML_SRC/include/SFML" "$STAGE/include/"
cp "$BUILD_DIR"/lib/libsfml-main.a \
   "$BUILD_DIR"/lib/libsfml-window-s.a \
   "$BUILD_DIR"/lib/libsfml-system-s.a "$STAGE/lib/"

# A stamp so the kit manifest and any later audit can say what these are
# without re-deriving it. The archives are ABI- and API-specific and nothing
# in their filenames says so.
cat > "$STAGE/BUILD-INFO" <<EOF
sfml_version=3.1.0
modules=system,window,main
abi=$ABI
api_level=$API
ndk=$(basename "$NDK")
built=$(date -u +%Y-%m-%dT%H:%M:%SZ)
host_triple=$(clang++ -dumpmachine 2>/dev/null || echo unknown)
EOF
say "  build/sfml-stage/  include/SFML, lib/, BUILD-INFO"

# ------------------------------------------------------------------ install

if [ "$INSTALL" -eq 1 ]; then
    step "install into kit.sfml"
    [ -d "$(dirname "$KIT")" ] || die "kit.sfml does not exist yet at resources/kits/kit.sfml"
    mkdir -p "$KIT/include" "$KIT/lib"
    rm -rf "$KIT/include/SFML"
    cp -r "$STAGE/include/SFML" "$KIT/include/"
    cp "$STAGE"/lib/*.a "$KIT/lib/"
    cp "$STAGE/BUILD-INFO" "$KIT/"
    say "  copied into resources/kits/kit.sfml/tree/sq_kit/"
    say ""
    say "  These are aarch64 binaries in a tracked directory. .gitignore must"
    say "  carry, anchored to the repository root:"
    say ""
    say "    /resources/kits/kit.sfml/tree/sq_kit/lib/"
    say "    /resources/kits/kit.sfml/tree/sq_kit/include/SFML/"
    say "    /resources/kits/kit.sfml/tree/sq_kit/BUILD-INFO"
    say ""
    say "  Check with: git status --short resources/kits/kit.sfml"
fi

step "next"
if [ "$INSTALL" -eq 1 ]; then
    say "  build/sqpg initialize"
    say "  sqpg new droid -t template.android.sfml -k kit.sfml \\"
    say "      -p project_name=droid -p package_name=com.example.droid"
else
    say "  --install to copy into kit.sfml for local testing"
fi
