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

The development `.sq` boundary keeps archive mechanics native. A thin
`squared.sq` Lua binding calls the C++ validator and transactional extractor;
the Lua project builder owns registry paths, command presentation, and
collision-safe composition. Generated projects discover registered module
CMake targets in stable directory order. Squared Time is the first proof and
the Android SDL2 Lua template is the second. Squared Data is the third proof:
it owns the strict JSON API and implementation together with its private
yyjson source and license. Squared Messaging is the fourth proof and owns the
Telegram/Telegraph API and dispatcher implementation. Messaging requires exact
Data and Time versions. Squared Application is the fifth proof and exports its
lifecycle and event headers through a source-free CMake `INTERFACE` target.
Squared Math is the sixth proof and owns the platform-neutral vector and
matrix primitives used by Graphics2D. Squared Graphics is the seventh proof
and owns color values plus the SDL/OpenGL ES context boundary. Its CMake target
consumes platform targets supplied by the selected template. Squared
Graphics2D is the eighth proof and owns textures, regions, sprites, atlases,
batching, and orthographic cameras while requiring exact Graphics and Math
versions. Squared Scene2D is the ninth proof and begins Phase 6 with actor
hierarchy, ordered traversal, bounds hit testing, and a root stage while
requiring exact Graphics2D. The template directly requires Application,
Scene2D, and Messaging. None of these framework modules is physically owned by the
template. The template is read from an independently registered `.sq` package
rather than directly from the generator source tree. Executable template hooks
remain disabled.

Module manifests may declare exact module dependencies. The Lua registry
resolves the complete graph without filesystem mutation, orders dependencies
before dependents, and suppresses repeated nodes in diamond graphs. Stable
identifier ordering makes composition reproducible. Missing packages, kind
mismatches, dependency cycles, conflicting versions of one package ID, and
graphs larger than 256 nodes fail before a project tree is rendered.

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

The Phase 6 Scene2D foundation now owns `Actor`, `Group`, and `Stage` hierarchy
semantics. Rendering, transforms, input propagation, focus, actions, layouts,
skins, and widgets remain later Phase 6 layers built on the Phase 5 graphics
foundation rather than coupled directly to OpenGL ES.

The generated script runtime exposes data-oriented commands rather than SDL
pointers. Its default library profile excludes filesystem, process, package
loading, and debug libraries. The plug-in manager narrows that boundary again
with declared capabilities, local module roots, deterministic ordering, and
per-plug-in protected calls.

Local builds are the primary workflow. The generated GitHub Actions workflow
is manual and reconstructs the ignored SDL2 kit from the pinned public
release.

Documentation follows the same offline-first model. `squared-pg docs` uses the
generator's private LDoc installation and the host Doxygen executable. A
separate manually dispatched GitHub workflow can reproduce and upload the
three generated API references.

`squared-pg` is frontend-neutral at its public boundary. The current
`sdl_pg` Lua namespace is a private implementation detail retained during the
0.6 naming migration; it does not define the supported frontend set. The
generator-only provider dispatcher resolves a template package and its full
dependency graph, matches the declared template profile to a built-in adapter,
and only then permits project staging. Android SDL2 Lua is the first adapter.
That adapter preflights its external resources through the generator-only
dependency dispatcher. The first stable dependency ID is `android-sdl2`; the
older `kit` command is retained as a compatibility alias.

Provider selection is not copied into generated projects. The selected
adapter renders a statically specialized source tree containing only its
frontend, libraries, and platform glue. Generated application startup and
frame execution therefore pay no provider-registry lookup, retained selection
state, or unused-frontend RAM cost.

The repository boundary and the criteria for eventually extracting the
application scripting runtime are described in
[[Repository-Model|Repository Model]].
