#!/bin/sh
# Probe: build a real, installable APK end to end.
#
# This isolates the packaging question from the graphics one. The test app uses
# only android/native_activity.h and android/log.h -- both confirmed present --
# so nothing here depends on the missing EGL and GLES headers. If this
# succeeds, APK packaging is solved and only graphics remain.
#
# It also answers the risk I flagged and could not check: whether a
# NativeActivity APK with no classes.dex installs on a current Android. This
# device is SDK 36, which is exactly where I would expect trouble if there were
# any.
#
# The chain:
#   clang++ -shared              -> lib/arm64-v8a/libsqprobe.so
#   aapt2 link -I android.jar    -> base APK with binary manifest + resources
#   zip                          -> inject the .so
#   apksigner sign               -> installable
#
# Writes only under $TMPDIR, plus the finished APK where you can reach it.
# Installs nothing by itself: it prints the command and stops.

set -u
. "$(dirname "$0")/_lib.sh"

PKG=com.squared.probe
LIB=sqprobe
ABI=${ABI:-$(getprop ro.product.cpu.abi 2>/dev/null || echo arm64-v8a)}
CXX=${CXX:-clang++}

fatal() { printf '\n  \033[31mSTOP\033[0m %s\n' "$*"; exit 1; }

section "prerequisites"

for tool in "$CXX" aapt2 apksigner zip keytool; do
    if have "$tool"; then ok "$tool"; else no "$tool"; fatal "$tool is required for this probe"; fi
done
note "abi" "$ABI"

# Highest platform wins: a newer android.jar can still target an older
# minSdkVersion, and using the newest avoids "attribute not found" on anything
# recent the manifest happens to use.
ANDROID_JAR=""
for candidate in $(ls -d "${PREFIX:-/usr}/opt/android-sdk/platforms"/android-* \
                          "${ANDROID_HOME:-}/platforms"/android-* 2>/dev/null | sort -V -r); do
    [ -f "$candidate/android.jar" ] && { ANDROID_JAR="$candidate/android.jar"; break; }
done
[ -n "$ANDROID_JAR" ] || fatal "no android.jar found"
ok "android.jar" "$ANDROID_JAR"

scratch=$(probe_scratch) || fatal "could not create a scratch directory"
trap 'rm -rf "$scratch"' EXIT INT TERM
note "scratch" "$scratch"

section "1. compile the native activity"

mkdir -p "$scratch/lib/$ABI"
cat > "$scratch/probe.cpp" <<'CPP'
// Minimal NativeActivity. No graphics: this probe is about packaging, and
// mixing in EGL would make a failure ambiguous.
#include <android/native_activity.h>
#include <android/log.h>

namespace {
constexpr const char* kTag = "sqprobe";

void on_start(ANativeActivity*) {
    __android_log_print(ANDROID_LOG_INFO, kTag, "squared-pg probe: onStart");
}
void on_resume(ANativeActivity*) {
    __android_log_print(ANDROID_LOG_INFO, kTag, "squared-pg probe: onResume -- packaging works");
}
void on_destroy(ANativeActivity*) {
    __android_log_print(ANDROID_LOG_INFO, kTag, "squared-pg probe: onDestroy");
}
void on_window_created(ANativeActivity*, ANativeWindow* window) {
    __android_log_print(ANDROID_LOG_INFO, kTag, "squared-pg probe: window %p", (void*)window);
}
}  // namespace

extern "C" __attribute__((visibility("default")))
void ANativeActivity_onCreate(ANativeActivity* activity, void*, size_t) {
    __android_log_print(ANDROID_LOG_INFO, kTag, "squared-pg probe: onCreate");
    activity->callbacks->onStart = on_start;
    activity->callbacks->onResume = on_resume;
    activity->callbacks->onDestroy = on_destroy;
    activity->callbacks->onNativeWindowCreated = on_window_created;
}
CPP

if "$CXX" -std=c++20 -O2 -fPIC -shared -fvisibility=hidden \
        "$scratch/probe.cpp" -o "$scratch/lib/$ABI/lib$LIB.so" \
        -landroid -llog 2>"$scratch/err"; then
    ok "lib$LIB.so" "$(wc -c < "$scratch/lib/$ABI/lib$LIB.so") bytes"
else
    no "lib$LIB.so"
    sed 's/^/        /' "$scratch/err" | head -n 8
    fatal "the shared object did not build"
