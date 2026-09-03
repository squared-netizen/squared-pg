# squared-pg

An offline-first project generator for applications built on the Squared
framework. A native C++20 engine provides generation capability; a Lua 5.4
control layer sequences it into workflows. The output is a self-contained
developer workspace you own outright.

Termux on Android is a first-class target. A phone with a compiler and `make` is
enough — to run the generator, and to build what it produces.

## Build

```sh
cmake -S . -B build && cmake --build build && ctest --test-dir build
```

Without CMake — Termux, chiefly:

```sh
make          # engine, CLI and tests
make check    # run the tests
make smoke    # generate a project, build it, run it
```

No network, at any point. A clean checkout plus `third-party/` is the whole
dependency list.

## Use

```sh
./build/sqpg list
./build/sqpg new hello --template template.termux.cpp --kit kit.terminal
cd hello && make && ./build/hello
```

Add a Lua script workspace alongside the C++ one:

```sh
./build/sqpg new hello --template template.termux.cpp --kit kit.terminal --kit kit.lua
```

See what would happen without doing it:

```sh
./build/sqpg plan hello --template template.termux.cpp --kit kit.terminal
```

## The idea

The engine is a programmable pipeline: fixed, well-tested capability with
programmable sequencing on top. It knows what a template, kit, package and asset
*are*, and how to resolve, validate and materialize them. It does not decide
which one you want.

Lua decides. It parses arguments, loads configuration, chooses the template and
kits, calls engine operations in whatever order it likes, and interprets the
results. The CLI is one consumer of that stack, not the defining interface — and
the workflows stay editable after release, without a rebuild.

```text
User / tool input  →  Lua workflow  →  engine control surface  →  workspace
```

## Layout

```text
engine/        the C++20 engine — include/ is public, src/ is not
engine/lua/    the Lua binding layer
lua/           workflows and shared Lua modules
app/           sqpg, the reference CLI host
resources/     templates, kits, packages, assets — inert data
sqcart/        the Squared Cartridge reference implementation (nested project)
third-party/   vendored dependencies
tools/         maintenance and smoke tests
docs/          specification and the two documentation trees
```

## Documentation

- [`docs/programmer/`](docs/programmer/README.md) — using the engine, writing
  workflows, authoring templates and kits
- [`docs/developer/`](docs/developer/README.md) — how it works inside, the
  patterns used, and what it deliberately does not do
- [`docs/spec/`](docs/spec/Specification%20Index.md) — the functional
  specification (an Obsidian vault)

## What ships today

**Engine.** Lifecycle state machine, ten core services, an enumerable operation
registry, a manifest-identity resource index built on `sqcart`, plan production,
and transactional new-workspace generation with provenance recording.

**Resources.** `template.termux.cpp` (a C++20 terminal application that builds
with make and a compiler), `kit.terminal` (header-only console I/O, regex
helpers and a libGDX-style `FileHandle`), `kit.lua` (an embedded Lua 5.4
interpreter and an `sq_lua/` script workspace).

**Not yet.** Regeneration into an existing workspace, package and asset
materialization, template composition, and enforcement of the sandboxed workflow
trust tier. Each is refused with a structured error rather than approximated;
see [limitations](docs/developer/engine/limitations.md).

## Guarantees the generated project gets

- It does not depend on squared-pg. The generator is not on its build path.
- You never edit outside the working directory (`sq_app/`) to develop it.
- Nothing generator-owned is written inside that directory.
- Every generated path is recorded with the hash it had when written, so a
  future update can tell your edits from its own output without guessing.
- Delete `.squared/` and you still have a working project. You lose only the
  ability to update it in place.
