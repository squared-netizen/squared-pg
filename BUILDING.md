# Building squared-pg

No network access is needed at any point. A clean checkout plus `third-party/`
is the entire dependency list — there is no `FetchContent`, no submodule to
initialise, and no package to install before configuring.

## Quick version

```sh
cmake -S . -B build && cmake --build build -j4 && ctest --test-dir build
```

Or, on a host without CMake:

```sh
make -j4 && make check && make smoke
```

Both produce `build/sqpg`. The CMake build is the supported one; the
hand-written `Makefile` is a behavioural equivalent for Termux and for quick
iteration.

## What you need

| | Required | Notes |
|---|---|---|
| C++20 compiler | yes | clang 15+ or GCC 12+. `std::expected` is not required — there is a tested fallback. |
| C compiler | yes | for the vendored C dependencies |
| `make` | yes | GNU make |
| CMake 3.21+ | no | for the CMake build path |
| `sh` | yes | for `tools/smoke.sh` |

Nothing else. Lua, miniz, yyjson, SHA-256 and sqcart are all vendored and built
from source in the same pass.

## Per platform

### Termux (Android)

```sh
pkg install clang make cmake
```

This is the primary target and everything is tested here first.

### Debian, Ubuntu

```sh
sudo apt install build-essential cmake
```

### Fedora, Arch

```sh
sudo dnf install gcc-c++ cmake make      # Fedora
sudo pacman -S base-devel cmake          # Arch
```

### macOS

```sh
xcode-select --install
brew install cmake
```

Untested. The engine has no macOS-specific code and the executable-path
lookup handles Darwin, but nobody has run it. If it does not work, that is a
bug and worth reporting rather than working around.

### Windows

Not supported. `tools/smoke.sh` needs a POSIX shell, the durability path uses
`fsync`, and the Android template shells out to `readelf`. The engine itself is
portable C++20 and would probably compile; nothing else would. WSL works.

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

A project produced by `template.terminal.cpp` needs a C++20 compiler and
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
