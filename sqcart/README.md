# sqcart

Reference implementation of the [Squared Cartridge Specification
2.0](docs/developer/specs/sq-cartridge-2.0.md) — the `.sq` container format
used for Squared templates, kits, packages, asset bundles, plugins and
distributable applications.

Standalone C++20. No dependency on `squared-pg` or on the Squared framework.

## Build

```sh
cmake -S . -B build -DSQCART_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Or, without CMake (handy on Termux):

```sh
make check
```

Embedded, with the host supplying the backends:

```cmake
set(SQCART_USE_EXTERNAL_MINIZ  ON)
set(SQCART_USE_EXTERNAL_YYJSON ON)
add_subdirectory(sqcart)
target_link_libraries(my_target PRIVATE sqcart::read)
```

## Targets

| Target | Option | Default | For |
|---|---|---|---|
| `sqcart::read` | — | always | every consumer |
| `sqcart::write` | `SQCART_ENABLE_WRITER` | `ON` | packaging tools |
| `sqcart` (CLI) | `SQCART_ENABLE_CLI` | top-level only | humans and CI |
| `sqcart::lua` | `SQCART_ENABLE_LUA` | `OFF` | deferred; no consumer yet |

A game runtime or Android launcher links `sqcart::read` alone and carries no
compressor, no canonical-ordering logic and no manifest serialiser.

## Library

```cpp
#include <sqcart/sqcart.hpp>

auto cart = sqcart::Cartridge::open("kit.sdl3-1.2.0.sq");
if (!cart) {
    std::cerr << sqcart::to_string(cart.error().code) << ": "
              << cart.error().message << '\n';
    return 1;
}

// The envelope is typed. Everything a *consumer* needs is carried as text
// under its own namespace and parsed by that consumer -- sqcart does not
// know what a kit is, and deliberately cannot be taught here.
std::cout << cart->manifest().kind() << '\n';        // "kit", an opaque token

if (auto section = cart->manifest().consumer("squared_pg")) {
    // JSON text. Parse it with your own reader.
}
```

sqcart implements the container and the manifest envelope. It carries
consumer sections without interpreting them, which is what lets the same
library serve `squared-pg` and the Squared framework runtime without either
acquiring the other's vocabulary (spec §0.1, §5.6).

`.sq` archives and exploded directories containing `SQ-INF/manifest.json` are
both accepted; `open()` detects which, and the two are semantically equivalent
(spec §3.5) down to producing the same content digest.

## CLI

```
reading:
  list <cartridge>            entries: size, compressed, method, path (tab-separated)
  show <cartridge>            manifest JSON, for piping into jq
  info <cartridge>            human summary
  cat <cartridge> <entry>     one entry's bytes to stdout
  verify <cartridge>          full validation pipeline
  digest <cartridge>          content digest, sha256sum-compatible output

writing:
  pack <directory> -o <out>   build a cartridge from an exploded tree
  extract <cartridge> [-d D]  materialise the payload
```

Exit codes: `0` success, `1` operation failed, `2` usage error, `3` cartridge
is non-conforming. `verify` is usable as a CI gate unchanged.

stdout carries machine-readable results only; diagnostics go to stderr.

```sh
$ sqcart digest resources/kits/sdl3          # exploded tree
84bcf810ec4007d9fa4fa4910797195d4d8f9ec07833f51e5dcddb6a39e0745b  resources/kits/sdl3
$ sqcart pack resources/kits/sdl3 -o kit.sdl3.sq
84bcf810ec4007d9fa4fa4910797195d4d8f9ec07833f51e5dcddb6a39e0745b  kit.sdl3.sq
```

The digest is computed over content and paths, never over archive bytes, so
it is stable across compressors and across container forms.

## C++20 and `std::expected`

`<expected>` is C++23. `include/sqcart/expected.hpp` aliases `std::expected`
where available and supplies a minimal substitute otherwise. Code using
`sqcart::Result<T>` compiles either way; `tests/test_expected.cpp` asserts the
two paths behave identically.

## Constraints

See `AGENTS.md` for the scope rule, the CLI's IO-isolation rule, and the
single-digest-implementation rule. Run `../tools/sqcart-isolation.fish` before
committing.
