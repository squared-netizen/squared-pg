---
title: Commands
tags:
  - cli
  - reference
---

# Commands

## Environment

```sh
sdl-pg version
sdl-pg doctor
```

## Dependency registry

```sh
sdl-pg kit add ARCHIVE
sdl-pg kit status
sdl-pg wrapper add EXISTING_PROJECT
sdl-pg wrapper status
```

`kit add` requires the exact v1.0.0 Android ARM64 archive and its adjacent
`.sha256` file. `wrapper add` imports only the four Gradle Wrapper files.

## Projects

```sh
sdl-pg new NAME --package JAVA.PACKAGE
sdl-pg new NAME --package JAVA.PACKAGE --project
sdl-pg new NAME --foundation
sdl-pg promote NAME
sdl-pg demote NAME
```

Android is the default profile. `--foundation` selects the small host-only
Phase 1 scaffold.
