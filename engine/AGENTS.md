# AGENTS.md — `engine/`

Local conventions for the native engine. Supplements the repository
`AGENTS.md`; where the two disagree about this directory, this file wins
(§1.12).

## The one rule

**The engine provides capability. It never decides policy.**

Before adding anything here, ask whether a workflow could reasonably want the
opposite behaviour. If it could, the decision belongs to Lua and the engine
should expose the mechanism instead. §2.2.5 lists what the engine may never
contain: argument parsing, prompts, workflow logic, single-workflow
assumptions, hidden paths, presentation policy.

## Layout

```text
engine/include/squared/pg/   public headers — the control surface
engine/src/                  implementation; not reachable by consumers
engine/src/services/         core services (§2.4.3)
engine/src/support/          internal helpers
engine/lua/                  the Lua binding layer (§2.6)
engine/tests/                the test suite
```

The public/private split is enforced by include paths, not by convention:
nothing under `engine/src/` is on a consumer's include path (§1.3).

## Adding an operation

1. Register a descriptor in `Engine::Impl::register_operations`: identity,
   typed parameters, required services, required capabilities, whether it
   mutates a workspace, and the codes it may return (§2.4.6).
2. Implement it as a composition of service calls. **No capability may exist
   only at the operation layer** (D-008) — if the handler does real work that
   no service exposes, that work belongs in a service.
3. Nothing else. The Lua binding is generated from the registry, so the
   operation becomes callable with no edit to `engine/lua/` (§2.6.3).

## Errors

Every fallible function returns `Result<T>`. An error carries a closed
`category` for branching and an open dotted `code` for diagnosis (§2.15.1).

Two habits matter more than the rest:

- **Name what is wrong, in a list.** `template.parameter.missing` reports every
  missing parameter, not the first, so a workflow asks the user one question
  instead of five (§2.7.2).
- **Put the evidence in `diagnostics`.** §2.15.3 is explicit that "no
  compatible version found" is not actionable and the list of versions
  actually considered is.

Exceptions may exist internally but must never cross the public API, and never
reach Lua (§2.4.8, §2.6.12).

## Determinism

§2.7.13 requires equivalent inputs to produce equivalent output, byte-identical
where nothing is timestamp- or ordering-sensitive. In practice:

- No clock reads in anything that reaches a generated file or metadata.
- No environment lookups outside `EngineConfig`.
- Sort anything derived from filesystem enumeration before using it.
- `Value` objects preserve insertion order; do not swap them for `std::map`.

## Termux

- Never stage in `/tmp` or `$TMPDIR`. Staging is on the target filesystem
  (D-024) because cross-device `rename(2)` fails on Android.
- No elevated privileges, no heavy toolchains, no assumptions about `/proc`
  beyond `/proc/self/exe` with a fallback.
