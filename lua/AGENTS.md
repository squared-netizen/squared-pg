# AGENTS.md — `lua/`

Local conventions for the Lua control layer. Supplements the repository
`AGENTS.md`; where the two disagree about this directory, this file wins
(§1.12).

## What belongs here

`lua/` is the orchestration layer (§2.5). Modules here decide *what* the
engine is asked to do and *in what order*. They do not implement generator
capability.

```text
lua/squaredpg/     shared modules, required by workflows
lua/workflows/     workflow directories, each with workflow.json + entry script
```

## Rules

- **Never reimplement an engine capability.** Filesystem management, template
  processing, resource resolution, transactions and validation are engine
  services (§2.5.1). If a workflow needs one and it is missing, that is an
  engine gap to raise, not a thing to write in Lua.
- **Reach the engine only through the bound API.** The `engine` table is the
  whole surface (§2.5.7). There is no back door and adding one would make the
  boundary unverifiable.
- **Never address a resource by path.** Resources are identities (§2.6.7).
  A workflow that knows where `resources/templates/` is has coupled itself to
  repository layout, which §2.5.9 forbids.
- **User-space I/O only.** A workflow may read the configuration files and
  streams the user pointed it at, and write to stdout and stderr. It must not
  read generator-owned resources or write into a workspace directly (D-013).
- **Branch on results, not on text.** `result.ok`, `result.error.category` and
  `result.error.code` are the contract (§2.5.8). Parsing a message string
  couples the workflow to wording that is explicitly not part of the contract.
- **Document configuration precedence.** §2.5.6 makes precedence between
  sources a workflow decision, so a workflow that combines sources says in its
  usage text which wins.

## Trust

A workflow declares a `trust` tier in `workflow.json`, and the *host* decides
whether to grant it (§2.14.4). A workflow may not elevate itself. Anything
that calls `loadfile`, `os.execute` or `io` write access is `trusted` and must
say so; a `sandboxed` workflow reaches the filesystem only through engine
services.

## Style

- Two-space indent, `snake_case` locals and functions.
- Modules return a table; no globals except the host-provided `engine` and
  `sqpg`.
- `local x = require("...")` at the top of the file, never mid-function.
