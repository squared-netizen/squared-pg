# Manifest handling

Programmer counterpart:
[manifest reference](../../programmer/resources/manifests.md).

## The divergence (D-029)

The specification defines a manifest envelope in §2.8.3:

```json
{ "schema_version": 1, "id": "...", "type": "template", "version": "1.0.0",
  "requires_engine": "^1.0", "requires_capabilities": [], "template": { } }
```

The cartridge format specification — which the shipped seed manifests, the
sqcart reader, and the sqcart test fixtures all implement — defines a different
one:

```json
{ "format": "squared-cartridge", "format_version": 1, "id": "...",
  "kind": "template", "version": "1.0.0",
  "engine": { "id": "squared-pg", "version": ">=0.1.0 <0.2.0" },
  "requires_features": [], "template": { } }
```

The identifier grammars differ too: §2.8.1 writes segments as
`[a-z][a-z0-9]*(-[a-z0-9]+)*` with no segment-count limit; cartridge format §5.2
writes `[a-z][a-z0-9_]*` with two to eight segments.

**The engine implements the cartridge form.** AGENTS.md's temporary stance makes
the repository ground truth during initial implementation, and the repository
already contains working manifests, a tested reader, and a CLI that writes them.
Implementing §2.8.3 would have meant either a second manifest reader or
rewriting sqcart, and both are worse than recording the divergence.

`ResourceId::parse` accepts both `_` and `-` as segment separators —
underscores because manifests use them, hyphens because the specification asks
for them. That is the one place the two are reconciled rather than one being
chosen.

**This needs a spec amendment**, either aligning §2.8.1–2.8.3 with the cartridge
format or stating that generator resources are cartridges and inherit its
envelope. The latter is the smaller change and matches what the code does.

## Extension fields

The generator architecture needs fields the cartridge format does not model:

| Field | Body | Purpose | Spec |
|---|---|---|---|
| `working_directory` | template | the one user working directory | §2.7.3, §2.8.6 |
| `integration_areas` | template | anchors kits may write | §2.8.6 |
| `integration_arity` | template | `single` or `multi` per area | §2.9.3 |
| `processor` | template | `copy` or `substitute` | §2.8.8 |
| `parameters[].default` | template | literal default | §2.8.7 |
| `parameters[].default_from` | template | derived default | §2.8.7 |
| `executable` | any | globs to mark executable | — |
| `external.acquisition` | kit | `package`, `system` or `vendored` | §2.9.4 |
| `conflicts_with` | kit | declared incompatibility | §2.9.3 |

These are read from `Manifest::raw_json()` via `detail::manifest_extension`,
which the format guarantees is preserved verbatim (§5.4, default tier: ignore
unrecognised members).

Reading them from raw JSON rather than forking sqcart's manifest model keeps the
divergence in one direction — the engine knows more than the format, and the
format never has to know about generation. That is the same scope rule
`sqcart/AGENTS.md` states: sqcart knows the manifest schema, never resolution,
selection, generation or workflow behaviour.

The cost is that these fields are unvalidated by sqcart. Where one is malformed
the engine falls back to a documented default rather than failing:
`working_directory` to `sq_app`, `processor` to `substitute`, arity to `single`.
Each fallback is the conservative choice.

## Where a fallback is *not* used

An unknown `processor` is a hard failure (`capability.unsatisfied`), because
§2.14.1 requires an unsatisfied capability to fail at resolution and silently
falling back to `copy` would ship a workspace full of `{{project_name}}`.
