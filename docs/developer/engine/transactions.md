# Transactions

Programmer counterpart: [errors](../../programmer/engine/errors.md).
Specification: §2.15.7–2.15.14, D-024.

## The constraint that decided the design

Termux. `/tmp` is frequently on a different filesystem from the user's storage,
and cross-device `rename(2)` fails with `EXDEV`. Any staging scheme that builds
in a temporary directory and moves the result into place therefore breaks on the
primary target platform — and breaks *late*, after all the work is done.

D-024's answer: **staging always occurs on the target filesystem**, so
cross-device rename cannot arise. Not "usually", not "we retry with a copy" —
cannot arise.

## New project generation

```text
<parent>/.<name>.squared-tmp/     build the entire workspace here
        |
        | rename()                one call, same filesystem, atomic
        v
<parent>/<name>/
```

The staging directory is a *sibling* of the target, which is what puts it on the
target's filesystem by construction rather than by hope.

Failure at any point removes the staging tree and returns. The target never
existed, so there is nothing to roll back to — §2.7.13's "an incomplete
workspace must not be presented as a valid project" holds structurally rather
than by cleanup discipline.

A leftover `.<name>.squared-tmp` means a previous run died before its rename.
Nothing was ever visible as a project, so it is removed and the removal is
reported.

## Durability

| Mode | Behaviour |
|---|---|
| `durable` (default) | `fsync` each staged file, then the parent directory after the rename |
| `fast` | skip the content fsync |

Recorded in the operation result and in the metadata record, so a caller always
knows which was used (§2.15.11).

The parent-directory `fsync` after the rename is the half people omit. Without
it a rename can be lost on power failure even though every byte of content
survived, and the workspace comes back not existing.

## What is not implemented

Regeneration into an existing workspace. That needs the write-ahead journal of
§2.15.9, per-file staging under `.squared/staging/`, crash recovery with the
four journal states, and the `damaged` terminal state of §2.15.13.

`project.generate` therefore refuses an existing target with
`filesystem.workspace.exists` and says why in the error's `hint`. Refusing
loudly is the right failure: a half-implemented journal would corrupt user work
in exactly the cases the journal exists to protect.

The pieces already in place for it: provenance hashes are written on every
generation, so conflict detection has its input; the plan is reified, so an
update plan is the same type; ownership classes are assigned and enforced. What
is missing is the journal and the apply-in-place loop.
