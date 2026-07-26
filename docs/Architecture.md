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

Generated applications use four layers:

1. Android and `SDLActivity` provide the platform entry point.
2. SDL2 and its companion libraries own portable UI, input, media, and basic
   networking.
3. C++ owns stable systems, performance-sensitive code, and the Lua host API.
4. Lua owns project scripts and plug-in logic through lifecycle callbacks.

The generated script runtime exposes data-oriented commands rather than SDL
pointers. Its default library profile excludes filesystem, process, package
loading, and debug libraries. Callback errors remain inside protected Lua
calls and are reported through SDL logging.

Local builds are the primary workflow. The generated GitHub Actions workflow
is manual and reconstructs the ignored SDL2 kit from the pinned public
release.

The repository boundary and the criteria for eventually extracting the
application scripting runtime are described in
[[Repository-Model|Repository Model]].
