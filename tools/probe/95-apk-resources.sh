#!/bin/sh
# Probe: the resource compilation step, which 70 skipped entirely.
#
# 70-apk-smoke.sh built an APK with NO res/ directory -- it went straight to
# `aapt2 link --manifest`. The real template has an icon, a label and a colour,
# which means the two-stage path:
#
#     aapt2 compile --dir res -o compiled.zip
#     aapt2 link -I android.jar --manifest ... compiled.zip -o base.apk
#
# and a manifest referencing @mipmap/ic_launcher and @string/app_name, which
# only resolve if stage one produced them.
#
# This is standard and well documented. So was linking -lEGL, which bound the
# wrong library. Verify it.
#
# Also exercises the adaptive-icon XML the plan relies on: a vector drawable
# plus mipmap-anydpi-v26, so the default icon is text rather than a binary blob
# no one can review.

set -u
. "$(dirname "$0")/_lib.sh"

PKG=com.squared.resprobe
LIB=sqresprobe
ABI=${ABI:-$(getprop ro.product.cpu.abi 2>/dev/null || echo arm64-v8a)}
CXX=${CXX:-clang++}

fatal() { printf '\n  \033[31mSTOP\033[0m %s\n' "$*"; exit 1; }

section "prerequisites"
for tool in "$CXX" aapt2 apksigner zip keytool; do
    have "$tool" && ok "$tool" || fatal "$tool required"
done

ANDROID_JAR=""
for candidate in $(ls -d "${PREFIX:-/usr}/opt/android-sdk/platforms"/android-* 2>/dev/null | sort -V -r); do
    [ -f "$candidate/android.jar" ] && { ANDROID_JAR="$candidate/android.jar"; break; }
done
[ -n "$ANDROID_JAR" ] || fatal "no android.jar"
ok "android.jar" "$ANDROID_JAR"

scratch=$(probe_scratch) || fatal "no scratch dir"
trap 'rm -rf "$scratch"' EXIT INT TERM

section "1. author the resource tree"

mkdir -p "$scratch/res/values" "$scratch/res/drawable" "$scratch/res/mipmap-anydpi-v26" \
         "$scratch/res/mipmap-hdpi" "$scratch/lib/$ABI"

cat > "$scratch/res/values/strings.xml" <<'XML'
<?xml version="1.0" encoding="utf-8"?>
<resources>
  <string name="app_name">squared res probe</string>
</resources>
XML

cat > "$scratch/res/values/colors.xml" <<'XML'
<?xml version="1.0" encoding="utf-8"?>
<resources>
  <color name="ic_launcher_background">#1B5E20</color>
</resources>
XML

# A vector drawable, not a PNG. The whole point of the icon decision in the
# plan: the default is text, diffable, and parameterisable by project name.
cat > "$scratch/res/drawable/ic_launcher_foreground.xml" <<'XML'
<?xml version="1.0" encoding="utf-8"?>
<vector xmlns:android="http://schemas.android.com/apk/res/android"
        android:width="108dp" android:height="108dp"
        android:viewportWidth="108" android:viewportHeight="108">
  <path android:fillColor="#FFFFFF"
        android:pathData="M34,34h16v40h-16z M58,34h16v16h-16z M58,58h16v16h-16z" />
</vector>
XML

cat > "$scratch/res/mipmap-anydpi-v26/ic_launcher.xml" <<'XML'
<?xml version="1.0" encoding="utf-8"?>
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
  <background android:drawable="@color/ic_launcher_background" />
  <foreground android:drawable="@drawable/ic_launcher_foreground" />
</adaptive-icon>
XML

# The legacy fallback for API < 26. Smallest valid PNG: 1x1, generated rather
# than shipped, so this probe has no binary payload of its own.
if have python3; then
    python3 - "$scratch/res/mipmap-hdpi/ic_launcher.png" <<'PY'
import struct, sys, zlib

