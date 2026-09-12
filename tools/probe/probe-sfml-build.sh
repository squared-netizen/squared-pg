#!/usr/bin/env bash
# probe-sfml-build.sh — can this host build SFML for Android?
#
# Answers, in order of how much they matter:
#   1. what toolchain is present (cmake, clang target, NDK layout)
#   2. does a plain configure pick the WRONG backend (X11)?
#   3. does -DANDROID=1 pick the RIGHT one (Android)?
#   4. if so, does System+Window actually compile, and to what?
#
# Configures System and Window only — Graphics, Audio and Network off — so
# none of the vendored dependencies are touched. Read-only apart from a build
# directory under $TMPDIR. Writes ./sfml-build-probe.md
#
# Usage: tools/probe/probe-sfml-build.sh [path/to/SFML-3.1.0]
#
# The default is third-party/SFML-3.1.0 relative to the repo root. Note that
# third-party/sfml-deps holds SFML's DEPENDENCIES, not SFML: pointing this at
# that directory is a mistake the script now refuses rather than reports four
# times.

set -uo pipefail

# ------------------------------------------------------------------ locate

find_root() {
	    local d; d="$(cd "${1:-$PWD}" 2>/dev/null && pwd)" || return 1
    while [ "$d" != "/" ]; do
        [ -d "$d/resources" ] && [ -d "$d/lua/workflows" ] && { printf '%s\n' "$d"; return 0; }
        d="$(dirname "$d")"
    done
    return 1
}

ROOT="$(find_root "$PWD")" || ROOT="$PWD"
SFML="${1:-$ROOT/third-party/SFML-3.1.0}"

die() { printf 'probe: %s\n' "$*" >&2; exit 1; }

[ -d "$SFML" ] || die "no directory at $SFML"
SFML="$(cd "$SFML" && pwd)"

# Validate the tree BEFORE doing anything. A directory that merely exists is
# not an SFML checkout, and running four phases against one produces three
# 'undetermined' verdicts and no useful information.
if [ ! -f "$SFML/CMakeLists.txt" ]; then
    printf 'probe: %s has no CMakeLists.txt — this is not an SFML source tree.\n' "$SFML" >&2
    printf '\n' >&2
    if [ -d "$SFML/freetype" ] || [ -d "$SFML/harfbuzz" ] || [ -d "$SFML/SheenBidi" ]; then
        printf "       It looks like sfml-deps, which holds SFML's dependencies.\n" >&2
        printf '       SFML itself is the sibling directory.\n' >&2
    fi
    printf '\n       Try:  %s %s\n' "$0" "$ROOT/third-party/SFML-3.1.0" >&2
    exit 2
fi
if [ ! -d "$SFML/src/SFML/Window/Android" ]; then
    die "$SFML/src/SFML/Window/Android is missing; this tree is not SFML 3.x or is incomplete"
fi

for tool in cmake clang++; do
    command -v "$tool" >/dev/null 2>&1 || die "$tool is not installed"
done

REPORT="$PWD/sfml-build-probe.md"
: > "$REPORT"
say() { printf '%s\n' "$*" | tee -a "$REPORT"; }
hdr() { printf '\n## %s\n' "$*" | tee -a "$REPORT"; }
fence() { printf '```\n' | tee -a "$REPORT"; }

TMP="${TMPDIR:-/tmp}/sfml-probe.$$"
mkdir -p "$TMP" || die "cannot create $TMP"
trap 'rm -rf "$TMP"' EXIT

say "# SFML Android build probe"
say ""
say "- sfml:   $SFML"
say "- sfml version: $(grep -m1 -oE 'VERSION [0-9]+\.[0-9]+\.[0-9]+' "$SFML/CMakeLists.txt" 2>/dev/null || echo '?')"
say "- tmp:    $TMP"

# --------------------------------------------------------------- toolchain

