#!/usr/bin/env bash
#
# build-sfml.sh — build SFML's static archives for Android from vendored source.
#
#   tools/build-sfml.sh                 build and stage
#   tools/build-sfml.sh --install       ...and copy into kit.sfml's payload
#   tools/build-sfml.sh --patch         apply SFML's dependency patches, exit
#   tools/build-sfml.sh --check         report readiness, build nothing
#   tools/build-sfml.sh --clean         remove build directories
#   tools/build-sfml.sh --verbose       stream raw build output
#
#   SQ_ABI=arm64-v8a  SQ_MIN_SDK=24     target overrides
#
# Builds System, Window, Main, Graphics and Audio. Network is off: it needs
# mbedTLS and libssh2, neither vendored, for a module an Android app does not
# need to draw a frame.
#
# ==============================================================================
# WHY THIS SCRIPT IS SHAPED THIS WAY
#
# Everything below was established empirically. Each item produces a failure
# that names something other than its cause, which is why each is written down
# rather than left as a flag.
#
# --- 1. The Android backend is selected by a variable, not the system name ---
#
# SFML tests `if(ANDROID)` in cmake/Config.cmake. Termux's CMake sets it, so
# the Android branch is chosen without help — but -DANDROID=1 is passed anyway
# so the build does not depend on a Termux packaging detail.
#
# When it is NOT set, SFML selects the X11 backend, builds cleanly, and
# produces libraries that are wrong for an APK with no error anywhere. The
# verify phase checks the archive for WindowImplAndroid and against
# WindowImplX11 for that reason.
#
# --- 2. EGL and GLES must be named by absolute path ---
#
# FindEGL.cmake does `find_library(EGL_LIBRARY NAMES EGL)`. On Termux that
# finds $PREFIX/lib/libEGL.so, which belongs to libglvnd — the X11 EGL, not
# Android's. The link succeeds and the APK dies inside the package installer.
#
# A search order cannot go wrong when there is no search.
#
# --- 3. EGL_INCLUDE_DIR must NOT be the NDK sysroot ---
#
# SFML does target_link_libraries(sfml-window PRIVATE EGL::EGL), and an
# imported target propagates its includes as -isystem. Point that at the NDK
# sysroot and a second complete set of C headers lands ahead of libc++;
# <cctype> then finds the NDK's ctype.h and the build stops with "tried
# including <ctype.h> but didn't find libc++'s".
#
# mk/kit_opengl.mk solves the same hazard with -idirafter. Here the path
# arrives through an imported target's interface, so the fix has to be a
# directory: build/sfml-khronos holds symlinks to exactly the five Khronos
# directories Termux lacks, and nothing else.
#
# --- 4. No cross-toolchain ---
#
# Termux's clang reports aarch64-unknown-linux-android24 and already emits
# Android objects. The NDK's android.toolchain.cmake is deliberately unused:
# it sets CMAKE_SYSROOT and reintroduces hazard 3 wholesale.
#
# --- 5. Archive naming and link order ---
#
# Static archives are suffixed -s, except libsfml-main.a, which is not.
# Dependency archives have no suffix convention at all.
#
# Link order, left to right: main, window, graphics, audio, system, then the
# dependency archives, then the NDK stubs. Static archives resolve left to
# right, so a symbol needed by an earlier archive must be defined by a later
# one.
#
# --- 6. The dependency patches ---
#
# Graphics and Audio need six third-party libraries. SFML acquires them with
# FetchContent and every declaration carries a PATCH_COMMAND that edits the
# dependency's CMakeLists.txt. These are not cosmetic:
#
#   freetype   HARFBUZZ_FOUND -> TRUE, breaking the FreeType<->HarfBuzz cycle
#              where each wants the other to exist first
#   harfbuzz   target_link_libraries(harfbuzz ...) gains the missing PUBLIC
#   sheenbidi  add_library(SheenBidi) becomes OBJECT, folding it into
#              sfml-graphics instead of producing a separate archive
#   ogg        raises the cmake_minimum_required ceiling; CMake 4 rejects
#              declarations below 3.5
#   vorbis     stops it calling find_package(Ogg) when the target exists
#   flac       drops subdirectories that build tools nobody links
#
# FETCHCONTENT_SOURCE_DIR_<NAME> feeds FetchContent from a local tree, and
# that override skips the update stage — which is where PATCH_COMMAND lives.
# So `--patch` applies them by hand, once, and the patched trees are committed.
#
# All six are string(REPLACE)/string(REGEX REPLACE) whose output no longer
# matches their own input, so --patch is idempotent.
#
# What is not safe is a patch that silently does nothing. These are
# version-pinned literal matches: bump freetype and HARFBUZZ_FOUND may not
# appear where expected, string(REPLACE) does nothing, and the build fails
# later somewhere unrelated. A patched tree looks identical to an unpatched
# one. Hence verify_patches, which asserts each post-condition.
#
# --- 7. Dependencies must be staged INTO the build tree ---
#
# This is the subtle one, and it cost a full Graphics build.
#
# SFML knows freetype is not unity-build-safe: src/pfr/pfr.c uses `local` as a
# variable while src/gzip/zutil.h does `#define local static`, and combining
# them in one translation unit is a syntax error. So Graphics/CMakeLists.txt
# excludes pfr.c and smooth.c from the unity build — naming them as
# ${FETCHCONTENT_BASE_DIR}/freetype-src/src/pfr/pfr.c.
#
# Point FETCHCONTENT_SOURCE_DIR_FREETYPE at third-party/sfml-deps/freetype and
# that exclusion names a path that does not exist. CMake matches source-file
# properties by path STRING, not by inode, so a symlink does not help either.
# set_source_files_properties silently does nothing, pfr.c goes into the unity
# blob, and freetype fails to compile with seven errors about `local`.
#
# So each dependency is staged at build/sfml-build/_deps/<name>-src, which is
# exactly where SFML expects it, using a hardlink copy — near-instant, no
# duplicated bytes. The build writes only into _deps/<name>-build, never into
# sources, so sharing inodes with the vendored tree is safe.
# ==============================================================================

