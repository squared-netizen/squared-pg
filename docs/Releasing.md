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
- `lua/sdl_pg/version.lua`
- `CHANGELOG.md`
- the bundled template manifest and generated project metadata

## Suggested Git sequence

Review the complete diff and explicitly stage the intended source paths. Do
not use a blind `git add .` in an incubation worktree that contains historical
archives or compiler scratch files:

```bash
git diff --check
git status --short
# git add only the reviewed source paths
git diff --cached --check
git diff --cached --stat
git status --short
```

After the local and Android tests pass:

```bash
git commit -m "Release Squared Project Generator VERSION"
git tag -a vVERSION -m "Squared Project Generator vVERSION"
git push
git push origin vVERSION
```

Creating the public release and attaching its checksummed archive remains an
explicit manual operation.

The first public `squared-pg` publication should preserve the incubation
history. Confirm the final GitHub owner/repository and whether the existing
remote will be renamed or a new remote added before changing `origin`,
committing, tagging, or pushing.