hdr "1. toolchain"
say ""
fence
printf 'cmake      %s\n' "$(cmake --version 2>/dev/null | head -1)" | tee -a "$REPORT"
printf 'clang++    %s\n' "$(clang++ --version 2>/dev/null | head -1)" | tee -a "$REPORT"
TRIPLE="$(clang++ -dumpmachine 2>/dev/null || echo '?')"
printf 'target     %s\n' "$TRIPLE" | tee -a "$REPORT"
printf 'make       %s\n' "$(make --version 2>/dev/null | head -1 || echo 'NOT FOUND')" | tee -a "$REPORT"
printf 'ninja      %s\n' "$(ninja --version 2>/dev/null || echo 'not installed')" | tee -a "$REPORT"
fence
say ""
case "$TRIPLE" in
    *android*)
        say "The compiler already targets Android. Everything it emits is"
        say "loadable in an APK, so no cross-toolchain is required — the NDK is"
        say "needed only for headers and stub libraries Termux does not ship."
        ;;
    *)
        say "WARNING: the compiler does NOT target Android. Objects built here"
        say "will not load in an APK. A cross-toolchain is required; see"
        say "section 2 for whether one is available."
        ;;
esac

# --------------------------------------------------------------------- NDK

hdr "2. NDK"
say ""
fence
NDK="${ANDROID_NDK_HOME:-}"
if [ -z "$NDK" ]; then
    NDK="$(ls -d "${PREFIX:-/usr}/opt/android-sdk/ndk"/* "${ANDROID_HOME:-/nonexistent}/ndk"/* \
                 "${ANDROID_SDK_ROOT:-/nonexistent}/ndk"/* 2>/dev/null | sort -Vr | head -1)"
fi
printf 'ndk        %s\n' "${NDK:-NOT FOUND}" | tee -a "$REPORT"

HAVE_TOOLCHAIN_FILE=0
if [ -n "$NDK" ]; then
    for p in "toolchains/llvm/prebuilt" "build/cmake/android.toolchain.cmake"; do
        if [ -e "$NDK/$p" ]; then
            printf '  present  %s\n' "$p"
            [ "$p" = "build/cmake/android.toolchain.cmake" ] && HAVE_TOOLCHAIN_FILE=1
        else
            printf '  absent   %s\n' "$p"
        fi
    done | tee -a "$REPORT"
    HOSTDIR="$(ls -d "$NDK"/toolchains/llvm/prebuilt/* 2>/dev/null | head -1)"
    printf '  host tag %s\n' "$(basename "${HOSTDIR:-none}")" | tee -a "$REPORT"
    for h in EGL/egl.h GLES3/gl3.h KHR/khrplatform.h; do
        f="${HOSTDIR:-/nonexistent}/sysroot/usr/include/$h"
        printf '  %-20s %s\n' "$h" "$([ -f "$f" ] && echo present || echo 'NOT FOUND')" | tee -a "$REPORT"
    done
fi
fence
say ""
if [ "$HAVE_TOOLCHAIN_FILE" -eq 1 ]; then
    say "A full NDK with android.toolchain.cmake is present, so a conventional"
    say "CMake cross-compile is available as a fallback if section 3's route"
    say "misbehaves."
else
    say "No android.toolchain.cmake: this is a sysroot rather than a full NDK."
    say "A conventional cross-compile is unavailable, so section 3's route is"
    say "the only one."
fi

# ------------------------------------------------------- backend selection

hdr "3. backend selection — the decisive test"
say ""
say "On Linux, SFML sets SFML_OS_ANDROID from \`if(ANDROID)\`, not from the"
say "system name (cmake/Config.cmake). Termux's CMake reports Linux, so a"
say "plain configure selects the X11 backend, builds cleanly, and produces"
say "libraries that are wrong for an APK without reporting anything."
say ""

# Which backend a configure chose, read from the generated build system rather
# than guessed: the source list is what actually determines the answer.
detect_backend() {
	    local dir="$1" hits=""
    hits="$(grep -rhos 'Window/Android/WindowImplAndroid\|Window/Unix/WindowImplX11' \
            "$dir" 2>/dev/null | sort -u | tr '\n' ' ')"
    case "$hits" in
        *Android*Unix*|*Unix*Android*) printf 'BOTH (unexpected)\n' ;;
        *Android*)                     printf 'Android\n' ;;
        *X11*)                         printf 'X11\n' ;;
        *)                             printf 'undetermined\n' ;;
    esac
}

