# D-061 — Ownership tables glob the kit's own tree, enumerate outside it

**Status:** accepted (codifies existing practice in kit.terminal)

## Decision

A kit's `ownership.generated` uses:

- **A glob for its own subtree** — `sq_kit/**`. A kit owns its kit tree by
  definition; enumerating individual headers restates that.
- **Literal paths for files dropped outside it** — `mk/kit_<name>.mk`, and any
  other file placed into a directory the kit does not own outright.

`ownership.user` and `ownership.shared` follow the same rule: glob a subtree
the kit hands wholly to the user (`sq_lua/**`), enumerate anything narrower.

## Rationale

Both forms were in use. kit.terminal globbed `sq_kit/**`; kit.lua, kit.opengl,
and kit.termux enumerated their single header. The D-057 flatten showed the
difference is not cosmetic: the glob was immune to the header move, while all
three literal entries needed rewriting, and a stale entry would have left a
file that `workspace.verify` no longer tracks.

The split follows ownership itself. Everything under `sq_kit/` belongs to the
kit whatever it is named, so the pattern states a fact that stays true across
refactors. Files outside that subtree share a directory with other producers,
so the claim has to be specific enough to say which file is whose.

This is the same separation recorded in the earlier ownership work: claiming
specific files and claiming a region are distinct concerns, and the manifest
should say which one is meant rather than leave it to be inferred.

## Consequences

- template.kit scaffolds `sq_kit/**` plus a literal `mk/kit_<name>.mk`,
  matching kit.terminal.
- kit.lua, kit.opengl, and kit.termux should be converted to the glob form.
  Their literal entries are correct post-flatten but remain fragile.
- A kit that places files in a third location declares it literally and, if
  that location is shared with other kits, records it in `ownership.shared`
  rather than `generated`.

## Not covered

Whether `workspace.verify` should warn when a literal entry names a file
inside a subtree the same kit already globs. Redundant rather than wrong, but
it is the shape a stale entry takes.
