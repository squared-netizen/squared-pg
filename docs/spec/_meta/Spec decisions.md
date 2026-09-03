---
title: Spec decisions
tags: [spec, meta]
---

# Spec decision log

Back to [[Specification Index]]. Open items live in [[Spec open questions]].

Newest first. Every decision records the alternatives rejected.

---

## D-028 — Invariant register names a detection mechanism per invariant
**Q-25** · [[2.16 Architectural invariants]]

The invariant register (§2.16.2) gives every invariant a detection mechanism, so
an invariant without a test is a documented gap rather than a silent assumption.
The one known weak spot — I-10 "no undocumented global state" — is only partially
checkable mechanically; the shortfall is recorded in §2.16.3 as an open limitation
until a suitable static check is adopted.

Rejected: keeping I-10 aspirational with no stated detection mechanism (an
untestable guarantee is documentation, not a guarantee).

## D-020 — `merged` ownership class is reserved, not implemented in v1
**Q-20** · [[2.7 Project generation model]]

In-place region merging is where generators corrupt user work. v1 supports
`generated`, `seeded`, `user` and `metadata` only. Templates needing a file the
user edits *and* the generator maintains MUST emit a `generated` fragment
`include`d by a `seeded` file.

Rejected: marker-comment regions (fragile across formatters); structured-key
merging (viable later, needs a format-aware writer per build system).

## D-019 — No coroutines in v1
**Q-19** · [[2.4 Engine lifecycle]]

The execution model is synchronous. Async asset loading was the only candidate
and does not justify the complexity on the Termux target. Revisit only with a
measured need.

## D-018 — Generator-internal resources get their own namespace
**Q-18** · [[1.5 Resources]] · [[2.10 Assets and generator resources]]

`resources/generator/` holds resources `squared-pg` consumes for itself.
`resources/assets/` remains project-bound application assets only.

Rejected: dumping them at `resources/` root (indistinguishable from misc data);
a `_internal` prefix inside `assets/` (violates namespace distinctness).

## D-017 — Errors carry a coarse category and a stable specific code
**Q-17** · [[2.15 Error and transaction model]]

Two fields. `category` is a closed enum for control flow; `code` is a stable
dotted string for diagnosis and documentation, e.g.
`category: package`, `code: package.version.unsatisfiable`.

Rejected: single flat enum (either too coarse to diagnose or too large to switch
on).

## D-016 — Two-tier Lua trust model
**Q-16** · [[2.14 Extensibility model]]

Workflows are `trusted` (repository `lua/`, user config directory) or
`sandboxed` (anything else). Sandboxed workflows run without `os.execute`,
`package.loadlib`, `debug`, and raw `io` writes; they reach the filesystem only
through engine services.

