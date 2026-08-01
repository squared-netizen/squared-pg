---
title: Phase 5 Graphics2D Design
tags:
  - architecture
  - graphics
  - phase-5
---

# Phase 5 Graphics2D Design

Phase 5 begins the minimum viable `squared::` application framework. It is a
general two-dimensional foundation for games and tools such as text and
Markdown editors, graphics editors, dialogue editors, and tiled-map editors.
Application code uses the framework instead of Android APIs or raw OpenGL ES
for ordinary rendering.

## First vertical slice

The initial code path deliberately stays small:

- `squared::graphics::Context` owns the SDL window and OpenGL ES 2 context;
- `Texture` owns one GPU texture and can load through SDL_image;
- `TextureRegion` is a non-owning rectangular texture view;
- `TextureAtlas` transactionally loads libGDX text atlases and owns their page
  textures;
- `Sprite` stores one region's transform and tint;
- `SpriteBatch` preserves draw order while batching textured quads;
- `OrthographicCamera` supplies top-left or bottom-left logical coordinates;
- `Matrix4`, `Vector2`, and `Color` support those public APIs.

The generated example still gets its visual state from Lua, but its C++ host
now draws that state through `SpriteBatch`. This proves the framework boundary
without changing the scripting lifecycle at the same time.

The device-native Termux path consumes the Khronos GLES2 declarations already
vendored under SDL-specific header names in the registered kit. CMake uses an
NDK `GLESv2` linker stub when building with an Android toolchain and otherwise
links the phone's `/system/lib64/libGLESv2.so`. The system library is never
copied into the APK.

## Phase 5 remainder

The graphics foundation can grow after this vertical slice with bitmap fonts,
shape rendering, framebuffers, shader ownership, skin
loading, asset lifetime management, and context-loss recovery. A default skin
and prepacked atlas may be shipped by the generator. The generator will not
pack application textures.

Texture packing belongs to a future symmetric framework-facing asset-tools API,
not to project generation. The likely public home is
`squared::asset_tools::TexturePacker`, which may live in a separately released
library while remaining compatible with framework atlas data.

## Application boundary

The generated SDL entry point lives beneath `platform/` and imports only the
project's `create_application()` factory. It owns SDL subsystem setup, event
translation, the frame clock, pause/resume handling, and presentation.

Developer code lives beneath `application/`. A recursive
`CONFIGURE_DEPENDS` source scan is deliberately restricted to
`application/src/`, while framework and platform source lists remain
explicit. Headers require no CMake source entry. Generator updates must not
place replacement implementations in the developer-owned tree.

The first atlas slice accepts multiple pages, duplicate names distinguished by
index, rotation, trimming metadata, nine-patch split/pad metadata, filtering,
and repeat modes. Page paths are relative and traversal is rejected. Atlas
ownership keeps page textures alive for all returned non-owning regions, and a
failed reload preserves the prior atlas.

Bundled and default atlas pages use PNG, matching libGDX conventions and the
SDL_image decoder explicitly initialized by the generated host.

The default project uses two indexed atlas regions as an on-device lifecycle
sentinel. The status square begins blue and toggles green when touched. Green
surviving a background/foreground trip demonstrates that in-memory process
state survived; returning to blue identifies recreation. Durable persistence
is a later, separate concern.

## Strict JSON foundation

Phase 5 pins yyjson 0.12.0 behind `squared::data`; application headers do not
expose yyjson types. `JsonValue` owns its data and distinguishes signed
integers, unsigned integers, real numbers, strings, arrays, and objects.

Parsing is strict RFC 8259 by default. Comments, trailing commas, single
quotes, byte-order marks, invalid UTF-8, non-finite numbers, and duplicate
object keys are rejected. Callers may explicitly permit duplicates for
compatibility, in which case the last value wins. Input size and nesting depth
have configurable limits.

Serialization orders object keys bytewise, preserves numeric categories, and
supports compact or two-space pretty output. This deterministic behavior is
the persistence base for future project manifests, skins, editor documents,
and other tool data. A Lua facade and domain-specific schemas remain later
work; the generator itself continues to use Lua configuration where JSON adds
no benefit.

The generated Android sample proves this path at runtime. It loads text, a
font path, a point size, and optional colors from strict JSON; serializes and
parses the value again; then renders the JSON-provided text through SDL_ttf.
A separate stage-colored panel and structured log remain observable when text
cannot be rendered.

## Time domains

A `Timepiece` belongs to an application, simulation, UI, or editor-preview
time domain. It does not belong to each object, agent, or Telegram. The owner
advances it once per frame or fixed step. Dispatchers borrow a read-only
`Clock` view and capture the current time once per dispatcher update.

Agents share their domain's dispatcher. Immediate Telegrams use bounded
queued delivery, while delayed Telegrams store a due time. Duration precision
does not create a background microsecond or nanosecond update loop.

The implemented foundation exposes a read-only `Clock`, mutable `Timepiece`,
and resettable `ManualTimepiece`. A bounded `DeadlineQueue<T>` orders equal
deadlines by insertion sequence, supports cancellation, reports capacity and
time errors, and accepts an explicit per-update delivery limit. It deliberately
stores values rather than callbacks. Phase 5 messaging will wrap this
mechanism instead of creating a second timer system.

