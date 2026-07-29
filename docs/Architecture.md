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

Generated applications use six layers:

1. Android and `SDLActivity` provide the platform entry point.
2. SDL2 and its companion libraries own portable windowing, input, media, and
   basic networking.
3. `squared::graphics` owns the OpenGL ES context, while
   `squared::graphics2d` provides textures, regions, sprites, batching, and an
   orthographic camera.
4. A generated SDL adapter owns platform initialization, event translation,
   and the frame loop. It imports only the developer application factory.
5. Developer-owned C++ application code owns stable systems,
   performance-sensitive code, and the Lua host API.
6. A trusted Lua bootstrap and capability-limited plug-ins own application
   behavior through lifecycle
   callbacks.

Developer sources live beneath `application/src/` and are discovered by a
CMake `CONFIGURE_DEPENDS` scan limited to that directory. Framework and
platform sources remain explicit. This removes routine CMake edits without
allowing stale generated implementations to compete with developer code.

Scene graph and UI concepts including `Actor`, `Group`, `Stage`, actions, and
widgets are deliberately reserved for Phase 6. They will depend on the Phase 5
graphics foundation rather than being coupled directly to OpenGL ES.

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