set -eu
# Deliberately no `pipefail`: the NDK search globs paths that may not exist, and
# several verification pipelines end in `head -1` or `grep -q`, which exit early
# and SIGPIPE their producer. Every pipeline's output is checked explicitly.

# ------------------------------------------------------------------ arguments

MODE=build
INSTALL=0
VERBOSE=0

while [ $# -gt 0 ]; do
    case "$1" in
        --install) INSTALL=1; shift ;;
        --patch)   MODE=patch; shift ;;
        --check)   MODE=check; shift ;;
        --clean)   MODE=clean; shift ;;
        --verbose) VERBOSE=1; shift ;;
        -h|--help) sed -n '3,16p' "$0" | sed 's/^# \{0,1\}//; s/^#$//'; exit 0 ;;
        *) printf 'build-sfml: unknown option: %s\n' "$1" >&2; exit 2 ;;
    esac
done

# -------------------------------------------------------------------- output

say()  { printf '%s\n' "$*"; }
step() { printf '\n== %s\n' "$*"; }
warn() { printf 'build-sfml: %s\n' "$*" >&2; }
die()  { printf 'build-sfml: %s\n' "$*" >&2; exit 1; }

TERM_COLS="$( { tput cols; } 2>/dev/null || echo 60 )"
case "$TERM_COLS" in ''|*[!0-9]*) TERM_COLS=60 ;; esac
[ "$TERM_COLS" -lt 24 ] && TERM_COLS=60

# Run a long command with a heartbeat.
#
# The command runs in the background writing to a log; this polls every two
# seconds and rewrites one line with elapsed time and the most recent
# interesting output. A build that takes ten minutes must prove it is alive —
# a filter that only prints when the child speaks cannot do that, because the
# child can be silent for minutes while compiling one large translation unit.
#
# $1 log path, $2 description, then the command.
run_watched() {
    local log="$1" what="$2"; shift 2
    local start now elapsed last width rc pid

    if [ "$VERBOSE" -eq 1 ]; then
        set +e
        "$@" 2>&1 | tee "$log"
        rc=${PIPESTATUS[0]}
        set -e
        return "$rc"
    fi

    : > "$log"
    "$@" > "$log" 2>&1 &
    pid=$!
    start=$(date +%s)
    width=$(( TERM_COLS - 14 ))

    while kill -0 "$pid" 2>/dev/null; do
        now=$(date +%s)
        elapsed=$(( now - start ))
        # Newest line that says something about progress. Falls back to the
        # last line of any kind, so even unrecognised output shows movement.
        last="$(grep -aE '^\[ *[0-9]+%\]|^(Building|Linking|Scanning|Generating)' "$log" 2>/dev/null | tail -1)"
        [ -n "$last" ] || last="$(tail -1 "$log" 2>/dev/null || true)"
        [ -n "$last" ] || last="$what"
        printf '\r  [%3ds] %-*.*s' "$elapsed" "$width" "$width" "$last"
        sleep 2
    done

    set +e
    wait "$pid"
    rc=$?
    set -e

    elapsed=$(( $(date +%s) - start ))
    printf '\r%*s\r' "$TERM_COLS" ''
    if [ "$rc" -eq 0 ]; then
        say "  $what: done in ${elapsed}s"
    fi
    return "$rc"
}

