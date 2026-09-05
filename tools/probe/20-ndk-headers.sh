#!/bin/sh
# Probe: NDK headers and libraries needed for a NativeActivity + GLES 3.0 app.
#
# The decisive question for template.android.cpp is whether these are reachable
# on-device without installing the full NDK. On Termux they come from the
# `ndk-sysroot` package.
#
# android_native_app_glue is the one that usually is not: the NDK ships it as
# *source* under sources/android/native_app_glue/, not as a header on the
# include path. If it is absent, the kit vendors it -- which is the plan
# regardless, but knowing decides whether that is a convenience or a necessity.

set -u
. "$(dirname "$0")/_lib.sh"

# find_header <header> -- prints the resolved path, or nothing
find_header() {
    _h=$1
    if have clang; then
        # Ask the compiler rather than guessing a path: it knows its own search
        # order, including anything a Termux patch adds.
        _out=$(printf '#include <%s>\nint main(void){return 0;}\n' "$_h" \
               | clang -E -xc - -MM -MG 2>/dev/null | tr ' \\' '\n\n' | grep "$_h" | head -n 1)
        [ -n "$_out" ] && [ -f "$_out" ] && { printf '%s' "$_out"; return 0; }
    fi
    for _root in "${PREFIX:-/usr}/include" /usr/include "${ANDROID_NDK_HOME:-}/sysroot/usr/include"; do
        [ -f "$_root/$_h" ] && { printf '%s' "$_root/$_h"; return 0; }
    done
    return 1
}

check_header() {
    _path=$(find_header "$1") && ok "$1" "$_path" || no "$1"
}

section "android platform headers"

check_header android/native_activity.h
check_header android/looper.h
check_header android/log.h
check_header android/asset_manager.h
check_header android/native_window.h
check_header android/configuration.h
check_header android/input.h

section "graphics headers"

check_header EGL/egl.h
check_header EGL/eglext.h
check_header GLES3/gl3.h
check_header GLES3/gl3ext.h
check_header GLES2/gl2.h
check_header KHR/khrplatform.h

section "native app glue"

glue_found=0
for candidate in \
    "${PREFIX:-/usr}/include/android_native_app_glue.h" \
    "${ANDROID_NDK_HOME:-}/sources/android/native_app_glue/android_native_app_glue.h" \
    "${ANDROID_NDK_HOME:-}/sources/android/native_app_glue/android_native_app_glue.c"
do
    [ -n "$candidate" ] || continue
    if [ -f "$candidate" ]; then
        ok "android_native_app_glue" "$candidate"
        glue_found=1
    fi
done
[ "$glue_found" -eq 0 ] && no "android_native_app_glue" "kit.opengl will vendor it (expected)"

section "link libraries"

# The .so links against these by name. A missing stub library is fatal at link
# time, so this is worth confirming separately from the headers.
for lib in android log EGL GLESv3 GLESv2 m dl z; do
    found=""
    for dir in "${PREFIX:-/usr}/lib" /usr/lib /system/lib64 /system/lib; do
        [ -d "$dir" ] || continue
        for ext in so a; do
            if [ -e "$dir/lib$lib.$ext" ]; then
                found="$dir/lib$lib.$ext"
                break 2
            fi
        done
    done
    if [ -n "$found" ]; then
        ok "lib$lib" "$found"
    else
        no "lib$lib"
    fi
done

section "package hints"

if have pkg; then
    note "termux pkg" "present"
    for package in ndk-sysroot ndk-multilib clang make aapt2 apksigner openjdk-17; do
        if pkg list-installed 2>/dev/null | grep -q "^$package/"; then
            ok "$package" "installed"
        else
            no "$package" "pkg install $package"
        fi
    done
else
    note "termux pkg" "not present (not Termux)"
fi

summary "ndk"
