#!/bin/sh
# Probe: where are the EGL and GLES headers?
#
# 20/50 found every android/* header and every link library -- libEGL.so and
# libGLESv2.so are in $PREFIX/lib, libGLESv3.so in /system/lib64 -- but no
# EGL/egl.h, GLES3/gl3.h or KHR/khrplatform.h anywhere on the include path.
#
# Libraries without headers is an odd state and usually means one of three
# things: the headers belong to a package that is not installed, they are
# installed somewhere off the default include path, or this ndk-sysroot build
# genuinely omits them. Each has a different fix, so this narrows it down
# instead of guessing.

set -u
. "$(dirname "$0")/_lib.sh"

section "filesystem hunt"

# Deliberately wide. The headers are small and the tree is not, but a wrong
# guess here costs a round trip, and a full find costs seconds.
found_any=0
for name in egl.h eglplatform.h khrplatform.h gl3.h gl2.h gl31.h gl32.h; do
    hits=$(find "${PREFIX:-/usr}" /system/include /vendor/include -name "$name" 2>/dev/null | head -n 5)
    if [ -n "$hits" ]; then
        found_any=1
        ok "$name"
        printf '%s\n' "$hits" | sed 's/^/        /'
    else
        no "$name"
    fi
done

if [ "$found_any" -eq 0 ]; then
    warn "no Khronos headers anywhere under \$PREFIX -- they are not merely off-path"
fi

section "which package owns the libraries"

# The .so files came from somewhere. That package, or its -dev sibling, is the
# most likely home for the headers.
for lib in libEGL.so libGLESv2.so libGLESv3.so libandroid.so; do
    for dir in "${PREFIX:-/usr}/lib" /system/lib64; do
        [ -e "$dir/$lib" ] || continue
        owner=""
        if have dpkg; then
            owner=$(dpkg -S "$dir/$lib" 2>/dev/null | cut -d: -f1)
        fi
        if [ -n "$owner" ]; then
            ok "$lib" "$dir  <- package: $owner"
        else
            note "$lib" "$dir  (no owning package; system or untracked)"
        fi
    done
done

section "what ndk-sysroot actually ships"

if have pkg; then
    # `pkg files` is the authoritative answer to "should these be here".
    listing=$(pkg files ndk-sysroot 2>/dev/null | grep -iE '/(EGL|GLES[0-9]*|KHR)/' | head -n 20)
    if [ -n "$listing" ]; then
        ok "ndk-sysroot ships Khronos headers"
        printf '%s\n' "$listing" | sed 's/^/        /'
        warn "they are shipped but were not found -- check the paths above"
    else
        no "ndk-sysroot ships Khronos headers" "this build omits them"
    fi

    section "candidate packages"

    for candidate in libglvnd libglvnd-dev mesa mesa-dev angle-android \
                     libandroid-shmem ndk-multilib vulkan-headers; do
        if pkg list-installed 2>/dev/null | grep -q "^$candidate/"; then
            ok "$candidate" "installed"
        elif pkg search "^$candidate\$" 2>/dev/null | grep -q "$candidate"; then
            no "$candidate" "available: pkg install $candidate"
        else
            no "$candidate" "not in the repositories"
        fi
    done

    section "repository search"

    for term in egl gles opengl khronos; do
        note "pkg search $term" ""
        pkg search "$term" 2>/dev/null | grep -iE '^[a-z0-9.+-]+/' | head -n 6 | sed 's/^/      /'
    done
else
    note "pkg" "not present; skipping package questions"
fi

section "compile against candidate include directories"

# If the headers exist somewhere unusual, adding -I would be the whole fix, and
# kit.opengl's build fragment can do exactly that. Test it rather than assume.
if have clang++; then
    scratch=$(probe_scratch) || exit 0
    trap 'rm -rf "$scratch"' EXIT INT TERM
    printf '#include <EGL/egl.h>\n#include <GLES3/gl3.h>\nint main(){return 0;}\n' > "$scratch/p.cpp"

    for candidate in \
        "${PREFIX:-/usr}/include" \
        "${PREFIX:-/usr}/include/aarch64-linux-android" \
        "${PREFIX:-/usr}/opt/android-sdk/ndk"/*/toolchains/llvm/prebuilt/*/sysroot/usr/include \
        /system/include
    do
        [ -d "$candidate" ] || continue
        if clang++ -std=c++20 -c "$scratch/p.cpp" -o "$scratch/p.o" -I"$candidate" 2>/dev/null; then
            ok "compiles with -I" "$candidate"
        else
            no "compiles with -I" "$candidate"
        fi
    done
fi

summary "graphics hunt"

cat <<'NOTE'

  If the headers are genuinely absent from every package, the fix is for
  kit.opengl to vendor the Khronos registry headers -- EGL/egl.h,
  EGL/eglplatform.h, KHR/khrplatform.h, GLES3/gl3.h, GLES3/gl3platform.h.
  They are ~6000 lines total, Apache-2.0 from the Khronos registry, and are
  exactly the kind of thing third-party/ exists for. The libraries are already
  present, so headers alone would close the gap.

  I cannot fetch them (no network), so if it comes to that I will need you to
  drop them in, or point me at a copy already on the device.
NOTE