fail_with_log() {
    local log="$1" what="$2"
    printf '\n--- last 40 lines of %s ---\n' "${log##*/}" >&2
    tail -40 "$log" >&2
    printf -- '--- end ---\n\n' >&2
    die "$what failed; full log at $log"
}

# ------------------------------------------------------------------ locations

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$here"

SFML_SRC="$here/third-party/SFML-3.1.0"
DEPS="$here/third-party/sfml-deps"
BUILD_DIR="$here/build/sfml-build"
FC_BASE="$BUILD_DIR/_deps"
STAGE="$here/build/sfml-stage"
KHRONOS="$here/build/sfml-khronos"
KIT="$here/resources/kits/kit.sfml/tree/sq_kit"

TMPWORK="${TMPDIR:-/tmp}/build-sfml.$$"
mkdir -p "$TMPWORK"
trap 'rm -rf "$TMPWORK"' EXIT

if [ "$MODE" = clean ]; then
    rm -rf "$BUILD_DIR" "$STAGE" "$KHRONOS"
    say "removed build/sfml-build, build/sfml-stage, build/sfml-khronos"
    say "kit payload left alone; remove it by hand if one was installed"
    exit 0
fi

# --------------------------------------------------------------- dependencies
#
# Columns: FetchContent name | directory under sfml-deps | patch tool dir |
#          patch script variable
#
# The FetchContent name is what SFML declared and what CMake uppercases into
# FETCHCONTENT_SOURCE_DIR_<NAME>; it is not always the directory name.
# Freetype and HarfBuzz are declared capitalised, ogg/flac/vorbis are not.
# The staged directory name is the declared name lowercased plus "-src",
# which is what SFML's unity-build exclusions reference.

DEP_ROWS="\
Freetype|freetype|freetype|FREETYPE_DIR
HarfBuzz|harfbuzz|harfbuzz|HARFBUZZ_DIR
SheenBidi|SheenBidi|sheenbidi|SHEENBIDI_DIR
ogg|ogg|ogg|OGG_DIR
vorbis|vorbis|vorbis|VORBIS_DIR
flac|flac|flac|FLAC_DIR"

