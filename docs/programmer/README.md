# Programmer documentation

For people **using** squared-pg: calling its APIs, writing workflows, authoring
templates and kits.

Nothing here describes how the engine works internally. That is
[`docs/developer/`](../developer/README.md), and each page below links to its
counterpart there.

## Pages

### Engine
- [Control surface](engine/control-surface.md) — the `Engine` class, lifecycle, and how to call an operation
- [Operations](engine/operations.md) — every registered operation, its parameters and its result
- [Errors](engine/errors.md) — categories, codes, and how to branch on them
- [Values](engine/values.md) — the data types that cross the boundary
- [Capabilities](engine/capabilities.md) — what a manifest may require

### Lua
- [Writing a workflow](lua/workflows.md) — the `engine` and `sqpg` tables, discovery, trust
- [The CLI](lua/cli.md) — `sqpg` commands and options

### Resources
- [Authoring a template](resources/templates.md)
- [Authoring a kit](resources/kits.md)
- [Manifest reference](resources/manifests.md)

## Two-minute version

```cpp
#include <squared/pg/engine.hpp>

squared::pg::EngineConfig config;
config.resource_roots.emplace_back("resources/templates");
config.resource_roots.emplace_back("resources/kits");

auto engine = squared::pg::Engine::create(std::move(config));
if (!engine->initialize().succeeded()) { /* report and stop */ }

squared::pg::Value request = squared::pg::Value::object();
request.set("name", "hello");
request.set("output", "./hello");
request.set("template", "template.termux.cpp");
request.set("kits", squared::pg::Value::strings({"kit.terminal"}));

const auto result = engine->execute("project.generate", request);
if (!result.succeeded()) { /* result.errors() tells you why */ }

(void)engine->shutdown();
```

The same thing from Lua:

```lua
local result = engine.project.generate({
  name = "hello",
  output = "./hello",
  ["template"] = "template.termux.cpp",
  kits = { "kit.terminal" },
})
if not result.ok then print(result.error.code, result.error.message) end
```

And from a shell:

```sh
sqpg new hello --template template.termux.cpp --kit kit.terminal
```

All three go through the same operation registry entry.
