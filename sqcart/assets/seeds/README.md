# Manifest seed

The template used by `sqcart create` to write a `SQ-INF/manifest.json` for a
plain folder that has none.

**One seed, not one per kind.** Format 1 shipped six, each carrying its
kind's body, which meant this tool held the schema of every role in the
ecosystem in string literals. Format 2 seeds the envelope and leaves
`consumers` empty for whoever knows what belongs in it (spec §5.6).

`create` cannot fill a consumer section and does not try. Guessing on a
consumer's behalf is the coupling that was removed.

These files are the source of truth, but they are **not read at runtime**.
They are embedded in the binary as raw string literals in
`app/src/seeds.cpp`, because a CLI that must locate an assets directory at
runtime breaks the moment it is moved, symlinked, or run from another
working directory — and this tool has to work from an arbitrary `cd` on
Termux.

`tests/test_seeds.cpp` reads these files and asserts byte equality with the
embedded copies, so the two cannot drift silently. Edit the JSON here, mirror
it in `seeds.cpp`, and the test tells you if you missed.

## Placeholders

Substituted by `cmd_create`. Every one is derived from the folder or from a
command-line flag; none is a timestamp or a hostname, because a seeded
manifest must be reproducible (§9.3).

| Placeholder | Source |
|---|---|
| `{{kind}}` | `--kind`, required; any well-formed token (§5.2.1) |
| `{{id}}` | `--id`, else `local.` plus the sanitised directory name |
| `{{version}}` | `--version`, default `0.1.0` |
| `{{title}}` | `--title`, else the directory name verbatim |

Four, down from six. `{{external_id}}` and `{{entry_module}}` went with the
kind bodies that needed them, and `{{asset_entries}}` with the asset-bundle
body it filled.

The derived id is `local.`-prefixed regardless of kind. Which prefix belongs
to which role is ecosystem policy (§5.2.2), not something this tool is
entitled to enforce.