configure() {
	    local tag="$1"; shift
    local dir="$TMP/$tag"
    mkdir -p "$dir"
    printf '\n### configure: %s\n\n```\n' "$tag" >> "$REPORT"
    ( cd "$dir" && cmake -S "$SFML" -B . \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DSFML_BUILD_GRAPHICS=OFF \
        -DSFML_BUILD_AUDIO=OFF \
        -DSFML_BUILD_NETWORK=OFF \
        -DSFML_BUILD_EXAMPLES=OFF \
        -DSFML_BUILD_TEST_SUITE=OFF \
        "$@" ) >> "$REPORT" 2>&1
    local rc=$?
    printf '  (exit %d)\n```\n' "$rc" >> "$REPORT"

    local backend; backend="$(detect_backend "$dir")"
    say "  $tag: configure exit $rc, window backend: $backend"
    printf '%s' "$backend" > "$TMP/$tag.backend"
    return $rc
}

configure plain
configure android -DANDROID=1

PLAIN_BACKEND="$(cat "$TMP/plain.backend" 2>/dev/null || echo undetermined)"
ANDROID_BACKEND="$(cat "$TMP/android.backend" 2>/dev/null || echo undetermined)"

# ----------------------------------------------------------------- compile

hdr "4. compile"
say ""
if [ "$ANDROID_BACKEND" != "Android" ]; then
    say "Skipped: the -DANDROID=1 configure did not select the Android backend,"
    say "so compiling would only prove the wrong thing builds."
else
    fence
    for target in sfml-system sfml-window; do
        ( cd "$TMP/android" && cmake --build . --target "$target" -j"$(nproc 2>/dev/null || echo 2)" ) \
            >> "$REPORT" 2>&1
        printf '  %-12s exit %d\n' "$target" "$?" | tee -a "$REPORT"
    done
    say ""
    while IFS= read -r lib; do
        printf '  %-28s %8s bytes\n' "$(basename "$lib")" "$(wc -c < "$lib")" | tee -a "$REPORT"
        # An archive whose members are not Android objects would link and then
        # fail on device; ask the file itself rather than trusting the flags.
        obj="$(ar t "$lib" 2>/dev/null | head -1)"
        if [ -n "$obj" ]; then
            ( cd "$TMP" && ar x "$lib" "$obj" 2>/dev/null && \
              printf '    member %-20s %s\n' "$obj" \
                "$(readelf -h "$obj" 2>/dev/null | awk '/Machine:/{$1="";print $0}' | xargs)" \
              && rm -f "$obj" ) | tee -a "$REPORT"
        fi
    done < <(find "$TMP/android" -name 'libsfml-*.a' 2>/dev/null | sort)
    fence
fi

# ----------------------------------------------------------------- verdict

hdr "5. verdict"
say ""
if [ "$PLAIN_BACKEND" = "X11" ] && [ "$ANDROID_BACKEND" = "Android" ]; then
    say "**-DANDROID=1 works.** A plain configure selects X11 as predicted;"
    say "the flag selects the Android backend. The build script follows"
    say "directly: same flags, plus an install step that stages headers and"
    say "archives into kit.sfml's payload."
elif [ "$ANDROID_BACKEND" = "Android" ]; then
    say "**-DANDROID=1 works**, though the plain configure reported"
    say "'$PLAIN_BACKEND' rather than X11 — worth a glance at its log, but it"
    say "does not block the build."
elif [ "$ANDROID_BACKEND" = "undetermined" ] && [ -s "$TMP/android.backend" ]; then
    say "The -DANDROID=1 configure did not produce a readable source list."
    say "Read its log in section 3; a configure error is the usual cause."
    [ "$HAVE_TOOLCHAIN_FILE" -eq 1 ] && \
        say "Fallback available: -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake"
else
    say "-DANDROID=1 selected '$ANDROID_BACKEND', not the Android backend."
    [ "$HAVE_TOOLCHAIN_FILE" -eq 1 ] && \
        say "Use the NDK toolchain file instead: -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24"
fi
say ""
say "Report: $REPORT"
printf '\nDone. Paste %s back.\n' "$REPORT"
