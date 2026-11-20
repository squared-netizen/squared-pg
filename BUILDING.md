# Building squared-pg

No network access is needed at any point. A clean checkout plus `third-party/`
is the entire dependency list — there is no `FetchContent`, no submodule to
initialise, and no package to install before configuring.

## Quick version

```sh
./tools/bootstrap.sh            # once: sqcart/ and resources/.squared at their pins
make -j4 && make check && make smoke
```

This produces `build/sqpg`, and sqcart's CLI as `sqcart/v3/build/sqcart3`
(installed beside sqpg as `sqcart`). The `Makefile` is the only build.

## What you need

| | Required | Notes |
|---|---|---|
| clang and libc++ 19+ | yes | C++23 with `import std;`: the engine reads cartridges through sq.cart (sqcart 3.0), whose modules sqcart/v3's Makefile builds from libc++'s own module sources. 23 is what sqterm and CI use. GCC and Apple's clang cannot build it. |
| `make` | yes | GNU make |
| `sh` | yes | for `tools/smoke.sh` |

Nothing else. Lua, yyjson, SHA-256 and sq.cart (with zstd's decoder) are all
vendored and built from source in the same pass. Pass the compiler as
`make CXX=/path/to/clang++` when `clang++` on PATH is not the one.

## Per platform

### Termux (Android)

```sh
pkg install clang make
```

This is the primary target and everything is tested here first. Termux's
clang uses libc++ already.

### Debian, Ubuntu

clang and libc++ from apt.llvm.org (Ubuntu 24.04's own clang is 18):

```sh
sudo apt install clang-23 lld-23 libc++-23-dev libc++abi-23-dev make
make CXX=/usr/lib/llvm-23/bin/clang++
```

`tools/ci-toolchain.sh` is what CI runs; it also writes `clang++` and `clang`
wrappers that link libc++, so the projects `make smoke` generates use it too.

### macOS

Homebrew's LLVM (Apple's clang has no std module):

```sh
./tools/ci-toolchain.sh ~/.local/llvm      # brew install llvm, and wrappers that link its libc++
make CXX=~/.local/llvm/clang++
```

CI builds and tests here.

### Windows

Not supported. `tools/smoke.sh` needs a POSIX shell, the durability path uses
`fsync`, and the Android template shells out to `readelf`. WSL works.

## After building

```sh
./build/sqpg list
./build/sqpg new hello --template template.terminal.cpp --kit kit.terminal
cd hello && make && ./build/hello
```

`sqpg` finds its resources and workflows relative to its own executable, so it
works from any build directory name and from anywhere on `PATH`. It never reads
the current working directory to decide what to generate.

## Generated projects need less than this

A project produced by `template.terminal.cpp` needs a C++ compiler and
`make`. It does not depend on squared-pg, does not reference the generator
tree, and does not need it installed to build, run or ship.

`template.android.cpp` is the exception, and its requirements are its own:

| For | Needs |
|---|---|
| `make` | clang targeting Android, an NDK, native-app-glue |
| `make apk` | + `aapt2`, `apksigner`, `keytool`, an `android.jar` |
| `make install` | + `termux-open` or `adb` |

```sh
cd myapp && make android-status    # what the build found, and what it did not
```

On Termux: `pkg install ndk-sysroot aapt2 apksigner openjdk-17`.

## Checking a host before you commit to it

```sh
sh tools/probe/00-run-all.sh
```

Read-only, no network, nothing written outside `$TMPDIR`. Reports the compiler,
the Android toolchain, the packaging chain and the device, and says what is
missing rather than failing at the first gap. `50-compile-checks.sh` builds
real translation units rather than checking for header presence, because a
header that exists but is unusable looks identical to a working one until you
compile against it.

## Layout of the build

```text
build/sqpg                 the CLI
build/libsquaredpg.a       engine + sqcart + vendored C  (Makefile only)
build/test_*               the engine test suite
```

`make clean` or `rm -rf build`. Nothing is installed anywhere by either build.

## Troubleshooting

**`no workflow 'workflow.generate.default' on the workflow search path`** — the
binary could not find its installation. It walks upward from its own location
looking for a directory containing both `lua/workflows` and `resources`. If you
moved the binary out of the tree, set `SQUARED_PG_WORKFLOWS` and
`SQUARED_PG_RESOURCES`, or copy the tree to `<prefix>/share/squared-pg`.

**Tests fail with permission or path errors** — they stage scratch workspaces
under `$TMPDIR`. Termux sets it; on other hosts it falls back to `.`. Set it
explicitly if `.` is not writable.

**`vendored dependency missing`** — the checkout is incomplete. `third-party/`
is part of the repository, not something fetched.