fi

section "2. link the base APK"

# android:extractNativeLibs="true" on purpose. The alternative, "false",
# requires the .so to be page-aligned inside the APK, and zipalign is not
# installed here -- Android 15+ wants 16 KiB alignment, which is not something
# to improvise. With "true" the library is stored compressed and the installer
# extracts it, so alignment never arises. Slightly larger on disk, works
# everywhere.
cat > "$scratch/AndroidManifest.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="$PKG"
          android:versionCode="1"
          android:versionName="0.1.0">

  <application android:label="squared probe"
               android:hasCode="false"
               android:extractNativeLibs="true">

    <activity android:name="android.app.NativeActivity"
              android:label="squared probe"
              android:exported="true"
              android:configChanges="orientation|keyboardHidden|screenSize">
      <meta-data android:name="android.app.lib_name" android:value="$LIB" />
      <intent-filter>
        <action android:name="android.intent.action.MAIN" />
        <category android:name="android.intent.category.LAUNCHER" />
      </intent-filter>
    </activity>
  </application>
</manifest>
XML

if aapt2 link -o "$scratch/base.apk" \
        -I "$ANDROID_JAR" \
        --manifest "$scratch/AndroidManifest.xml" \
        --min-sdk-version 24 \
        --target-sdk-version 34 \
        2>"$scratch/err"; then
    ok "aapt2 link" "$(wc -c < "$scratch/base.apk") bytes"
else
    no "aapt2 link"
    sed 's/^/        /' "$scratch/err" | head -n 10
    fatal "aapt2 could not link the manifest"
fi

section "3. inject the native library"

cp "$scratch/base.apk" "$scratch/unsigned.apk"
if ( cd "$scratch" && zip -q -X "unsigned.apk" "lib/$ABI/lib$LIB.so" ) 2>"$scratch/err"; then
    ok "lib/$ABI/lib$LIB.so added"
    unzip -l "$scratch/unsigned.apk" 2>/dev/null | sed -n '3,12p' | sed 's/^/        /'
else
    no "zip"
    sed 's/^/        /' "$scratch/err" | head -n 4
    fatal "could not add the library to the APK"
fi

section "4. sign"

KEYSTORE="$HOME/.android/debug.keystore"
if [ ! -f "$KEYSTORE" ]; then
    note "keystore" "absent; generating one"
    mkdir -p "$(dirname "$KEYSTORE")"
    keytool -genkeypair -keystore "$KEYSTORE" -alias androiddebugkey \
        -storepass android -keypass android -keyalg RSA -keysize 2048 \
        -validity 10000 -dname 'CN=Android Debug,O=Android,C=US' 2>/dev/null \
        || fatal "keytool could not create a debug keystore"
fi
ok "keystore" "$KEYSTORE"

OUT_DIR=${SQ_APK_OUT:-$HOME}
OUT="$OUT_DIR/squared-probe.apk"

if apksigner sign --ks "$KEYSTORE" --ks-pass pass:android --key-pass pass:android \
        --ks-key-alias androiddebugkey \
        --out "$OUT" "$scratch/unsigned.apk" 2>"$scratch/err"; then
    ok "apksigner sign" "$OUT"
else
    no "apksigner sign"
    sed 's/^/        /' "$scratch/err" | head -n 8
    fatal "signing failed"
fi

if apksigner verify --print-certs "$OUT" >"$scratch/verify" 2>&1; then
    ok "apksigner verify"
    grep -iE 'signer|v[0-9] scheme' "$scratch/verify" | head -n 4 | sed 's/^/        /'
else
    no "apksigner verify"
    sed 's/^/        /' "$scratch/verify" | head -n 6
fi

section "result"

ok "APK" "$OUT  ($(wc -c < "$OUT") bytes)"

cat <<NOTE

  Every packaging step succeeded. Install it by hand -- this probe will not:

      termux-open $OUT

  Then launch "squared probe" from the launcher. It draws nothing (there is no
  renderer yet); a black screen is success. Confirm with:

      logcat -d -s sqprobe

  Expect onCreate, onStart, onResume and a window pointer. If those appear,
  a classes.dex-free NativeActivity APK installs and runs on this device, and
  template.android.cpp's packaging path is settled.

  If the *install* is refused, say so -- that is the one remaining risk in the
  plan and it changes the approach rather than the details.
NOTE

summary "apk smoke"
