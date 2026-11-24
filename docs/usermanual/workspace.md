---
title: Your workspace
tags: [usermanual]
---

# Your workspace

## Who owns what

Every file a generated workspace contains is one of:

| Class | Meaning |
|---|---|
| `seeded` | written once, then **yours**. Edit freely; the generator will not overwrite it. |
| `generated` | the generator's. Rewritten on update — anything you write there is lost. |
| `user` | yours, untouched. |
| `metadata` | the provenance record. |

The rule of thumb: **`mk/` is the generator's, everything else is yours.** Your
build settings go in `Makefile`, which is yours and which every generated
variable is written to allow overriding:

```sh
make SQ_MIN_SDK=21 apk
```

## The record

`SQ-INF/` is the workspace's record, the workspace being a cartridge:
`SQ-INF/manifest.json` (kind `workspace`: what it was made from, where its
parts are) and `SQ-INF/files.json` (every file the generator wrote, with its
class, origin and SHA-256). square reads them (`pkg install square`):

```sh
square info           # what this workspace is and was made from
square owner PATH     # a file's class and origin, and whether it changed
square verify         # every listed file that changed or went missing
```

**`verify` does not repair anything.** What to do about a generated file you
changed is your decision; it only tells you an update would overwrite it. It
exits non-zero when anything changed, so it works in a `check` target.

## Directory shapes

The names vary by template. Common ones:

| Directory | What |
|---|---|
| `sq_app/` | your application code. Portable — no platform headers. |
| `sq_android/` | the Android platform layer. Yours, rarely edited. |
| `sq_kit/` | whatever kits contributed. Generated. |
| `mk/` | build fragments. Generated. |
| `build/` | output. Disposable. |

The one worth defending is `sq_app/`. Keeping platform headers out of it is
what lets the same application code compile under a different template.

## Regenerating

This engine build generates **new** workspaces only. Regenerating into an
existing one is refused:

```
error: the output location already exists
  code:      filesystem.workspace.exists
```

Generate elsewhere and merge by hand, or remove the old workspace.
