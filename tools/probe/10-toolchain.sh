#!/bin/sh
# Probe: the C/C++ toolchain and its Android targeting.
#
# What we need to learn:
#   - is clang present, and does it default to an Android triple
#   - what API level does it target by default
#   - where is its sysroot, and is a *separate* NDK sysroot installed
#
# Termux's clang already targets Android, which is why an on-device NDK build
# is plausible at all. This confirms it rather than assuming it.

set -u
. "$(dirname "$0")/_lib.sh"

section "compilers"

for compiler in clang clang++ cc c++ gcc g++; do
    path=$(where "$compiler")
    if [ -n "$path" ]; then
        ok "$compiler" "$path"
    else
        no "$compiler"
    fi
done

if have clang; then
    note "version" "$(first_line clang --version)"
    note "default target" "$(clang -dumpmachine 2>/dev/null || echo unknown)"
fi

section "android targeting"

if have clang; then
    triple=$(clang -dumpmachine 2>/dev/null || echo "")
    case "$triple" in
        *android*) ok "clang targets Android natively" "$triple" ;;
        *)         no "clang targets Android natively" "target is $triple" ;;
    esac

    # __ANDROID_API__ is what every NDK header branches on. Its value decides
    # which APIs are declared, so it matters more than the NDK version string.
    api=$(printf '__ANDROID_API__\n' | clang -E -xc - 2>/dev/null | tail -n 1)
    case "$api" in
        __ANDROID_API__|"") no "__ANDROID_API__ defined" "not an Android target" ;;
        *)                  ok "__ANDROID_API__" "$api" ;;
    esac

    sysroot=$(clang -print-resource-dir 2>/dev/null || true)
    [ -n "$sysroot" ] && note "resource dir" "$sysroot"

    # The include search path tells us where headers will actually be found,
    # which is more reliable than guessing $PREFIX layout.
    note "include search path" ""
    printf '' | clang -E -xc -v - 2>&1 \
        | sed -n '/#include <...> search starts here/,/End of search list/p' \
        | grep '^ ' | sed 's/^/      /' || true
fi

section "make and archiver"

for tool in make ar ranlib strip llvm-strip; do
    path=$(where "$tool")
    if [ -n "$path" ]; then
        ok "$tool" "$path"
    else
        no "$tool"
    fi
done

have make && note "make version" "$(first_line make --version)"

section "environment"

note "PREFIX" "${PREFIX:-<unset>}"
note "TMPDIR" "${TMPDIR:-<unset>}"
note "ANDROID_HOME" "${ANDROID_HOME:-<unset>}"
note "ANDROID_NDK_HOME" "${ANDROID_NDK_HOME:-<unset>}"
note "ANDROID_SDK_ROOT" "${ANDROID_SDK_ROOT:-<unset>}"
note "uname" "$(uname -a 2>/dev/null || echo unknown)"

summary "toolchain"