# Post-conditions: <dir>|present|<literal> or <dir>|absent|<literal>
PATCH_CHECKS="\
freetype|absent|HARFBUZZ_FOUND
harfbuzz|present|target_link_libraries(harfbuzz PUBLIC
SheenBidi|present|add_library(SheenBidi OBJECT
SheenBidi|absent|include(GNUInstallDirs)
ogg|present|cmake_minimum_required(VERSION 3.6...3.10)
vorbis|present|cmake_minimum_required(VERSION 2.8.12...3.10)
vorbis|absent|find_package(Ogg REQUIRED)"

# Reports every failure rather than the first: a half-patched dependency set is
# worth seeing whole. Writes to a file because the loop runs in a subshell.
verify_patches() {
    local report="$TMPWORK/patchcheck"
    printf '%s\n' "$PATCH_CHECKS" | while IFS='|' read -r dir kind needle; do
        [ -n "$dir" ] || continue
        local file="$DEPS/$dir/CMakeLists.txt"
        if [ ! -f "$file" ]; then
            printf '  MISSING   %-12s %s\n' "$dir" "${file#"$here"/}"
            continue
        fi
        if grep -qF -- "$needle" "$file"; then
            if [ "$kind" = present ]
                then printf '  ok        %-12s %s\n' "$dir" "$needle"
                else printf '  UNPATCHED %-12s still has: %s\n' "$dir" "$needle"
            fi
        else
            if [ "$kind" = absent ]
                then printf '  ok        %-12s no %s\n' "$dir" "$needle"
                else printf '  UNPATCHED %-12s missing: %s\n' "$dir" "$needle"
            fi
        fi
    done > "$report"
    cat "$report"
    # if/then rather than `grep -q ... && bad=1`: under set -e a failing &&
    # list aborts the script, and "nothing unpatched" is the success case.
    if grep -q 'UNPATCHED\|MISSING' "$report"; then return 1; fi
    return 0
}

apply_patches() {
    printf '%s\n' "$DEP_ROWS" | while IFS='|' read -r name dirname tooldir var; do
        [ -n "$name" ] || continue
        local target="$DEPS/$dirname"
        local script
        script="$(ls "$SFML_SRC/tools/$tooldir"/Patch*.cmake 2>/dev/null | head -1 || true)"
        if [ ! -d "$target" ]; then
            printf '  absent    %-12s expected %s\n' "$dirname" "${target#"$here"/}"
        elif [ -z "$script" ]; then
            printf '  no patch  %-12s (none shipped for it)\n' "$dirname"
        elif cmake "-D${var}=$target" -P "$script" >/dev/null 2>&1; then
            printf '  patched   %-12s %s\n' "$dirname" "$(basename "$script")"
        else
            printf '  FAILED    %-12s %s\n' "$dirname" "$(basename "$script")"
        fi
    done
}

# Stage dependencies where SFML's unity-build exclusions expect them. See item
# 7 in the header. Hardlink copy when the filesystem allows, plain copy
# otherwise; both give real paths under _deps.
stage_dependencies() {
    mkdir -p "$FC_BASE"
    printf '%s\n' "$DEP_ROWS" | while IFS='|' read -r name dirname _t _v; do
        [ -n "$name" ] || continue
        local lower dest
        lower="$(printf '%s' "$name" | tr '[:upper:]' '[:lower:]')"
        dest="$FC_BASE/${lower}-src"
        # Cleared before every attempt. A partially-populated destination makes
        # `cp -a src dest` copy INTO dest rather than as dest, nesting the tree
        # one level deeper -- which is silent, and leaves no CMakeLists.txt
        # where SFML looks for one.
        rm -rf "$dest"
        # Plain copy, not `cp -al`. Hardlinks are unavailable on Android's
        # filesystems: the link attempt creates the directory skeleton, fails
        # on the first file, and leaves exactly the partial destination the
        # rm above now guards against.
        if cp -a "$DEPS/$dirname" "$dest" 2>/dev/null \
           && [ -f "$dest/CMakeLists.txt" ]; then
            printf '  staged    %-12s -> _deps/%s-src\n' "$dirname" "$lower"
        else
            printf '  FAILED    %-12s no CMakeLists.txt at _deps/%s-src\n' "$dirname" "$lower"
        fi
    done
}

# --------------------------------------------------------------- --patch mode

if [ "$MODE" = patch ]; then
    [ -d "$SFML_SRC" ] || die "no SFML source at third-party/SFML-3.1.0"
    [ -d "$DEPS" ] || die "no dependencies at third-party/sfml-deps"
    command -v cmake >/dev/null 2>&1 || die "cmake is not installed"

    step "applying SFML's dependency patches"
    say "  These edit the vendored trees in place, and are idempotent."
    say ""
    apply_patches

    step "verifying"
    if verify_patches; then
        say ""
        say "All post-conditions hold. Commit the modified trees:"
        say ""
        say "  git add third-party/sfml-deps && git commit"
        say ""
        say "Record in third-party/README.md that these CMakeLists.txt files are"
        say "patched and why. A patched tree is indistinguishable from an"
        say "unpatched one, and divergence from the upstream tag otherwise reads"
        say "as corruption."
    else
        say ""
        die "some patches did not take; see above"
    fi
    exit 0
fi

# ------------------------------------------------------------- prerequisites

step "prerequisites"

MISSING=0
check_have() {
    if [ -e "$2" ]; then
        say "  ok        $1"
    else
        say "  MISSING   $1  ($2)"
        MISSING=$((MISSING + 1))
    fi
}
check_tool() {
    if command -v "$1" >/dev/null 2>&1; then
        say "  ok        $1"
    else
        say "  MISSING   $1"
        MISSING=$((MISSING + 1))
    fi
}

check_tool cmake
check_tool make
check_tool clang++
check_have "SFML source" "$SFML_SRC/CMakeLists.txt"
check_have "dependencies" "$DEPS"
[ "$MISSING" -eq 0 ] || die "$MISSING prerequisite(s) missing"

# ---------------------------------------------------------------------- NDK
#
# Same search order as mk/squared_generated.mk: ANDROID_NDK_HOME wins when
# set, because someone who set it meant it.

NDK="${ANDROID_NDK_HOME:-}"
if [ -z "$NDK" ]; then
    NDK="$(ls -d "${PREFIX:-/usr}/opt/android-sdk/ndk"/* \
                 "${ANDROID_HOME:-/nonexistent}/ndk"/* \
                 "${ANDROID_SDK_ROOT:-/nonexistent}/ndk"/* 2>/dev/null \
           | sort -Vr | head -1 || true)"
fi
[ -n "$NDK" ] && [ -d "$NDK" ] || die "no Android NDK found; set ANDROID_NDK_HOME"

HOSTDIR="$(ls -d "$NDK"/toolchains/llvm/prebuilt/* 2>/dev/null | head -1 || true)"
[ -n "$HOSTDIR" ] || die "$NDK has no toolchains/llvm/prebuilt"
SYSROOT="$HOSTDIR/sysroot"

API="${SQ_MIN_SDK:-24}"
ABI="${SQ_ABI:-arm64-v8a}"
case "$ABI" in
    arm64-v8a)   LIBTRIPLE=aarch64-linux-android ;;
    armeabi-v7a) LIBTRIPLE=arm-linux-androideabi ;;
    x86_64)      LIBTRIPLE=x86_64-linux-android ;;
    x86)         LIBTRIPLE=i686-linux-android ;;
    *) die "unsupported SQ_ABI: $ABI" ;;
