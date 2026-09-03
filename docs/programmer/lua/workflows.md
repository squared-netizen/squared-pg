# Writing a workflow

Developer counterpart: [binding layer](../../developer/lua/binding.md).

A workflow is a directory with a `workflow.json` and an entry script. It decides
which template is selected, which kits are applied, in what order the engine is
called, and what to do when a call fails. The engine decides none of that.

## Discovery

Searched in order:

```text
1. repository     lua/workflows/
2. user config    $XDG_CONFIG_HOME/squared-pg/workflows/
```

Run one with `sqpg --workflow <id>`. To customise the default, copy
`lua/workflows/workflow.generate.default/` into your config directory, change
it, and run it by id. No rebuild.

## `workflow.json`

```json
{
  "schema_version": 1,
  "id": "workflow.generate.default",
  "version": "1.0.0",
  "entry": "init.lua",
  "requires_engine": "^0.1",
  "requires_capabilities": ["capability.project.generate@^1.0"],
  "trust": "trusted"
}
```

`trust` is a *request*. The host assigns the tier and may refuse.

## What the host gives you

### `engine`

```lua
engine.execute{ operation = "project.generate", parameters = config }
engine.project.generate(config)              -- sugar; same registry entry
engine.template.resolve("template.termux.cpp")  -- a lone string binds to the
                                                -- first required parameter
engine.state()        -- "ready"
engine.version()      -- "0.1.0"
engine.api_version    -- boundary contract version
engine.capabilities   -- array of { token, version, summary }
```

The call surface is generated from the operation registry, so it always matches
what the engine actually implements. `engine.operations()` enumerates it.

### `sqpg`

```lua
sqpg.args           -- argv after the program name, unparsed
sqpg.program        -- argv[0]
sqpg.workflow_id
sqpg.workflow_path
sqpg.cwd
```

Host information, not engine information. Parsing `sqpg.args` is your job — that
is the point.

## Results

```lua
local result = engine.project.plan(config)

result.ok            -- boolean
result.status        -- "success" | "warning" | "failure" | "cancelled"
result.data          -- operation-specific payload
result.warnings      -- always check these, even on success
result.diagnostics
result.errors        -- array
result.error         -- alias for errors[1]
```

Branch on `result.error.code` or `result.error.category`. Never on
`result.error.message` — its wording is not part of the contract.

## Rules

- **Never reimplement an engine capability.** Filesystem management, template
  processing, resolution, transactions and validation are engine services. If
  one is missing, that is an engine gap to raise.
- **Never address a resource by path.** Resources are identities. A workflow
  that knows where `resources/templates/` lives has coupled itself to repository
  layout.
- **User-space I/O only.** Read the config files and streams the user pointed
  you at; write to stdout and stderr. Do not read generator-owned resources or
  write into a workspace directly — the engine owns both.
- **Document your precedence.** If you combine sources, say which wins.

## Shared modules

```lua
local args   = require("squaredpg.args")    -- parsing, config loading, merging
local report = require("squaredpg.report")  -- output, errors, JSON, plan rendering
```

## Smallest useful workflow

```lua
local report = require("squaredpg.report")

local name = sqpg.args[1]
if name == nil then
  report.err("usage: sqpg --workflow workflow.mine <name>")
  return 2
end

local result = engine.project.generate({
  name = name,
  output = "./" .. name,
  ["template"] = "template.termux.cpp",
  kits = { "kit.terminal" },
})

if not result.ok then
  report.failure(result)
  return 1
end

report.out("created " .. result.data.workspace)
return 0
```

Note `["template"]` — `template` is not a Lua keyword, but writing it bracketed
keeps it visually distinct from the many other places the word appears.
