---
title: Lua Scripting
tags:
  - lua
  - api
  - architecture
---

# Lua Scripting

The generated application starts at the trusted APK asset
`app/src/main/assets/lua/bootstrap.lua`. The bootstrap loads the runtime and
returns one aggregate callback table to C++:

```lua
return {
    init = function() end,
    update = function(delta_seconds) end,
    event = function(kind, first, second) end,
    shutdown = function() end
}
```

Application logic lives in plug-ins rather than the bootstrap. See
[[Plugins|Plug-ins]] for manifests, local modules, capabilities, and lifecycle
ordering.

## Native boundary

The trusted bootstrap initially receives the complete native `host` table:

| Function | Purpose |
| --- | --- |
| `host._read_asset(path)` | Read a validated Lua APK asset for the trusted loader |
| `host.runtime_version()` | Return the embedded Lua runtime version |
| `host.log(message)` | Write an SDL log entry |
| `host.logical_size()` | Return logical width and height |
| `host.set_background(r, g, b)` | Publish the next clear color |
| `host.set_tile(x, y, size, r, g, b)` | Publish the example tile state |
| `host.request_quit()` | Ask the native loop to stop safely |

The bootstrap removes the raw `host` and global `load` references after the
trusted runtime captures them. Plug-ins receive read-only proxies containing
only their declared capabilities. They never receive `_read_asset`.

## Protected execution

Native lifecycle calls use protected Lua calls. The plug-in manager also wraps
each plug-in callback independently.

- A plug-in load failure disables only that plug-in.
- An `init` failure disables that plug-in for the session.
- Another callback failure disables only that callback.
- Other plug-ins continue in deterministic registry order.
- Shutdown runs in reverse order for plug-ins that initialized successfully.

## Open libraries

The native runtime opens:

- Lua base functions;
- coroutine;
- table;
- string;
- math;
- UTF-8.

It omits `io`, `os`, `package`, and `debug`. The filesystem-backed base
functions `dofile` and `loadfile` are removed.

Plug-in environments are narrower still. They receive selected base functions
and read-only proxies for coroutine, table, string, math, and UTF-8. They do
not receive `load`, raw metatable functions, the raw native host, or ordinary
filesystem and package loading.

This is a capability boundary for cooperative application plug-ins. It is not
a hardened sandbox for hostile code.
