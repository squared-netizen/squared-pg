# Resources

Discovery, indexing and resolution. Programmer counterpart:
[manifest reference](../../programmer/resources/manifests.md).
Specification: §2.4.4, §2.8.4.

## Discovery

`ResourceService::build_index` walks each declared root, recursing into
directories until it finds either a `.sq` file or a directory containing
`SQ-INF/manifest.json`.

Recursion stops at a resource directory. A template's own subdirectories are not
candidates — otherwise a template shipping an example project would index it.

Entries within a root are **sorted** before processing. Filesystem enumeration
order is not stable across hosts, and §2.8.4 requires duplicate-identity
conflicts to be reported consistently rather than depending on which of two
paths the directory iterator happened to yield first.

## Skip or fail

| Situation | Response |
|---|---|
| unreadable cartridge | warn, skip |
| unknown kind | warn, skip |
| identity violates the grammar | warn, skip |
| version is not SemVer | warn, skip |
| requires an engine version this build is not | warn, index anyway |
| **duplicate (id, version)** | **fail initialization** |

The asymmetry is from §2.8.1. A broken third-party kit is that author's problem
and must not brick the generator. Two resources claiming one identity is
ambiguity, and silently picking one would mean the same request generates
different projects on different machines.

A resource requiring a newer engine is indexed rather than hidden, because
§2.14.1 wants the failure at *resolution*, where the error can name the resource
and its requirement. Hiding it would produce `template.not_found`, which is a
different and misleading claim.

## The index

Immutable for the session (D-011). Selection follows §2.8.2: collect every
version of the identity, discard those failing the constraint, sort descending,
take the first. A pre-release is only selectable when a clause explicitly named
one at the same `(major, minor, patch)` — without that rule, `>=1.0.0` would
happily pick `2.0.0-alpha.1` over `1.9.0`.

## Payload opening

Lazy and cached. `verify_integrity` is off at index time and **on** when a
resource is actually opened for use, so the cost of hashing every entry is paid
only for the resources a run touches (§2.10.7).

`ResolvedResource` holds a `shared_ptr<sqcart::Cartridge>` alongside pointers
into its manifest. The manifest is owned by the cartridge, so keeping the
pointer without keeping the cartridge would dangle — and the type system would
not catch it.

## Payload root (D-030)

Templates declare `template.tree`. The cartridge format gives kits, packages and
asset bundles no equivalent field, so the convention is: `tree/` when the
cartridge has entries under it, the cartridge root otherwise. `SQ-INF/` is never
payload.

## What Lua may see

`ResourceRecord::to_public_value` returns identity, kind, version, title and
description. **Not the location.** §2.5.9 keeps Lua independent of repository
layout, and a path a workflow can see is a path it will eventually depend on.
