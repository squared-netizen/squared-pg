---
title: Vendoring third-party libraries
tags: [developer, design, vendoring]
---

# Vendoring third-party libraries

Decision: D-068 · Open: Q-34, Q-35
Worked example: [[developer/resources/sfml-android]]

How a third-party library that a kit needs gets from upstream into a generated
workspace.

## The constraint that shapes everything

**Resources are inert data.** §2.13.2 gives `resources/` no outgoing
dependencies, and §2.13.6 lists a file-type check over resource roots as an
enforcement mechanism. Source trees and built archives are exactly what that
check exists to catch.

So a vendored library cannot live under `resources/`, however convenient the
adjacency to the kit that consumes it would be.

## The shape

```
third-party/<library>/       upstream source, trimmed. Committed.
third-party/<library>-deps/  its own dependencies. Committed.
        |
        |  tools/build-<library>.sh
        v
build/<library>-stage/       headers + archives. Gitignored.
        |
        |  --install (alpha)          |  release step (CI)
        v                             v
resources/kits/kit.X/tree/sq_kit/     the shipped .sq cartridge
   (gitignored)
```

Source at the repository root beside miniz and yyjson. Artifacts never in git —
they are platform-specific, and a repository whose CI runs on macOS and Linux
has no use for aarch64 archives.

## Why generation stays a file copy

The engine's capabilities are copy, substitute, plan, generate, index, archive
and rename. **Nothing builds.** Adding a build step would mean a new capability,
a new failure mode inside a transaction, and a toolchain dependency at
generation time.

It is not needed. The compile happens in tooling — the same place
`bootstrap.sh` lives, and for the same reason: assembling the tree is not the
engine's job.

The consequence is that a workspace receives finished artifacts and stays
self-contained, which keeps the smoke suite's "the generated project does not
depend on the generator" check honest.

## Alpha versus release

During alpha the archives are built on-device and installed into the kit
payload with `--install`, gitignored. At release CI runs the same script and the
artifacts reach users inside the cartridge.

The middle ground is deliberate: `--install` gives a fast iteration loop while
a kit is being debugged, and the default output path keeps binaries out of the
resource tree when nobody asked for them. The `.gitignore` entries are the only
thing standing between the two, so the build script prints them and tells you
to check `git status`.

## Trimming

Upstream trees carry test corpora, documentation and platform directories the
build never reads. Trim aggressively when the numbers are large and not at all
when they are small — every `rm` is a chance to remove something a build
reaches for.

For SFML's dependencies the split was 147M to 34M, almost all of it two
directories: harfbuzz's font fixtures (87M) and SheenBidi's Unicode Character
Database (19M). Verify before removing: SheenBidi's `Tools/` sits behind
`BUILD_GENERATOR`/`BUILD_TESTS`, which is checkable in its `CMakeLists.txt`.

**Record what was removed.** Someone re-fetching from the pinned tag gets the
full tree and will wonder what is missing. `third-party/README.md` is the place.

## Choosing versions

Take the upstream project's own pins where they exist. SFML's
`FetchContent_Declare` blocks name exact tags for freetype, harfbuzz,
SheenBidi, ogg, flac and vorbis; using those rather than choosing
independently means the combination is one upstream tests.

Record that this is what happened, or a later reader will assume the versions
were chosen deliberately and be reluctant to move them.

## Where this is not settled

- **The release path has never run.** CI building these artifacts is the
  intended shape, not an exercised one.
- **Multi-ABI.** One archive set per kit build, and nothing reads the ABI it
  was built for (Q-35).
- **Whether a second vendored library would follow this shape.** SFML is the
  only worked example, and some of what looks like a pattern here may be SFML's
  particulars.