def chunk(tag, data):
    return (struct.pack(">I", len(data)) + tag + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

w = h = 48
# Solid green RGBA, one filter byte per scanline.
row = b"\x00" + bytes([0x1B, 0x5E, 0x20, 0xFF]) * w
png = (b"\x89PNG\r\n\x1a\n"
       + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
       + chunk(b"IDAT", zlib.compress(row * h, 9))
       + chunk(b"IEND", b""))
open(sys.argv[1], "wb").write(png)
PY
    ok "legacy png" "$(wc -c < "$scratch/res/mipmap-hdpi/ic_launcher.png") bytes"
else
    no "python3" "skipping the legacy PNG"
fi

ok "resource tree" "$(find "$scratch/res" -type f | wc -l) files"

section "2. aapt2 compile"

if aapt2 compile --dir "$scratch/res" -o "$scratch/compiled.zip" 2>"$scratch/err"; then
    ok "aapt2 compile" "$(wc -c < "$scratch/compiled.zip") bytes"
    unzip -l "$scratch/compiled.zip" 2>/dev/null | sed -n '4,12p' | sed 's/^/        /'
else
    no "aapt2 compile"
    sed 's/^/        /' "$scratch/err" | head -n 10
    fatal "resource compilation failed -- the icon path in the plan does not work"
fi

section "3. link, with resource references in the manifest"

# @string, @mipmap and @color all have to resolve. If stage two cannot see
# stage one's output, this is where it shows.
cat > "$scratch/AndroidManifest.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="$PKG" android:versionCode="1" android:versionName="0.1.0">
  <application android:label="@string/app_name"
               android:icon="@mipmap/ic_launcher"
               android:hasCode="false"
               android:extractNativeLibs="true">
    <activity android:name="android.app.NativeActivity"
              android:label="@string/app_name"
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

if aapt2 link -o "$scratch/base.apk" -I "$ANDROID_JAR" \
        --manifest "$scratch/AndroidManifest.xml" \
        --min-sdk-version 24 --target-sdk-version 34 \
        "$scratch/compiled.zip" 2>"$scratch/err"; then
    ok "aapt2 link with resources" "$(wc -c < "$scratch/base.apk") bytes"
else
    no "aapt2 link with resources"
    sed 's/^/        /' "$scratch/err" | head -n 12
    fatal "linking with resources failed"
fi

section "4. build and inject the library, including libc++_shared"

cat > "$scratch/probe.cpp" <<'CPP'
// Uses the C++ library on purpose, so libc++_shared.so is a real dependency
// and the packaging step below is actually exercised.
#include <android/native_activity.h>
#include <android/log.h>
#include <string>
#include <vector>

namespace {
void on_resume(ANativeActivity*) {
    std::vector<std::string> parts{"resources", "and", "libc++"};
    std::string joined;
    for (const auto& p : parts) { joined += p; joined += ' '; }
    __android_log_print(ANDROID_LOG_INFO, "sqresprobe", "onResume: %s", joined.c_str());
}
}  // namespace

extern "C" __attribute__((visibility("default")))
void ANativeActivity_onCreate(ANativeActivity* a, void*, size_t) {
    __android_log_print(ANDROID_LOG_INFO, "sqresprobe", "onCreate");
    a->callbacks->onResume = on_resume;
}
CPP

NDK=""
for candidate in $(ls -d "${PREFIX:-/usr}/opt/android-sdk/ndk"/* 2>/dev/null | sort -V -r); do
    [ -d "$candidate/toolchains/llvm/prebuilt" ] && { NDK="$candidate"; break; }
done
NDK_LIB=""
if [ -n "$NDK" ]; then
    for host in linux-arm64 linux-x86_64; do
        d="$NDK/toolchains/llvm/prebuilt/$host/sysroot/usr/lib/aarch64-linux-android"
        [ -d "$d" ] && { NDK_LIB="$d"; break; }
    done
fi

"$CXX" -std=c++20 -O2 -fPIC -shared -fvisibility=hidden \
    "$scratch/probe.cpp" -o "$scratch/lib/$ABI/lib$LIB.so" -landroid -llog \
    2>"$scratch/err" || { sed 's/^/        /' "$scratch/err" | head -n 6; fatal "compile failed"; }
ok "lib$LIB.so" "$(wc -c < "$scratch/lib/$ABI/lib$LIB.so") bytes"

needed=$(readelf -d "$scratch/lib/$ABI/lib$LIB.so" 2>/dev/null \
    | sed -n 's/.*NEEDED.*\[\(.*\)\]/\1/p' | tr '\n' ' ')
note "NEEDED" "$needed"

if printf '%s' "$needed" | grep -q 'libc++_shared'; then
    found_cxx=""
    for candidate in "$NDK_LIB/libc++_shared.so" "$NDK_LIB/24/libc++_shared.so"; do
        [ -f "$candidate" ] && { found_cxx="$candidate"; break; }
    done
    if [ -n "$found_cxx" ]; then
        cp "$found_cxx" "$scratch/lib/$ABI/libc++_shared.so"
        ok "libc++_shared.so bundled" "$(wc -c < "$found_cxx") bytes"
    else
        no "libc++_shared.so" "needed but not found under $NDK_LIB"
        warn "the APK will install and then crash on launch"
    fi
else
    ok "no libc++_shared dependency"
fi

cp "$scratch/base.apk" "$scratch/unsigned.apk"
( cd "$scratch" && zip -q -X unsigned.apk lib/"$ABI"/*.so ) || fatal "zip failed"
ok "libraries injected"
unzip -l "$scratch/unsigned.apk" 2>/dev/null | sed -n '3,14p' | sed 's/^/        /'

section "5. sign"

KEYSTORE="$HOME/.android/debug.keystore"
[ -f "$KEYSTORE" ] || keytool -genkeypair -keystore "$KEYSTORE" -alias androiddebugkey \
    -storepass android -keypass android -keyalg RSA -keysize 2048 -validity 10000 \
    -dname 'CN=Android Debug,O=Android,C=US' 2>/dev/null

OUT="${SQ_APK_OUT:-$HOME}/squared-resprobe.apk"
if apksigner sign --ks "$KEYSTORE" --ks-pass pass:android --key-pass pass:android \
        --ks-key-alias androiddebugkey --out "$OUT" "$scratch/unsigned.apk" 2>"$scratch/err"; then
    ok "signed" "$OUT ($(wc -c < "$OUT") bytes)"
else
    no "signed"; sed 's/^/        /' "$scratch/err" | head -n 6; fatal "signing failed"
fi

apksigner verify "$OUT" >/dev/null 2>&1 && ok "verified" || no "verified"

summary "apk resources"

cat <<NOTE

  This is the *real* packaging path: compiled resources, an adaptive icon, a
  manifest with @string and @mipmap references, and libc++_shared.so bundled.

      termux-open $OUT

  Two things to check that the earlier probe could not:

    1. the launcher entry shows a green icon, not the default grey Android one
       -- that means the adaptive icon resolved
    2. it does not crash on launch -- that means libc++_shared.so was packaged
       correctly

      logcat -d -s sqresprobe

  Expect onCreate and an onResume line containing "resources and libc++".
NOTE