Rejected: no sandbox (unsafe once workflows are shareable); sandbox everything
(breaks the CLI's own workflows).

## D-015 — Engine instances are single-threaded and workspace-exclusive
**Q-15** · [[2.4 Engine lifecycle]] · [[2.15 Error and transaction model]]

An engine instance has thread affinity to its creating thread and is not
thread-safe. Multiple instances MAY coexist in a process. One `lua_State` per
engine. One writer per workspace, enforced by an advisory lock. The job wrapper
MAY use a worker thread but only one operation executes per engine at a time.

Rejected: internally locked thread-safe engine (cost without a use case on the
target platform).

## D-014 — Engine domain knowledge is MUST, not MAY
**Q-14** · [[2.2 Engine responsibility and role]]

The contradicting MAY is deleted.

## D-013 — Lua filesystem prohibition scoped to generator-owned resources
**Q-13** · [[2.5 Lua control layer]] · [[2.6 Engine-Lua boundary]]

Lua MAY read user-space input (config files, streams, stdin). Lua MUST NOT touch
generator resources or mutate the workspace except through engine services.

## D-012 — Validation split: envelope vs type block
**Q-12** · [[2.4 Engine lifecycle]]

Resource Service validates manifest envelope well-formedness, identity syntax,
and integrity. The owning typed service validates the type-specific block.

## D-011 — Immutable resource index
**Q-11** · [[2.4 Engine lifecycle]]

Initialization builds an immutable identity→manifest index. Lazy loading may
only fetch payloads for already-indexed identities. The index MUST NOT change
during a session. This is what makes hybrid eager/lazy loading deterministic.

## D-010 — Transaction references corrected
**Q-10** · [[2.4 Engine lifecycle]]

Transactions are [[2.15 Error and transaction model]]. §2.14 is extensibility.

## D-009 — Typed operation registry is canonical
**Q-09** · [[2.4 Engine lifecycle]]

A compiled, typed C++ operation registry is the core dispatch. The Lua table
form `engine.execute{operation=...}` is binding sugar over it. Preserves
compile-time checking inside the engine.

Rejected: stringly-typed core (loses type checking on the engine's own internals).

## D-008 — Operations are documented compositions of services
**Q-08** · [[2.4 Engine lifecycle]]

No capability may exist only at the operation layer. Every operation is
expressible as a documented sequence of service calls, which keeps the service
layer the single source of truth and makes operations testable as compositions.

## D-007 — Synchronous core execution, optional job wrapper
**Q-07** · [[2.4 Engine lifecycle]]

The duplicated §2.4.6 is unified. The synchronous model is normative. The job
wrapper is optional, MUST NOT change the operation contract, and MUST produce
identical behaviour.

## D-006 — Resource *definition* split from resource *composition*
[[2.7 Project generation model]] · [[2.8 Templates]] · [[2.9 Kits]] · [[2.10 Assets and generator resources]] · [[2.11 Packages]]

§2.8–2.11 define what a resource *is* — manifest, identity, versioning,
discovery, validation. §2.7 defines how resources are *composed into a project*.

## D-006a — §2.12 states the guarantee, §2.7 owns the mechanism
[[2.12 Generated-project boundary]] · [[2.7 Project generation model]]

Ownership classes and provenance stay in §2.7.10. §2.12 becomes the short,
checkable statement of the boundary guarantee: what a generated project may
never contain and may never require.

## D-005 — Ownership classes and file provenance
[[2.7 Project generation model]]

Five classes plus a provenance table recording path, class, originating resource
and version, and content hash. Promotes workspace metadata from SHOULD to MUST.

Rejected: timestamp comparison (unreliable across VCS and copies); never
overwriting anything (makes updates impossible).

## D-004 — Undeclared ownership class defaults to `seeded`
[[2.7 Project generation model]]

The safe default is never to overwrite.

## D-003 — Generation plan is reified and shared with execution
[[2.7 Project generation model]]

Plan and apply consume the same command list, so they cannot diverge.

## D-002 — Determinism split into resolution vs output
[[Spec conventions]]

## D-001 — Single declared user working directory
[[2.7 Project generation model]] · [[2.8 Templates]]

Reference convention `sq_app/`. No `generated`-class file may be written into it.
Derived directly from the project vision.

---

## Foundational decisions recorded during the completion pass

## D-021 — Manifests are JSON
**Q-03** · [[2.8 Templates]] and siblings

`manifest.json` at each resource root, parsed with the vendored yyjson. JSON is
already a build dependency, is offline, has an unambiguous grammar, and needs no
new vendored parser.

Rejected: Lua tables as manifests (executable data, defeats sandboxing and static
validation); TOML/YAML (new vendored dependency for no gain).

## D-022 — Identifier and version grammar
**Q-04** · [[2.8 Templates]] · [[2.11 Packages]]

Identifiers are `<type>.<segment>(.<segment>)*`, segments matching
`[a-z][a-z0-9]*(-[a-z0-9]+)*`. Versions are SemVer 2.0.0. Reference syntax is
`identity@constraint`. Selection takes the highest satisfying version.

## D-023 — Engine capability tokens
**Q-05** · [[2.14 Extensibility model]]

Named, SemVer-versioned, compiled-in, enumerable. Manifests declare
`requires_capabilities`. Unsatisfied requirements fail at resolution, never at
materialization.

## D-024 — Same-filesystem staging with a write-ahead journal
**Q-06** · [[2.15 Error and transaction model]]

Staging always occurs on the target filesystem, so cross-device rename cannot
arise. New projects build in a sibling temp directory and land with one rename.
Regeneration stages per file under `.squared/staging/` and applies through a
journal that is written and flushed before the apply phase begins. An interrupted
transaction is detected and resolved on next open.

Rejected: copy-then-delete (not atomic); `/tmp` staging (cross-device on Android).

## D-025 — The C++ public API is the boundary; Lua is its first consumer
**Q-01** · [[2.1 Architectural model]] · [[2.5 Lua control layer]]

Embedders MAY call the engine C++ API directly or host the Lua layer. The rule
formerly in §2.5.4 is restated as: the CLI MUST NOT implement generation policy
in native code.

## D-026 — Authoritative dependency direction
**Q-02** · [[2.13 Dependency graph]]

`third-party/` ← `engine/` ← `lua/` ← hosts. `resources/` is inert data with no
outgoing dependencies, reached only through the Resource Service. Generated
projects are sinks. Unblocks [[1.11 Dependency direction]]. (Directory naming
follows the repository's current `third-party/`; see [[1.6 Third-party dependencies]].)

## D-027 — §2.3 is conceptual, §2.7.9 sets the reference phase order
[[2.3 Generation pipeline]] · [[2.7 Project generation model]]

§2.3 describes the workflow-facing pipeline. §2.7.9 sets the *reference* phase
order, which a workflow SHOULD follow (Lua may legitimately reorder within the
resolve-before-mutate and transaction constraints). §2.3 links rather than
restates.

---

## Decisions recorded during the first implementation pass

> [!note] Stance
> These record where the implementation diverged from Item 2 as written. Per the
> repository `AGENTS.md`, the repository is ground truth during initial
> implementation and the specification is amended to match. Each entry names the
> sections that need amending.

## D-028 — The CLI host lives at `app/`; the Lua binding at `engine/lua/`
[[1.1 Repository identity]] · [[2.1 Architectural model]] · [[2.6 Engine-Lua boundary]]

§1.1 divides the repository into `engine/`, `lua/`, `third-party/`, `tools/` and
`docs/`, which leaves the CLI host and the binding layer without a home.

- **`app/`** — the `sqpg` host. It is a *consumer* of the engine (§2.1.6), not
  part of it; placing it under `engine/` would blur the line §2.2.5 draws.
- **`engine/lua/`** — the binding layer. §2.6.3 requires it to be generated from
  or validated against the operation registry, which makes it engine-side. It
  builds as a separate target so an embedder may link the engine alone and never
  pay for a Lua runtime.

Rejected: putting the binding in `lua/`, which is Lua source by §1.4 and would
have put C++ there; putting the CLI in `engine/`, which §2.2.5 forbids in spirit.

Amend §1.1 to list both.

## D-029 — Resources are cartridges; the manifest envelope is the cartridge format's
[[2.8 Templates]] · [[2.10 Assets and generator resources]]

§2.8.3 defines an envelope (`schema_version`, `type`, `requires_engine`,
`requires_capabilities`) and §2.8.1 an identifier grammar
(`[a-z][a-z0-9]*(-[a-z0-9]+)*`, unbounded segments). The repository already
contains manifests in the *cartridge* format (`format_version`, `kind`,
`engine`, `requires_features`; segments `[a-z][a-z0-9_]*`, two to eight), a
tested reader for them, and a CLI that writes them.

The engine implements the cartridge form and opens every resource through
`sqcart`. `ResourceId::parse` accepts both `_` and `-` as segment separators so
that identities written against either document validate.

Rationale: a second manifest reader would be a second chance to disagree about
what a manifest means, and disagreement between the tool that packs cartridges
and the tool that consumes them has the worst possible symptoms.

Rejected: implementing §2.8.3 alongside the cartridge envelope (two readers);
rewriting sqcart to §2.8.3 (discards a tested, shipped implementation).

Amend §2.8.1–2.8.3 to state that generator resources are cartridges and inherit
the container format's envelope and identity grammar.

## D-030 — Resource payload root
[[2.8 Templates]] · [[2.9 Kits]] · [[2.10 Assets and generator resources]]

Templates declare their payload root as `template.tree`. The cartridge format
gives kits, packages and asset bundles no equivalent field, so the engine's
convention is: `tree/` when the cartridge has entries beneath it, the cartridge
root otherwise. `SQ-INF/` is never payload.

Amend §2.9 and §2.10 to state the convention, or add a `tree` field to those
bodies in the format.

## D-031 — Architecture fields absent from the cartridge format are read from raw JSON
[[2.7 Project generation model]] · [[2.8 Templates]] · [[2.9 Kits]]

The generator architecture needs manifest fields the container format does not
model: `working_directory`, `integration_areas`, `integration_arity`,
`processor`, parameter `default`/`default_from`, `executable`,
`external.acquisition`, `conflicts_with`.

These are read from `Manifest::raw_json()`, which format §5.4 guarantees is
preserved verbatim. This keeps the divergence one-directional: the engine knows
more than the format, and the format never learns about generation — which is
the scope rule `sqcart/AGENTS.md` already states.

Where such a field is absent or malformed the engine falls back to a documented
conservative default (`working_directory` → `sq_app`, `processor` →
`substitute`, arity → `single`). An *unknown* processor is a hard
`capability.unsatisfied` failure, because §2.14.1 requires an unsatisfied
capability to fail at resolution.

## D-032 — v1 implements new-workspace generation only
[[2.7 Project generation model]] · [[2.15 Error and transaction model]]

`project.generate` refuses an existing workspace with
`filesystem.workspace.exists`. Regeneration (§2.7.11) requires the write-ahead
journal (§2.15.9), per-file staging, crash recovery across the four journal
states, and the `damaged` terminal state (§2.15.13).

Refusing loudly is the correct failure: a half-implemented journal would corrupt
user work in exactly the situations the journal exists to protect.

Already in place for it: provenance hashes written on every generation, a
reified plan (the same type an update plan needs), and enforced ownership
classes. Missing: the journal and the apply-in-place loop.

Package and asset *materialization* are deferred on the same principle —
resolution is implemented and reported, materialization is not, and there is no
package or asset in the repository to design it against.

## D-033 — The engine supplies built-in substitution parameters
[[2.7 Project generation model]] · [[2.8 Templates]]

The template parameter contract is validated against the **effective**
parameters: the template's declared defaults, then engine built-ins derived from
the request, then the workflow's values.

Built-ins: `project_name`, `namespace`, `working_directory`,
`generator_version`, `template_id`, `template_version`, `framework_version`,
`kit_list`. Every one derives from the request or the engine — never from the
environment or the clock, because §2.7.13 requires byte-identical output from
equivalent inputs.

Without this, every workflow would have to restate `project_name = name` to pass
a check the engine was about to satisfy itself. §2.8.7 should say that the
engine contributes built-ins and that validation sees the effective set.
