# Building squared-pg

No network access is needed at any point after the tree is assembled. A
clean checkout plus `third-party/` is the entire dependency list: there is
no `FetchContent`, no submodule to initialise, and no package to install
before building.

## Quick version

```sh
./tools/bootstrap.sh            # once: sqcart/ and resources/.squared at their pins
sqmake && sqmake test && sqmake smoke
```

This produces `build/sqpg`, and sqcart's CLI as `build/sqcart3` (installed
beside sqpg as `sqcart`). `build.lua` is the only build, and sqmake builds it.

## What you need

| | Required | Notes |
|---|---|---|
| clang and libc++ 19+ | yes | C++23 with `import std;`: the engine reads cartridges through sq.cart (sqcart 3.0), whose modules sqcart/v3/build.lua builds; the std module comes from libc++'s own module sources. 23 is what sqterm and CI use. GCC cannot build it. |
| sqmake | yes | the SDK's build (`pkg install sqmake` in sqterm; elsewhere its `tools/bootstrap.sh` makes one from source with a compiler and a shell) |
| `sh` | yes | for `tools/smoke.sh` |

Nothing else. Lua, yyjson, SHA-256 and sq.cart (with zstd) are all vendored
and built from source in the same pass. Name the compiler with
`CXX=/path/to/clang++` (and `CC=`) when `clang++` on PATH is not the one;
`OPT=-O2` for a release's optimisation (`-O1` otherwise).

## Per platform

### sqterm (Android)

```sh
pkg install llvm sqmake
```

The primary target: everything is tested here first. sqterm's clang uses
libc++ already.

### Debian, Ubuntu

clang and libc++ from apt.llvm.org (Ubuntu 24.04's own clang is 18):

```sh
sudo apt install clang-23 lld-23 libc++-23-dev libc++abi-23-dev
CXX=/usr/lib/llvm-23/bin/clang++ CC=/usr/lib/llvm-23/bin/clang sqmake
```

`tools/ci-toolchain.sh` is what CI runs: it installs them, writes `clang++`
and `clang` wrappers that link libc++ (so the projects `sqmake smoke`
generates use it too), and bootstraps sqmake from its source beside them.

Other systems are not supported: squared is for Linux and Android.

## After building

```sh
./build/sqpg list
./build/sqpg new hello --template template.terminal.cpp --kit kit.terminal
cd hello && sqmake run
```

`sqpg` finds its resources and workflows relative to its own executable, so it
works from any build directory name and from anywhere on `PATH`. It never reads
the current working directory to decide what to generate.

## Generated projects need less than this

A workspace made from any template builds with sqmake and a compiler, from its
own `build.lua` (and the template's `sq/build/template.lua`). It does not
depend on squared-pg, does not reference the generator tree, and does not need
it installed to build, run or ship. `template.android.cpp`'s needs are its
template's: its build says what it found (`sqmake toolchain`).

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
build/sqcart3              sqcart's CLI, installed beside sqpg as sqcart
build/libsquaredpg.a       the engine
build/test_*               the engine test suite
```

`sqmake clean` or `rm -rf build`. Nothing is installed anywhere by the build.

## Troubleshooting

**`sqcart/ is not present, or is older than its build.lua`** — the tree is not
assembled, or was assembled before sqcart had a `build.lua`: run
`./tools/bootstrap.sh`.

**`no workflow 'workflow.generate.default' on the workflow search path`** — the
binary could not find its installation. It walks upward from its own location
looking for a directory containing both `lua/workflows` and `resources`. If you
moved the binary out of the tree, set `SQUARED_PG_WORKFLOWS` and
`SQUARED_PG_RESOURCES`, or copy the tree to `<prefix>/share/squared-pg`.

**Tests fail with permission or path errors** — they stage scratch workspaces
under `$TMPDIR`. sqterm sets it; on other hosts it falls back to `.`. Set it
explicitly if `.` is not writable.

**`vendored dependency missing`** — the checkout is incomplete. `third-party/`
is part of the repository, not something fetched.
