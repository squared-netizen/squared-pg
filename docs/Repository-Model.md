---
title: Repository Model
tags:
  - architecture
  - release
  - workflow
---

# Repository Model

The toolchain begins with two reusable repositories:

| Repository | Responsibility |
| --- | --- |
| `sdl2-android-arm64` | Versioned SDL2 headers, Android ARM64 libraries, Java glue, licenses, checksums, and capability tests |
| `sdl-project-generator` | The `sdl-pg` command, private Lua toolchain, project templates, tests, and documentation |

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
application runtime may move to a third repository such as
`sdl-lua-runtime`.

That runtime should be distributed as a versioned archive with a separate
SHA-256 file. `sdl-pg` would register and cache it just like the SDL2 kit.
Generated projects would still contain all required sources, so builds would
remain independent and offline.

Git submodules are intentionally avoided. Release archives provide clearer
version boundaries and are easier to cache, verify, back up, and restore on
Termux.
