---
title: Repository Model
tags:
  - architecture
  - release
  - workflow
---

# Repository Model

The intended toolchain uses three reusable repository boundaries:

| Repository | Responsibility |
| --- | --- |
| `sdl2-android-arm64` | Versioned SDL2 headers, Android ARM64 libraries, Java glue, licenses, checksums, and capability tests |
| `squared-pg` | Frontend-neutral generator CLI, private Lua toolchain, `.sq` package infrastructure, composition tests, and documentation |
| `squared` | Reusable Application, Data, Time, Messaging, Math, Graphics, Graphics2D, and Scene2D package sources |

The existing `sdl-project-generator` repository is the incubation history for
`squared-pg`. The 0.6 development line establishes the new identity before
the Squared package sources are extracted. Preserving history is preferred to
starting the generator repository over from an empty initial commit.

During that extraction, the repository-owned `packages/` tree is a frozen
bootstrap catalog and integration fixture. New Squared package versions are
built in the `squared` repository with the installed `squared-pg package
build`, registered with `package add`, and activated with `template use`.
Changing framework code therefore does not require rebuilding or republishing
the generator.

SDL2 is the first frontend provider. Future SFML and PDCurses templates should
select only their required source, libraries, and adapters during generation.
Generated applications must not carry unused frontend implementations or
runtime selection machinery. This keeps RAM use primary and active execution
paths direct.

The current provider table belongs exclusively to the generator process. It
maps a validated template profile to a built-in generation adapter after
recursive package resolution. This is a bounded migration boundary, not a
runtime plug-in system; unsupported profiles fail before project staging.
Each selected adapter then preflights its external build inputs through stable
generator dependency IDs. `android-sdl2` is the first implementation; future
SFML and PDCurses adapters can declare different dependencies without changing
generated runtime architecture.

Every serious generated application becomes its own repository beneath
`~/projects`. Experimental applications remain beneath `~/sandbox` unless
explicitly promoted.

## Current Lua ownership

The generator currently owns:

- the pinned Lua 5.4.8 source archive used by its private tools;
- the Lua source staged into generated applications;
- the generated C++↔Lua host bridge;
- the scripting contract and its tests.

Keeping these pieces together allows the scripting API to evolve without
coordinating releases across another repository.

## Future runtime extraction

After the plug-in API has stabilized through real applications, the reusable
application runtime may move to another repository such as
`squared-lua-runtime`.

That runtime should be distributed as a versioned archive with a separate
SHA-256 file. `squared-pg` would register and cache it as a versioned
dependency.
Generated projects would still contain all required sources, so builds would
remain independent and offline.

Git submodules are intentionally avoided. Release archives provide clearer
version boundaries and are easier to cache, verify, back up, and restore on
Termux.
