---
title: Commands
tags:
  - cli
  - reference
---

# Commands

## Environment

```sh
squared-pg version
squared-pg doctor
squared-pg verbose
squared-pg agent-feedback
squared-pg self-test
```

`verbose` prints the full local generator, tool, dependency, wrapper, package,
template, and current-project status. `agent-feedback` emits the same decision
signals in the stable `SQUARED_PG_AGENT_FEEDBACK_V1` line-oriented format. It
is capped at 32 lines and 4 KiB, includes package counts rather than a full
inventory, never dumps arbitrary environment variables, and always ends with
`truncated=true` or `truncated=false`. `self-test` performs non-mutating checks
of the installed generator and its registered inputs.

## Uninstall

```sh
squared-pg uninstall
squared-pg uninstall --confirm
```

The command without `--confirm` is a dry run: it prints every generator-owned
path that would be removed and the end-user roots that will be preserved.
Confirmed uninstall removes the canonical `~/.squared/squared-pg` tool tree,
installed launchers, configuration, package registry, cached registered
inputs, and corresponding legacy `sdl-pg` state.

Run the confirmed command from outside the generator tree:

```sh
cd "$HOME"
squared-pg uninstall --confirm
```

The command never traverses or removes `~/sandbox` or `~/projects`.

## Documentation

```sh
squared-pg docs
```

Run this command from a generated Android project or any directory beneath
it. It recursively generates separate Lua, authored C++, and vendored SDL2
Java-wrapper API references beneath `build/docs/`.

The generated project keeps an Obsidian-compatible entry point at
`docs/API.md`. Doxygen emits HTML and XML for C++ and Java; LDoc emits HTML
for Lua.

## Dependency registry

```sh
squared-pg dependency add android-sdl2 ARCHIVE
squared-pg dependency status
squared-pg dependency status android-sdl2
squared-pg kit add ARCHIVE
squared-pg kit status
squared-pg wrapper add EXISTING_PROJECT
squared-pg wrapper status
squared-pg package add BACKUP.sq
squared-pg package build SOURCE_DIRECTORY OUTPUT.sq
squared-pg package verify ARCHIVE.sq
squared-pg package status
squared-pg package resolve ID@VERSION
squared-pg template status
squared-pg template use ID@VERSION
```

`dependency add android-sdl2` requires the exact v1.0.0 Android ARM64 archive
and its adjacent `.sha256` file. Dependency status lists built-in provider IDs
in stable order. `kit add` and `kit status` remain compatibility aliases for
`android-sdl2`. Existing cached kit state is reused without migration.
`wrapper add` imports only the four Gradle Wrapper files.
`package add` performs native structural, path, resource-limit, and SHA-256
validation before transactionally registering a local `.sq` backup. It does
not contact a network. Identical repeated imports are idempotent; an existing
ID/version with different content is rejected. The status command lists exact
installed versions. `package resolve` performs a read-only recursive preflight
and prints the deterministic dependency-first composition order followed by
the selected root package. Missing exact versions, kind mismatches, cycles,
conflicting versions, and graphs exceeding 256 packages are rejected.

Package source directories contain `manifest.json` and `content/`. `package
build` refuses to overwrite an output archive; `package verify` validates an
archive without registering it. A framework repository can therefore build,
verify, register, and select a new template independently:

```sh
squared-pg package build packages/my-module dist/my-module-1.0.0.sq
squared-pg package verify dist/my-module-1.0.0.sq
squared-pg package add dist/my-module-1.0.0.sq
squared-pg template use dev.example.template@1.0.0
```

Package registration remains immutable: changed contents require a new
version. `template use` performs the complete recursive preflight before it
changes the active selection.

## Projects

```sh
squared-pg new NAME --package JAVA.PACKAGE
squared-pg new NAME --package JAVA.PACKAGE \
  --template dev.squarednetizen.template.android-sdl2-lua@0.6.0-dev.15 \
  --app-name "Application Name" \
  --base-version 0.1.0
squared-pg new NAME --package JAVA.PACKAGE --project
squared-pg new NAME --package JAVA.PACKAGE --manage-all-files
squared-pg new NAME --foundation
squared-pg project verify [DIRECTORY]
squared-pg promote NAME
squared-pg demote NAME
```

The registered Android SDL2 Lua template is currently the default when
`--template` is omitted. The generator resolves the selected template and its
dependencies, then selects a generation adapter from the template's declared
profile before staging begins. Unsupported profiles fail without creating a
destination. Repeated transitive dependencies are composed once in
deterministic dependency-first order. Provider selection exists only in the
generator; the selected adapter also preflights its external dependency IDs
before staging. The generated project contains neither dispatcher.
`--manage-all-files` is an Android-only opt-in for applications that genuinely
require unrestricted shared-storage access. It adds the special permission,
opens the app-specific settings once on first startup when access is missing,
and adds a prominent warning to the generated README. Projects generated
without the switch do none of these things.
`--foundation` selects the small host-only Phase 1 scaffold.
`project verify` locates the nearest generated-project marker, safely reads
its metadata, checks `project.lua` agreement, and rejects leaked
generator-only dispatch modules.

## Naming compatibility

`sdl-pg` remains an installed compatibility alias during the 0.6 development
line. Existing `SDL_PG_*` environment variables, `.sdl-pg.lua` project
markers, configuration, and package caches remain readable. New scripts should
use `squared-pg`, `SQUARED_PG_*`, and `.squared-pg.lua`.