esac
NDKLIB="$SYSROOT/usr/lib/$LIBTRIPLE/$API"

for lib in libEGL.so libGLESv1_CM.so; do
    [ -f "$NDKLIB/$lib" ] || die "no $lib at $NDKLIB (wrong API level for this ABI?)"
done

step "toolchain"
say "  ndk       $NDK"
say "  sysroot   ${SYSROOT#"$NDK"/}"
say "  abi/api   $ABI / android-$API"
say "  clang     $(clang++ -dumpmachine 2>/dev/null || echo '?')"
say "  jobs      $(nproc 2>/dev/null || echo 2)"

# --------------------------------------------------------- dependency patches

step "dependency patches"
if verify_patches; then
    say "  all post-conditions hold"
else
    say ""
    die "dependencies are not patched. Run: tools/build-sfml.sh --patch"
fi

if [ "$MODE" = check ]; then
    step "ready"
    say "  Everything needed is present. Run without --check to build."
    exit 0
fi

# ------------------------------------------------------------- Khronos dir
#
# Item 3. Only these five: Termux's clang ships every android/* header in its
# own sysroot, so the NDK supplies the Khronos headers and nothing else.
# Symlinks rather than copies, so an NDK update is picked up by rerunning this
# script rather than by remembering to refresh a copy.

step "Khronos include directory"
rm -rf "$KHRONOS"
mkdir -p "$KHRONOS"
for d in EGL GLES GLES2 GLES3 KHR; do
    [ -d "$SYSROOT/usr/include/$d" ] || die "NDK sysroot has no $d/"
    ln -s "$SYSROOT/usr/include/$d" "$KHRONOS/$d"
done
say "  build/sfml-khronos -> EGL GLES GLES2 GLES3 KHR"

# ------------------------------------------------------------- stage the deps

step "staging dependencies"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
stage_dependencies
say "  (copied into the build tree so SFML's unity-build exclusions resolve)"

FC_ARGS=""
while IFS='|' read -r name _d _t _v; do
    [ -n "$name" ] || continue
    upper="$(printf '%s' "$name" | tr '[:lower:]' '[:upper:]')"
    lower="$(printf '%s' "$name" | tr '[:upper:]' '[:lower:]')"
    FC_ARGS="$FC_ARGS -DFETCHCONTENT_SOURCE_DIR_${upper}=$FC_BASE/${lower}-src"