The messaging layer uses independently implemented `Telegram`, `Telegraph`,
`MessageDispatcher`, stable namespaced `MessageId` and `EndpointId` values,
and move-only scoped `Subscription` handles. `send()` is queued by default;
`send_now()` is explicit; `schedule()` and `cancel()` use stable
`DispatchHandle` values. Directed and broadcast delivery are deterministic,
bounded, and performed on the dispatcher's calling thread.

Payloads are owned JSON values. Optional return receipts are separate queued
Telegrams carrying the original correlation ID and `handled`, `unhandled`,
`receiver_unavailable`, or `cancelled` status. There is no global dispatcher,
and each dispatcher borrows exactly one time domain.

`TelegramProvider` state-on-subscription delivery is implemented as an
authoritative alternative to retained-message replay. One scoped provider may
serve each message ID. It is called once for a new subscriber and may provide
fresh owned JSON state, report that no current state exists, or reject the
subscription with a diagnostic. Provided state enters the ordinary bounded
queue but targets only that new subscription; failure or queue exhaustion
rolls the subscription back atomically.

Pending-message inspection and versioned JSON snapshot/restoration are
implemented. Inspection visits immutable queue entries without copying their
payloads. Snapshots store remaining delays, stable IDs, owned payloads,
correlations, and receipt requests rather than raw time points, pointers,
handles, or subscription tokens. Restoration validates the complete document
and capacity before atomically committing into an empty queue.

Subscription-targeted provider startup state is explicitly nonpersistent
because its target token is process-local. It must be delivered before a
checkpoint; the snapshot operation reports the condition instead of silently
turning it into a broadcast. Richer cancellation metadata remains follow-on
Phase 5 work.

## Cartridge persistence direction

The future optional cartridge package models an inseparable physical
`Drive`/`Cartridge` pair. It is not a replacement for ordinary C++ or Lua
filesystem APIs.

Cartridge 1.0 will use a transparent ZIP-compatible tree with framework-owned
manifest, metadata, thumbnail, integrity, and recovery information beneath
`/.squared/`. Insertion validates and stream-extracts the cartridge into a
persistent working area. Autosave flushes that working state; checkpoint and
eject stream a validated replacement cartridge. The working state survives
Android activity and process destruction.

SQLite is reserved for a specialized future cartridge storage type only when
target-device benchmarks demonstrate a substantial benefit for many small or
queryable records. It is not the general 1.0 cartridge format.

## Native shared-storage tooling

Generated Android applications may request broad shared-storage access when
their native tools, plug-ins, or backup workflows genuinely require ordinary
C++ filesystem semantics. The Phase 5 diagnostic uses `std::filesystem` and
`std::ofstream` directly against `/sdcard/Download`; it does not depend on
MediaStore or a Java storage bridge.

Legacy `READ_EXTERNAL_STORAGE` and `WRITE_EXTERNAL_STORAGE` declarations are
retained for compatibility, while Android 11+ direct shared-storage access
requires the separately granted `MANAGE_EXTERNAL_STORAGE` special access.
Manifest presence and actual write success are reported independently.

Android process privilege does not automatically become a Lua plug-in
capability. The current restricted host has no filesystem API. Future plug-in
file capabilities must define bounded roots, atomic mutation behavior, and
backup/cartridge boundaries explicitly.

## Memory target

The framework targets applications capable of operating within a 256 MiB
resident working set, with swap treated as emergency capacity rather than
frame-time memory. Implementations therefore use bounded queues, streaming
archive and file operations, incremental font glyphs, explicit CPU and GPU
asset budgets, prompt release of decoded pixels, bounded undo history, and
observable current and peak subsystem memory.

Exact budgets require target-device measurements, but no framework API should
require loading a complete archive, asset collection, document collection, or
message history into memory.

## Builder direction after Phase 5

The development `.sq` proof begins that refactor without replacing the proven
project lifecycle. SDL Android is now one declarative, versioned, locally
registered template package. Manual template registration validates and copies
a self-contained archive into an immutable local store. The template directly
requires Squared Application, Squared Scene2D, and Squared Messaging. Scene2D
requires Graphics2D, preserving dependency-first composition.
Application is a header-only lifecycle contract represented by a CMake
`INTERFACE` module. Graphics owns color and the SDL/OpenGL ES context, while
Math owns the platform-neutral vector and matrix layer. Graphics2D owns
textures, sprites, atlases, batching, and cameras and requires exact Graphics
and Math versions. Messaging independently owns the Telegram and Telegraph
layer and declares exact Squared Data and Squared Time dependencies; Data owns
the strict JSON layer and its private yyjson source. The builder resolves the graph in
deterministic dependency-first order and rejects missing versions, cycles, and
version conflicts before rendering. Executable template hooks remain disabled
and require an explicit future format decision if introduced later.

## Phase 6 handoff

The scene graph and UI layer begins in Phase 6. The first independent Scene2D
slice now supplies `Actor`, `Group`, and `Stage` hierarchy semantics. The
following higher-level behavior remains outside the Phase 5 graphics MVP:

- actions and action composition;
- input routing, hit testing, focus, and event propagation;
- widgets, layout containers, and skin-backed UI controls.

Phase 6 will build those higher-level types on the proven Phase 5 camera,
batch, texture, atlas, font, shader, skin, and messaging foundations. Keeping
the boundary explicit prevents application code from depending on placeholder
scene APIs that would later need incompatible replacement.
