---
title: Phase 5 MVP Validation
tags:
  - android
  - graphics
  - phase-5
  - testing
status: candidate
---

# Phase 5 MVP Validation

Version 0.5.0 is a candidate until the private toolchain, generated project,
native build, APK packaging, and on-device OpenGL ES behavior pass on Android
ARM64.

## 1. Verify and test the generator

From the extracted candidate directory:

```bash
sha256sum -c SHA256SUMS.txt
lua5.4 toolchain.lua
```

The generator tests verify that a generated project contains every public
graphics header and implementation, requires OpenGL ES 2, links `GLESv2`, uses
SDL's bundled GLES2 headers, and takes the Squared rendering path instead of
`SDL_Renderer`. They also verify the generated libGDX atlas fixture, owning
atlas API, indexed lookups, relative-path safety, rotation path, and
transactional commit structure.

The same run verifies and extracts yyjson 0.12.0, compiles the
`squared::data` wrapper, and runs native strict-JSON tests. Generated projects
must contain the yyjson source, its MIT license, the public JSON header, and
the wrapper implementation. It also verifies the JSON-driven SDL_ttf
diagnostic, bundled DejaVu Sans Mono font, and font license.

Generator tests also verify the platform/application boundary: the SDL entry
point imports only `create_application()`, developer `.c` and `.cpp` files are
automatically discovered beneath `application/src/`, and the obsolete
top-level generated `main.cpp` is absent.

Native tests also verify time scaling, pause/resume, manual reset, deadline
ordering, cancellation, queue capacity, overflow rejection, and bounded
delivery.

Messaging tests verify queued directed delivery, explicit immediate delivery,
broadcast subscription order, delayed delivery against domain time, handled
and cancelled receipts, endpoint loss, queue limits, and invalid-delay
reporting. They also verify sole-provider registration, fresh state generated
at subscription time, delivery only to the new subscriber, no-current-state
registration, explicit provider failure, and atomic rollback when the initial
state queue is full. Pending persistence tests verify allocation-bounded
ordered inspection, relative delays, deterministic JSON, strict round-trip
parsing, atomic restoration, delivery order, nonempty-queue rejection,
capacity rejection without partial mutation, and explicit refusal to persist
process-local provider targets.

The Termux-native build resolves `/system/lib64/libGLESv2.so`; the GitHub NDK
build resolves the NDK linker stub. Neither path packages an OpenGL
implementation in the APK.

## 2. Install the candidate

```bash
./build/private-lua/bin/lua-5.4.8 \
  tools/private-run.lua \
  setup.lua install

hash -r
sdl-pg version
sdl-pg doctor
```

Expected version output:

```text
sdl-pg 0.5.0 (private Lua 5.4.8)
```

Existing SDL2-kit and Gradle-Wrapper registrations are reused.

## 3. Generate and build the MVP

```bash
sdl-pg new phase5-graphics-hack \
  --package dev.squarednetizen.phase5graphicshack

cd "$HOME/sandbox/phase5-graphics-hack"
lua5.4 tools/build.lua
sdl-pg docs
```

Confirm the generated C++ reference includes the `squared::graphics`,
`squared::graphics2d`, `squared::math`, and `squared::data` namespaces:

```text
build/docs/cpp/html/index.html
```

## 4. Validate on device

Install the generated debug APK. Confirm:

- the application starts without closing;
- the background and moving tile still render;
- the panel beside the lifecycle square displays
  `JSON → TTF: OK │ round-trip verified`;
- the status square begins blue;
- touching the status square changes it to green;
- after backgrounding and returning, green means the process survived, while
  blue means Android recreated it;
- the smaller square beneath it alternates magenta and cyan once per
  application-time second;
- the tiny square beside the timing square becomes green after authoritative
  provider state is delivered through the ordinary Telegram queue;
- the second tiny square beside it is green after a pending Telegram completes
  a JSON snapshot/restore round trip; orange means that diagnostic failed;
- the text report shows `PASS`, `FAIL`, or `PENDING` for every sentinel and
  displays a compact JSON provider Telegram;
- backgrounding freezes that smaller square's application clock, and returning
  resumes it without a wall-clock catch-up burst;
- a red status square means the atlas failed and the fallback renderer kept
  the diagnostic activity alive;
- touch moves the tile and release resumes automatic movement;
- Android Back exits through the Lua host API;
- backgrounding and foregrounding preserve rendering;
- rotating or resizing the window does not leave a stale viewport.

The text is loaded from `diagnostics/json-ttf-status.json`, serialized
deterministically, parsed again, and rendered with SDL_ttf. Its font path,
point size, text color, and panel color also come from that JSON document.
This makes visible text proof of native JSON execution rather than merely a
compile-time check.

If text cannot be displayed, the diagnostic panel uses these fallback colors:

| Color | Failure stage |
| --- | --- |
| Magenta | JSON asset read |
| Red | Strict JSON parse |
| Orange | JSON schema |
| Yellow | Deterministic JSON write |
| Purple | JSON round-trip |
| Cyan | SDL_ttf initialization |
| Blue | Font open |
| White | Glyph rendering or texture upload |

The exact stage and error are also written through `SDL_Log`.

The blue/green square is deliberately in-memory rather than persistent. It
makes Android lifecycle behavior observable without introducing the Phase 6
widget layer or a persistence design prematurely.

The magenta/cyan square proves the time domain and Telegram dispatcher on
device. Delayed directed Telegrams drive it from the application's
once-per-frame update; there is no background timer or per-object clock.

The tiny red/green square proves `TelegramProvider` without relying on logs:
red is the pre-delivery fallback and green means a fresh JSON state value was
provided specifically to the new subscriber and handled during dispatcher
update.

The second tiny orange/green square proves pending-message persistence without
filesystem assumptions. Its probe schedules a delayed Telegram, snapshots
remaining time and stable IDs, writes and parses deterministic JSON, restores
into an empty dispatcher, advances its manual timepiece, and confirms delivery.

Enable **Allow access to manage all files** for the installed diagnostic
application, relaunch it, and confirm:

```text
LOG: PASS /sdcard/Download/phase5_snapshot_hack-diagnostics.log
```

Then inspect the native log from Termux:

```bash
sed -n '1,80p' \
  /sdcard/Download/phase5_snapshot_hack-diagnostics.log
```

The log must contain the same explicit status lines and compact `MESSAGE` JSON
shown on screen. Without the special Android grant, `LOG: FAIL` is expected
even though the legacy manifest declarations are present.

After these checks pass, the candidate is suitable for committing and running
the manually dispatched GitHub verification workflow.
