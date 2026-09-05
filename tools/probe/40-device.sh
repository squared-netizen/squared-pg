#!/bin/sh
# Probe: the device itself -- ABI, API level, and how an APK gets installed.
#
# Matters for two reasons. The ABI decides the lib/<abi>/ path inside the APK,
# and the API level decides whether a classes.dex-free NativeActivity APK will
# install: older Android releases rejected an APK with no dex, newer ones do not.

set -u
. "$(dirname "$0")/_lib.sh"

section "device"

if have getprop; then
    ok "getprop" "$(where getprop)"
    for property in \
        ro.build.version.release \
        ro.build.version.sdk \
        ro.product.cpu.abi \
        ro.product.cpu.abilist \
        ro.product.model \
        ro.product.manufacturer
    do
        value=$(getprop "$property" 2>/dev/null || true)
        [ -n "$value" ] && note "$property" "$value"
    done
else
    no "getprop" "not on an Android device"
    note "uname -m" "$(uname -m 2>/dev/null || echo unknown)"
fi

section "installation path"

for tool in am pm termux-open termux-open-url adb; do
    path=$(where "$tool")
    if [ -n "$path" ]; then
        ok "$tool" "$path"
    else
        no "$tool"
    fi
done

if have termux-open; then
    note "install method" "termux-open <apk> hands it to the package installer"
elif have adb; then
    note "install method" "adb install <apk>"
else
    warn "no obvious install path; 'make apk' will produce the file and stop"
fi

section "storage"

for dir in "$HOME/storage/shared" /sdcard "$HOME/storage/downloads"; do
    if [ -d "$dir" ]; then
        ok "shared storage" "$dir"
    else
        no "shared storage" "$dir"
    fi
done

if ! [ -d "$HOME/storage" ] && have termux-setup-storage; then
    warn "run termux-setup-storage if you want the APK somewhere the installer can reach"
fi

section "existing android projects on this device"

# If a working Gradle/NDK project is already here, its android.jar and NDK
# paths are the fastest route to whatever this probe could not find.
for marker in "$HOME"/*/local.properties "$HOME"/*/*/local.properties; do
    [ -f "$marker" ] || continue
    ok "local.properties" "$marker"
    grep -E '^(sdk|ndk)\.dir' "$marker" 2>/dev/null | sed 's/^/      /' || true
done

summary "device"
