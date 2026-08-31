# Manifest seeds

Templates used by `sqcart create` to write a `SQ-INF/manifest.json` for a
plain folder that has none.

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
| `{{id}}` | `--id`, else derived from the directory name |
| `{{version}}` | `--version`, default `0.1.0` |
| `{{title}}` | `--title`, else the directory name verbatim |
| `{{external_id}}` | last segment of `{{id}}` |
| `{{asset_entries}}` | computed: one entry per payload file (asset-bundle) |
| `{{entry_module}}` | detected entry point (cartridge) |

A seed may ignore any placeholder it does not need.
