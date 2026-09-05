# tools/probe

Read-only host capability probes. They answer "will squared-pg build here, and
what can it target", without installing anything or writing outside `$TMPDIR`.

```sh
sh tools/probe/00-run-all.sh > probe-report.txt 2>&1
```

| Script | Answers |
|---|---|
| `10-toolchain.sh` | is there a compiler, does it target Android natively, at what API level |
| `20-ndk-headers.sh` | are the NativeActivity, EGL and GLES 3.0 headers and stub libraries reachable |
| `30-apk-tools.sh` | is the APK packaging chain present — and crucially, is there an `android.jar` |
| `40-device.sh` | ABI, API level, and how an APK would get installed |
| `50-compile-checks.sh` | **does it actually compile and link**, rather than merely being present |

`50-compile-checks.sh` is the one that matters. A header that exists but is
unusable — wrong API level, missing transitive include, a stub library without
the symbol — looks identical to a working one until you build against it, so
every check there builds a real translation unit and throws it away.

It also answers one question the others only raise: whether `aapt2 link` can
produce a base APK with **no `android.jar`**. If it can, on-device packaging is
dependency-free and the Android SDK never has to be installed.

## Safety

POSIX `sh`. No writes outside `$TMPDIR` (never `/tmp` — Termux has none). No
network. A missing tool is reported, never fatal: the point is a complete
report, not stopping at the first gap.

Override the compiler if the default is wrong:

```sh
CXX=clang++ CC=clang sh tools/probe/50-compile-checks.sh
```