done <<EOF
$DEP_ROWS
EOF

# ------------------------------------------------------------------ configure

step "configure"
# shellcheck disable=SC2086
set -- cmake -S "$SFML_SRC" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DANDROID=1 \
    -DSFML_BUILD_GRAPHICS=ON \
    -DSFML_BUILD_AUDIO=ON \
    -DSFML_BUILD_NETWORK=OFF \
    -DSFML_BUILD_EXAMPLES=OFF \
    -DSFML_BUILD_TEST_SUITE=OFF \
    -DSFML_USE_SYSTEM_DEPS=OFF \
    $FC_ARGS \
    -DEGL_INCLUDE_DIR="$KHRONOS" \
    -DEGL_LIBRARY="$NDKLIB/libEGL.so" \
    -DGLES_INCLUDE_DIR="$KHRONOS" \
    -DGLES_LIBRARY="$NDKLIB/libGLESv1_CM.so" \
    -Wno-dev
run_watched "$BUILD_DIR/configure.log" "configure" "$@" \
    || fail_with_log "$BUILD_DIR/configure.log" "configure"

# ---------------------------------------------------------------------- build

step "build"
say "  five SFML modules and five dependency libraries; expect several minutes"
run_watched "$BUILD_DIR/build.log" "build" \
    cmake --build "$BUILD_DIR" \
          --target sfml-system sfml-window sfml-main sfml-graphics sfml-audio \
          -j"$(nproc 2>/dev/null || echo 2)" \
    || fail_with_log "$BUILD_DIR/build.log" "build"

# -------------------------------------------------------------------- collect

step "archives"

SFML_LIBS="libsfml-main.a libsfml-window-s.a libsfml-graphics-s.a \
           libsfml-audio-s.a libsfml-system-s.a"
for lib in $SFML_LIBS; do
    [ -f "$BUILD_DIR/lib/$lib" ] || die "expected $lib was not produced"
    say "  $(printf '%-26s %9s bytes' "$lib" "$(wc -c < "$BUILD_DIR/lib/$lib")")"
done

# Dependency archives land wherever each project put them. Found by search
# rather than a list: the set depends on how each dependency names its targets,
# and SheenBidi produces none — it is patched to an OBJECT library and folds
# into sfml-graphics.
find "$BUILD_DIR/lib" ! -name "libsfml-*" -name '*.a' -type f 2>/dev/null | sort > "$TMPWORK/deps.txt" || true
DEP_COUNT="$(wc -l < "$TMPWORK/deps.txt" | tr -d ' ')"
[ "$DEP_COUNT" -gt 0 ] || die "no dependency archives in lib/; Graphics and Audio cannot link"
while IFS= read -r a; do
    say "  $(printf '%-26s %9s bytes' "$(basename "$a")" "$(wc -c < "$a")")"
done < "$TMPWORK/deps.txt"

# --------------------------------------------------------------------- verify
#
# Each check catches a failure that otherwise appears only when an APK is
# installed on a device and dies before any of its code runs.

step "verify"

# Android resolves this symbol by name at load time. Nothing in a generated
# project references it, so without -Wl,-u the linker drops it: the link
# succeeds, the APK installs, and the process dies at launch silently.
if nm --defined-only "$BUILD_DIR/lib/libsfml-main.a" 2>/dev/null \
     | grep -q 'T ANativeActivity_onCreate'; then
    say "  ok        ANativeActivity_onCreate defined"
else
    die "libsfml-main.a does not define ANativeActivity_onCreate"
fi

# An archive of host objects would link and then fail on the device.
member="$(ar t "$BUILD_DIR/lib/libsfml-window-s.a" | head -1)"
( cd "$TMPWORK" && ar x "$BUILD_DIR/lib/libsfml-window-s.a" "$member" )
machine="$(readelf -h "$TMPWORK/$member" 2>/dev/null | awk -F: '/Machine/{print $2}' | xargs || true)"
case "$machine" in
    AArch64|*aarch64*|*ARM*) say "  ok        objects are $machine" ;;
    *) die "archives contain '$machine' objects, not a target architecture" ;;
esac

# The backend that actually compiled, not the one the configure log claimed.
if ar t "$BUILD_DIR/lib/libsfml-window-s.a" | grep -q 'WindowImplX11'; then
    die "libsfml-window-s.a contains the X11 backend"
