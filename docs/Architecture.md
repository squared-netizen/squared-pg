---
title: Architecture
tags:
  - architecture
  - lua
---

# Architecture

The installed command runs on a private Lua 5.4.8 interpreter. Its Lua modules
own configuration, validation, filesystem transactions, dependency state, and
template rendering.

External processes have a narrow boundary:

- CMake extracts verified archives and configures native builds;
- Ninja compiles;
- Java launches the checked-in Gradle Wrapper;
- CTest runs the private-toolchain smoke tests.
- LDoc documents Lua APIs and Doxygen documents C++ and Java APIs.

Generated applications use five layers:

1. Android and `SDLActivity` provide the platform entry point.
2. SDL2 and its companion libraries own portable UI, input, media, and basic
   networking.
3. C++ owns stable systems, performance-sensitive code, and the Lua host API.
4. A trusted Lua bootstrap owns asset module loading and plug-in policy.
5. Capability-limited Lua plug-ins own application behavior through lifecycle
   callbacks.

The generated script runtime exposes data-oriented commands rather than SDL
pointers. Its default library profile excludes filesystem, process, package
loading, and debug libraries. The plug-in manager narrows that boundary again
with declared capabilities, local module roots, deterministic ordering, and
per-plug-in protected calls.

Local builds are the primary workflow. The generated GitHub Actions workflow
is manual and reconstructs the ignored SDL2 kit from the pinned public
release.

Documentation follows the same offline-first model. `sdl-pg docs` uses the
generator's private LDoc installation and the host Doxygen executable. A
separate manually dispatched GitHub workflow can reproduce and upload the
three generated API references.

The repository boundary and the criteria for eventually extracting the
application scripting runtime are described in
[[Repository-Model|Repository Model]].
