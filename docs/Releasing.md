---
title: Releasing
tags:
  - release
  - testing
---

# Releasing

Releases are prepared locally and published deliberately. The generator never
creates a Git tag or GitHub release on behalf of the user.

## Validation

From the repository root:

```bash
lua5.4 toolchain.lua
sha256sum -c SHA256SUMS.txt
git diff --check
git status --short
```

Generate and build at least one Android project with the registered offline
SDL2 kit and Gradle Wrapper. Install the resulting APK on an ARM64 Android
device before publishing a stable release.

## Version locations

Keep these values synchronized:

- `CMakeLists.txt`
- `lua/sdl_pg/main.lua`
- `CHANGELOG.md`
- generated project metadata in `lua/sdl_pg/project.lua`

## Suggested Git sequence

Review the complete diff before committing:

```bash
git add .
git diff --cached --check
git diff --cached --stat
git status --short
```

After the local and Android tests pass:

```bash
git commit -m "Release SDL Project Generator VERSION"
git tag -a vVERSION -m "SDL Project Generator vVERSION"
git push
git push origin vVERSION
```

Creating the public release and attaching its checksummed archive remains an
explicit manual operation.