fi
if ar t "$BUILD_DIR/lib/libsfml-window-s.a" | grep -q 'WindowImplAndroid'; then
    say "  ok        Android window backend"
else
    die "libsfml-window-s.a has no WindowImplAndroid; wrong backend"
fi

# Graphics without freetype would link and then fail on the first sf::Font.
if ar t "$BUILD_DIR/lib/libsfml-graphics-s.a" | grep -q 'Font'; then
    say "  ok        Graphics includes font support"
else
    warn "libsfml-graphics-s.a has no Font object; text rendering will not work"
fi

# ---------------------------------------------------------------------- stage

step "stage"
rm -rf "$STAGE"
mkdir -p "$STAGE/include" "$STAGE/lib"
cp -r "$SFML_SRC/include/SFML" "$STAGE/include/"
cp "$BUILD_DIR"/lib/libsfml-*.a "$STAGE/lib/"
# Flattened deliberately: the kit's make fragment names each archive by path,
# and a layout mirroring _deps/ would encode CMake's internal directory names
# into that fragment.
while IFS= read -r a; do
    [ -n "$a" ] && cp "$a" "$STAGE/lib/"
done < "$TMPWORK/deps.txt"

# A stamp, because the archives are ABI- and API-specific and nothing in their
# filenames says so.
{
    printf 'sfml_version=3.1.0\n'
    printf 'modules=system,window,main,graphics,audio\n'
    printf 'abi=%s\n' "$ABI"
    printf 'api_level=%s\n' "$API"
    printf 'ndk=%s\n' "$(basename "$NDK")"
    printf 'built=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    printf 'host_triple=%s\n' "$(clang++ -dumpmachine 2>/dev/null || echo unknown)"
    printf 'archives=%s\n' "$(cd "$STAGE/lib" && ls *.a | tr '\n' ' ')"
} > "$STAGE/BUILD-INFO"

say "  build/sfml-stage/include/SFML"
say "  build/sfml-stage/lib          $(ls "$STAGE/lib" | wc -l | tr -d ' ') archives, $(du -sh "$STAGE/lib" | cut -f1)"
say "  build/sfml-stage/BUILD-INFO"

# -------------------------------------------------------------------- install

if [ "$INSTALL" -eq 1 ]; then
    step "install into kit.sfml"
    [ -d "$(dirname "$KIT")" ] || die "kit.sfml not found at resources/kits/kit.sfml"
    mkdir -p "$KIT/include" "$KIT/lib"
    rm -rf "$KIT/include/SFML" "$KIT/lib"
    mkdir -p "$KIT/lib"
    cp -r "$STAGE/include/SFML" "$KIT/include/"
    cp "$STAGE"/lib/*.a "$KIT/lib/"
    cp "$STAGE/BUILD-INFO" "$KIT/"
    say "  copied into resources/kits/kit.sfml/tree/sq_kit/"
    say ""
    say "  These are $ABI binaries in a tracked directory. .gitignore must"
    say "  carry, anchored to the repository root of whichever repo owns"
    say "  resources/kits — that is 'squared', not 'squared-pg':"
    say ""
    say "    /kits/kit.sfml/tree/sq_kit/lib/"
    say "    /kits/kit.sfml/tree/sq_kit/include/SFML/"
    say "    /kits/kit.sfml/tree/sq_kit/BUILD-INFO"
    say ""
    say "  Verify with:  git -C resources/.squared status --short"
fi

# ----------------------------------------------------------------------- next

step "next"
if [ "$INSTALL" -eq 1 ]; then
    say "  mk/kit_sfml.mk must name every archive above, in link order:"
    say "    main, window, graphics, audio, system,"
    say "    then the dependency archives, then the NDK stubs."
    say ""
    say "  Graphics also links z, and Audio links OpenSLES — both from the NDK,"
    say "  by absolute path. \$PREFIX/lib/libz.so is Termux's, not Android's."
    say ""
    say "  build/sqpg initialize"
    say "  sqpg new droid -t template.android.sfml -k kit.sfml \\"
    say "      -p project_name=droid -p package_name=com.example.droid"
    say "  cd droid && make sfml-status"
else
    say "  --install   copy into kit.sfml for local testing"
    say "  --check     re-run the readiness checks only"
fi
