#!/bin/sh
# Probe: the C++ runtime, which the APK must carry.
#
# 80 showed the explicit NDK link produces NEEDED libc++_shared.so. Android
# does not ship that -- the NDK expects an app to bundle it -- so either it
# goes into the APK beside the app's own .so, or the runtime is linked
# statically and the dependency disappears.
#
# The probe APK never hit this because it used no C++ library. The real
# template will, so this decides one line of mk/kit_opengl.mk.

set -u
. "$(dirname "$0")/_lib.sh"

CXX=${CXX:-clang++}
API=${API:-24}

NDK=""
for candidate in $(ls -d "${PREFIX:-/usr}/opt/android-sdk/ndk"/* 2>/dev/null | sort -V -r); do
    [ -d "$candidate/toolchains/llvm/prebuilt" ] && { NDK="$candidate"; break; }
done
[ -n "$NDK" ] || { warn "no NDK"; exit 0; }

SYSROOT=""
for host in linux-arm64 linux-x86_64; do
    [ -d "$NDK/toolchains/llvm/prebuilt/$host/sysroot" ] && \
        SYSROOT="$NDK/toolchains/llvm/prebuilt/$host/sysroot" && break
done
[ -n "$SYSROOT" ] || { warn "no sysroot"; exit 0; }

section "where is libc++_shared.so"

# The NDK puts it in the ABI directory, not the API-level one, which is a
# distinction worth confirming rather than assuming.
for candidate in \
    "$SYSROOT/usr/lib/aarch64-linux-android/libc++_shared.so" \
    "$SYSROOT/usr/lib/aarch64-linux-android/$API/libc++_shared.so" \
    "${PREFIX:-/usr}/lib/libc++_shared.so"
do
    if [ -f "$candidate" ]; then
        ok "found" "$candidate  ($(wc -c < "$candidate") bytes)"
    else
        no "absent" "$candidate"
    fi
done

scratch=$(probe_scratch) || exit 0
trap 'rm -rf "$scratch"' EXIT INT TERM

cat > "$scratch/p.cpp" <<'CPP'
// Enough C++ library to force a real runtime dependency: a plain function
// using only headers would link without touching libc++ at all.
#include <string>
#include <vector>
#include <android/log.h>
extern "C" __attribute__((visibility("default"))) void probe() {
    std::vector<std::string> items{"a", "b"};
    items.push_back(std::string("c") + "d");
    __android_log_print(ANDROID_LOG_INFO, "p", "%zu", items.size());
}
CPP

NDK_LIB="$SYSROOT/usr/lib/aarch64-linux-android/$API"

section "shared runtime (default)"

if "$CXX" -std=c++20 -shared -fPIC "$scratch/p.cpp" -o "$scratch/shared.so" \
        -L"$NDK_LIB" -llog 2>"$scratch/err"; then
    ok "links"
    have readelf && note "NEEDED" "$(readelf -d "$scratch/shared.so" 2>/dev/null \
        | sed -n 's/.*NEEDED.*\[\(.*\)\]/\1/p' | tr '\n' ' ')"
    note "size" "$(wc -c < "$scratch/shared.so") bytes"
else
    no "links"
    sed 's/^/        /' "$scratch/err" | head -n 5
fi

section "static runtime (-static-libstdc++)"

# If this works and drops libc++_shared.so from NEEDED, the APK carries one
# file instead of two and there is nothing to get wrong at packaging time.
if "$CXX" -std=c++20 -shared -fPIC -static-libstdc++ "$scratch/p.cpp" -o "$scratch/static.so" \
        -L"$NDK_LIB" -llog 2>"$scratch/err"; then
    ok "links"
    needed=$(readelf -d "$scratch/static.so" 2>/dev/null \
        | sed -n 's/.*NEEDED.*\[\(.*\)\]/\1/p' | tr '\n' ' ')
    note "NEEDED" "$needed"
    note "size" "$(wc -c < "$scratch/static.so") bytes"
    if printf '%s' "$needed" | grep -q 'libc++_shared'; then
        warn "still needs libc++_shared.so -- the APK must carry it"
    else
        ok "no libc++_shared dependency" "the APK carries one .so"
    fi
else
    no "links"
    sed 's/^/        /' "$scratch/err" | head -n 5
    warn "static runtime unavailable; the APK must carry libc++_shared.so"
fi

summary "libc++"
