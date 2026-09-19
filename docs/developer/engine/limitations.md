# Limitations

What this build does not do. Everything here is a deliberate omission with a
stated reason, not an oversight — and each is refused with a structured error
rather than approximated.

## Regeneration and update (§2.7.11)

**Not implemented.** `project.generate` refuses an existing workspace with
`filesystem.workspace.exists`.

Needs the write-ahead journal (§2.15.9), per-file staging, crash recovery across
the four journal states, and the `damaged` terminal state (§2.15.13). A
half-implemented journal would corrupt user work in exactly the situations the
journal exists to protect, so refusing is the correct failure.

Already in place: provenance hashes on every generation, a reified plan, and
enforced ownership classes. Missing: the journal and the apply-in-place loop.

## Workspace locking (§2.15.12)

**Not implemented.** No `.squared/lock`.

Only meaningful once a second process could be mutating the same workspace,
which requires regeneration to exist first.

## Package materialization (§2.7.6) — packages yes, assets not yet

**Packages materialize; assets do not.**
`package.materialize` now writes payload through the shared
`detail::payload_prefix()` helper (D-076), so the payload root follows D-031's
exactly the way templates and kits do, with `shared` ownership defaulting to the
glob table for `sq_app/` and `mk/` (D-077). Verified by §6 smoke:
`package.squared-core` lands its full tree plus `mk/pkg_squared_core.mk`, 329
ownership rows in provenance, `override` of a template-owned path works, and an
empty payload warns instead of failing (D-079).

**What is still missing is the journal, not the payload.** The regeneration half
of §2.7.6 — an existing workspace that has *us*, the `shared` absorb/`regenerate`
write-back through a journal — stays refused (`filesystem.workspace.exists`),
exactly as §2.15.9's write-ahead journal would require. That deferral is
recorded in D-077 and this file's [[ROADMAP item 3]] note. `shared` under
materialization is also where the missing `merged`/absorb semantics show up;
ownership hashes (D-077) and diagnostic names carry the load.

**Assets (§2.7.7) not.** The asset service resolves and validates, but does not
write payload into the workspace — there are no assets to write yet.

## Template composition (§2.8.9, Q-21)

**Not implemented.** A template provides its complete workspace foundation
directly, which §2.8.9 explicitly permits.

Two templates now exist, and they do share structure: `sq_app/`, the
Makefile-plus-generated-fragment split, `.gitignore`, `.clang-format`. That is
the first real evidence for composition, and the first real risk of drift —
a fix applied to one template's Makefile does not reach the other. Worth
revisiting once a third template exists, which is when the duplication stops
being tolerable.

## Asset transformation (§2.10.5)

**Not implemented.** Only the identity transformation would apply, and there are
no assets.

## Job wrapper (§2.4.6)

**Not implemented.** Optional in the specification. Synchronous execution is the
normative model and the only consumer today is a CLI that has nothing to do
while it waits.

## Extension services and plugins (§2.14.2)

**Not implemented.** The capability registry exists and is enumerable;
registering *additional* tokens from an extension does not. No plugin exists to
register any.

## Sandboxed workflow trust tier (§2.14.4)

**Declared, not enforced — and therefore refused.** Workflows declare `trust` in
`workflow.json`. The host reads it and, for `sandboxed`, **refuses to run the
workflow at all** (exit 3) rather than granting it the full standard library.

Enforcement needs three things: a restricted `_ENV` (base library minus `load`,
`loadfile` and `dofile`; no `debug`; `os` reduced to the clock functions; no
`io`; no `package`), a whitelist-backed replacement for `require` since
`package.path` is directory traversal with extra steps, and a host-provided I/O
channel — which ripples, because `squaredpg.report` writes to `io.stderr`
directly and could not run sandboxed as written.

Refusing is the honest interim state. A tier that is declared but silently
unenforced is worse than no tier at all: it invites exactly the assumption it
fails to justify. Tracked as Q-26.

## Manifest schema divergence

The engine reads the *cartridge* manifest schema, not §2.8.3's envelope. See
[resources/manifests.md](../resources/manifests.md). Recorded as D-030.
