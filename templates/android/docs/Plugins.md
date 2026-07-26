---
title: Lua Plug-ins
tags:
  - lua
  - plugins
  - api
---

# Lua Plug-ins

Plug-ins are APK assets beneath:

```text
app/src/main/assets/lua/plugins/PLUGIN_ID/
```

The trusted registry `app/src/main/assets/lua/plugins.lua` lists plug-in IDs in
deterministic load and lifecycle order:

```lua
return {
    "diagnostics",
    "orbit"
}
```

There is no runtime directory scan. This keeps behavior stable across Android
packaging tools and filesystems.

## Manifest

Every plug-in provides `manifest.lua`:

```lua
return {
    id = "example",
    version = "1.0.0",
    api_version = 1,
    entry = "main",
    capabilities = {
        "log"
    }
}
```

The `id` must match its registry entry and directory. `version` uses
`MAJOR.MINOR.PATCH`. `api_version` must match the generated runtime.

## Capabilities

| Capability | Exposed host functions |
| --- | --- |
| `log` | `log` |
| `display` | `logical_size`, `set_background`, `set_tile` |
| `application` | `request_quit` |

Every plug-in also receives read-only metadata:

- `host.api_version`
- `host.plugin_id`
- `host.plugin_version`

Unknown and duplicate capabilities reject that plug-in without stopping other
plug-ins.

## Entry module

The entry module returns optional callbacks:

```lua
local plugin = {}

function plugin.init()
end

function plugin.update(delta_seconds)
end

function plugin.event(kind, first, second)
end

function plugin.shutdown()
end

return plugin
```

## Local modules

Plug-ins can load modules within their own directory:

```lua
local palette = require("palette")
local model = require("model.character")
```

Names are converted to paths below the plug-in root. Absolute paths,
traversal, malformed identifiers, missing modules, and dependency cycles are
rejected. Each plug-in has a separate module cache.

Plug-ins cannot load modules belonging to another plug-in. Shared modules
should be exposed through a deliberately versioned host API after their
contract becomes stable.

## Event names

The default native host translates:

- `touch_down`
- `touch_move`
- `touch_up`
- `back`
- `background`
- `foreground`

The numeric event payloads remain renderer-independent logical coordinates.
