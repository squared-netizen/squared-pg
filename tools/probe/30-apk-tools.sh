#!/bin/sh
# Probe: APK packaging toolchain.
#
# This is the decisive script. Building a .so on-device is well understood;
# turning it into an installable APK needs a second toolchain, and the piece
# that usually is not available without the full Android SDK is android.jar --
# aapt2 link requires one to resolve framework resource IDs.
#
# The chain is:
#   aapt2 compile   res/ -> flat files
#   aapt2 link      flat files + AndroidManifest.xml + android.jar -> base APK
#   (copy lib/<abi>/lib<name>.so into the APK)
#   zipalign        4-byte align
#   apksigner       sign with a debug key (keytool makes one)
#
# d8 is only needed if there is Java. A pure NativeActivity app has none, so
# its absence is not fatal -- but some installers historically disliked an APK
# with no classes.dex, which is what 40-device.sh checks for.

set -u
. "$(dirname "$0")/_lib.sh"

section "packaging tools"

for tool in aapt2 aapt zipalign apksigner d8 dx bundletool; do
    path=$(where "$tool")
    if [ -n "$path" ]; then
        ok "$tool" "$path"
    else
        no "$tool"
    fi
done

have aapt2     && note "aapt2 version" "$(first_line aapt2 version)"
have apksigner && note "apksigner version" "$(first_line apksigner --version)"
have zipalign  && note "zipalign" "$(first_line zipalign 2>&1)"

section "jvm (needed for keytool and apksigner)"

for tool in java keytool jarsigner; do
    path=$(where "$tool")
    if [ -n "$path" ]; then
        ok "$tool" "$path"
    else
        no "$tool"
    fi
done

have java && note "java version" "$(java -version 2>&1 | head -n 1)"
note "JAVA_HOME" "${JAVA_HOME:-<unset>}"

section "android.jar (the crunch)"

# Hunted rather than assumed: aapt2 link needs one, and where it lives on a
# Termux install without the SDK is precisely what this probe exists to answer.
jar_found=0
for candidate in \
    "${ANDROID_HOME:-}/platforms"/*/android.jar \
    "${ANDROID_SDK_ROOT:-}/platforms"/*/android.jar \
    "${PREFIX:-/usr}/share/aapt2"/*.jar \
    "${PREFIX:-/usr}/share/android-sdk/platforms"/*/android.jar \
    "${PREFIX:-/usr}/opt/android-sdk/platforms"/*/android.jar \
    "${PREFIX:-/usr}/lib/android-sdk/platforms"/*/android.jar \
    "$HOME/android-sdk/platforms"/*/android.jar
do
    if [ -f "$candidate" ]; then
        ok "android.jar" "$candidate"
        jar_found=$((jar_found + 1))
    fi
done

if [ "$jar_found" -eq 0 ]; then
    no "android.jar" "not found in the usual places"
    warn "aapt2 link needs one; a wider search may be worth running:"
    warn "  find \$PREFIX \$HOME -name 'android*.jar' -size +1M 2>/dev/null | head"
    warn "if there is genuinely none, packaging needs a different approach --"
    warn "see the note this probe prints at the end"
fi

# Some Termux setups ship a stripped framework stub under a different name.
section "framework stubs (fallbacks for android.jar)"

for pattern in \
    "${PREFIX:-/usr}/share"/*/android*.jar \
    "${PREFIX:-/usr}/share"/*/framework*.jar \
    /system/framework/framework-res.apk
do
    [ -e "$pattern" ] && ok "candidate" "$pattern"
done

section "signing key"

for keystore in "$HOME/.android/debug.keystore" "$HOME/.config/squared-pg/debug.keystore"; do
    if [ -f "$keystore" ]; then
        ok "debug keystore" "$keystore"
    else
        no "debug keystore" "$keystore"
    fi
done

if ! [ -f "$HOME/.android/debug.keystore" ] && have keytool; then
    warn "keytool is present, so one can be generated on first 'make apk':"
    warn "  keytool -genkeypair -keystore ~/.android/debug.keystore \\"
    warn "    -alias androiddebugkey -storepass android -keypass android \\"
    warn "    -keyalg RSA -validity 10000 -dname 'CN=Android Debug'"
fi

section "archive tools (an APK is a zip)"

for tool in zip unzip 7z python3; do
    path=$(where "$tool")
    if [ -n "$path" ]; then
        ok "$tool" "$path"
    else
        no "$tool"
    fi
done

# If aapt2 is unavailable but python3 is, a minimal APK can be assembled with
# zipfile plus a pre-built binary AndroidManifest. Ugly, and worth knowing is
# possible before concluding that packaging cannot be done at all.
have python3 && note "python3" "$(first_line python3 --version)"

summary "packaging"

cat <<'NOTE'

  If android.jar is absent, the options are, in order of preference:

    1. a 'platform-tools'-free SDK slice -- android.jar alone is ~40 MB and
       can be copied from any machine that has the SDK
    2. aapt2 link --auto-add-overlay with no framework, viable only if the
       manifest references no framework resources (@android:style/... etc.)
       -- which a NativeActivity manifest can just about manage
    3. assemble the APK by hand: a pre-compiled binary AndroidManifest.xml
       shipped in the template, plus zip and apksigner

  Option 2 is the one worth testing, because it would make packaging
  dependency-free. 50-compile-checks.sh tries it.
NOTE
