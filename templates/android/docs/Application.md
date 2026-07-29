---
title: {{PROJECT_TITLE}} Application Boundary
tags:
  - application
  - architecture
  - cpp
---

# Application Boundary

Generated projects separate platform integration, framework code, and
developer-owned application code:

```text
app/src/main/cpp/
├── platform/
│   └── sdl_main.cpp
├── squared/
└── application/
    ├── CMakeLists.txt
    ├── include/
    └── src/
```

`platform/sdl_main.cpp` is generator-managed. It initializes SDL, translates
supported SDL events into `squared::application::Event`, owns the frame loop,
and calls the single `{{PROJECT_ID}}::create_application()` factory.

Files beneath `application/src/` are developer-owned. Its CMake target uses a
recursive `CONFIGURE_DEPENDS` scan limited to that directory, so adding or
removing `.c` and `.cpp` files does not require hand-editing CMake. Headers do
not need source-list entries and may be added beneath `include/` normally.

Framework implementation beneath `squared/` and platform implementation
beneath `platform/` remain explicit build inputs. Generator updates must not
copy example implementations into the developer application directory.

The SDL adapter supplies one elapsed frame delta. Developer code advances its
application `Timepiece` exactly once from that delta; other systems borrow the
read-only `Clock` view. See [[Time|Time Domains]].

The current JSON/TTF diagnostic remains in the default application as a
transitional SDL_ttf use. The Phase 5 BitmapFont implementation will move
ordinary application text behind the Squared framework API.
