---
title: Regeneration and update model
tags: [design, not-normative]
---

# Regeneration and update model

> [!warning] Not normative
> This document is the specification §2.7.11 carried until D-033/D-054. It
> describes a mechanism **no implementation provides**: `project.generate`
> refuses an existing workspace, and there is no write-ahead journal.
>
> It is retained because the thinking is sound and the groundwork exists —
> provenance hashes, the reified plan, enforced ownership classes were all
> built for this. What is missing is the transactional guarantee, and a
> partial journal corrupts user work in exactly the cases it exists to
> prevent.
>
> §2.7.11 is a stub pointing here. Do not read this as a description of
> behaviour.


Regeneration is the application of generator operations to an **existing**
workspace.

Regeneration is optional. A generated project MUST remain fully usable without
ever regenerating.

#### Preconditions

Regeneration requires:

- a valid workspace metadata record;
- a valid provenance table;
- resolvable versions of the recorded resources, or explicitly requested new
  versions.

If metadata is absent or invalid, the engine MUST refuse to regenerate and MUST
return a structured error. The engine MUST NOT reconstruct provenance by
guessing from filesystem contents.

#### Operations

The update model MUST support:

- **add** — introduce a new kit, package, or asset;
- **remove** — remove a previously integrated kit, package, or asset;
- **update** — move a template, kit, package, asset, or framework version to a
  new version;
- **repair** — restore `generated`-class files to match current provenance.

Each operation is requested explicitly by Lua. The engine MUST NOT perform any
of them as a side effect of another operation.

#### Update sequence

An update MUST follow:

1. Load and validate workspace metadata and provenance.
2. Compute the current workspace state (hash every recorded path).
3. Resolve the requested target resource set.
4. Cross-validate the target set (as in §2.7.9).
5. Produce an **update plan**: paths to add, modify, remove, and paths in
   conflict.
6. Return the plan to Lua for a decision.
7. On approval, open a transaction and apply.
8. Update metadata and provenance.
9. Validate and commit.

Steps 1–6 MUST NOT mutate the workspace. The update plan MUST be obtainable as a
preview without applying it.

#### Preservation guarantees

Regeneration MUST preserve:

- all `user`-class files;
- all `seeded`-class files;
- any `generated`-class file whose hash indicates user modification, unless the
  workflow explicitly authorizes overwrite.

Regeneration MUST NOT delete a path it did not create, as recorded in
provenance.

#### Removal

Removing a kit, package, or asset MUST remove only paths whose provenance
attributes them to that resource **and** whose ownership class is `generated`
**and** whose hash is unmodified.

Paths that are user-modified, `seeded`, or `user` MUST be reported as orphaned
rather than deleted. Lua decides.

#### Version migration

A resource version change MAY require migration beyond file replacement — moved
paths, renamed targets, changed configuration schemas.

Where migration is required, the resource MUST declare migration rules. The
engine MUST apply declared migrations as part of the update transaction.

If a required migration is unavailable, the update MUST fail with a structured
compatibility error rather than proceeding partially.

#### Framework version updates

Updating the Squared framework version MAY require package updates, build
configuration changes, source changes, and metadata changes.

Framework updates MUST be explicitly requested, MUST be validated against every
resolved package and kit, and MUST follow the same plan-and-approve sequence.

#### Transaction requirement

Regeneration MUST occur within a generation transaction ([[2.15 Error and transaction model]]). A failed update
MUST restore the workspace to its pre-update state.

The engine MUST NOT leave a workspace in a state where its metadata describes a
resource set that does not match its files.

---

