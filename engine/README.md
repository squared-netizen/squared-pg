# engine

The native C++20 engine: generation capability, exposed through a documented
control surface.

- `include/squared/pg/` — the public API. This is the whole supported surface.
- `src/` — implementation. Not on any consumer's include path (§1.3).
- `lua/` — the Lua binding layer, built as a separate target so an embedder may
  link the engine alone.
- `tests/` — the suite; run with `make check` from the repository root.

Conventions for working here: `AGENTS.md`.
Public API: [`docs/programmer/`](../docs/programmer/README.md).
Internals: [`docs/developer/`](../docs/developer/README.md).
