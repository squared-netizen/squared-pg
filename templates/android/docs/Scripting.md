---
title: Lua Scripting
tags:
  - lua
  - api
  - architecture
---

# Lua Scripting

The APK asset `app/src/main/assets/lua/main.lua` returns a callback table.
Every callback is optional:

```lua
return {
    init = function() end,
    update = function(delta_seconds) end,
    event = function(kind, first, second) end,
    shutdown = function() end
}
```

Callbacks run through protected Lua calls. A callback error is written to the
SDL log, removed from the Lua stack, and disables that callback for the
remainder of the session; it does not unwind through C++ or create a per-frame
error storm.

## Host API

The native host exposes these functions:

| Function | Purpose |
| --- | --- |
| `host.log(message)` | Write an SDL log entry |
| `host.logical_size()` | Return logical width and height |
| `host.set_background(r, g, b)` | Publish the next clear color |
| `host.set_tile(x, y, size, r, g, b)` | Publish the example tile state |
| `host.request_quit()` | Ask the native loop to stop safely |

The `host` table is the compatibility boundary. Game and tool code should call
this API instead of binding scripts directly to SDL or Android.

## Open libraries

The generated runtime opens only:

- Lua base functions;
- coroutine;
- table;
- string;
- math;
- UTF-8.

It deliberately omits `io`, `os`, `package`, and `debug`. Future trusted-tool
profiles can add capabilities explicitly without weakening the default plug-in
environment.

The filesystem-backed base functions `dofile` and `loadfile` are also removed.
