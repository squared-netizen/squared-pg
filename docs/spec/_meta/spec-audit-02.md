---
title: Spec audit 02
tags: [spec, meta, audit]
---

# Spec audit 02

```
Audit run:  2026-09-05
Scope:      the delta since spec-audit-01 was applied (2026-09-02)
            — decisions D-029..D-044, and the specification gaps opened by
              the sysroot/tier design proposal
Status:     each finding awaits an APPROVED / REJECTED mark before any edit
Method:     read-only. No spec note has been modified by this pass.
```

Audit 01 audited the vault against itself and against the repository as it
stood. It was applied, and its findings are closed.

This pass audits the *delta*: sixteen decisions were recorded during the first
implementation pass and the Android work, every one of which names a section to
amend, and **none of the amendments has been made**. The specification and the
implementation now disagree in sixteen documented places.

That is a different kind of finding from audit 01's. There is no ambiguity to
resolve and no wording to repair — the decision text already says what the
correct wording is. The work is transcription, and the risk is that it keeps
being deferred until the spec stops being the thing you can read to understand
the system.

## Summary

| Class | Count | Blocking | Serious | Minor |
|---|---|---|---|---|
| K — decision recorded, amendment not applied | 16 | 3 | 9 | 4 |
| L — specification absent for planned work | 5 | 0 | 5 | 0 |
| M — numbering and register integrity | 3 | 1 | 1 | 1 |
| **Total** | **24** | **4** | **11** | **9** |

Severity: blocking = the spec as written contradicts shipped, tested
behaviour; serious = an implementer reading the spec would build the wrong
thing; minor = editorial.

---

## K — Decision recorded, amendment not applied

Each entry names the decision, the sections it says to amend, and what the
amendment is. The decision text in `Spec decisions` is the source; nothing here
is new reasoning.

### AUD-K-001 — §2.8.1–2.8.3 describe a manifest envelope nothing implements
**Status:** awaiting decision · **Severity:** blocking · **D-030**

§2.8.3 defines `schema_version` / `type` / `requires_engine` /
`requires_capabilities`; §2.8.1 defines an identifier grammar with hyphens and
no segment limit. The engine implements neither. It opens every resource
through sqcart, so resources are cartridges and inherit the container format's
envelope (`format_version` / `kind` / `engine` / `requires_features`) and its
identity grammar (`[a-z][a-z0-9_]*`, two to eight segments).

Amendment: rewrite §2.8.1–2.8.3 to state that generator resources are
cartridges. `ResourceId::parse` accepts both `_` and `-` separators, so
identities written against either document validate; say so.

This is the largest single divergence in the vault and the one most likely to
mislead someone authoring a resource from the spec.

### AUD-K-002 — §2.7.11 regeneration is specified; the engine refuses it
**Status:** awaiting decision · **Severity:** blocking · **D-033**

`project.generate` refuses an existing workspace with
`filesystem.workspace.exists`. §2.7.11 reads as though update is available.

Amendment: mark §2.7.11 and §2.15.9's journal as **not implemented in v1**,
with the reason (a half-implemented journal corrupts user work in exactly the
cases the journal exists to protect) and the list of what already exists for it
— provenance hashes, the reified plan, enforced ownership classes.

### AUD-K-003 — §2.14.4's sandboxed trust tier is refused, not enforced
**Status:** awaiting decision · **Severity:** blocking · **Q-26**

The host reads `trust` and *refuses* a workflow declaring `sandboxed` (exit 3)
rather than granting it the full standard library. §2.14.4 describes it as a
working tier.

Amendment: §2.14.4 states the current behaviour and what enforcement requires —
a restricted `_ENV`, a whitelist-backed `require`, and a host-provided I/O
channel, the last of which ripples because `squaredpg.report` writes to
`io.stderr` directly.

### AUD-K-004 — §1.1 omits `app/` and `engine/lua/`
**Status:** awaiting decision · **Severity:** serious · **D-029**

Amendment: §1.1 lists both, with the reasoning — the CLI host is a consumer of
the engine and not part of it; the binding layer is generated from the operation
registry and is therefore engine-side.

### AUD-K-005 — §2.9/§2.10 do not state the payload root convention
**Status:** awaiting decision · **Severity:** serious · **D-031**

Templates declare `template.tree`; kits, packages and asset bundles have no
equivalent field, so the engine's convention is `tree/` when present and the
cartridge root otherwise.

Amendment: state the convention in §2.9 and §2.10, or add a `tree` field to
those bodies in the cartridge format. The second is cleaner and is a change to
a different document.

### AUD-K-006 — §2.7/§2.8/§2.9 do not mention the raw-JSON extension fields
**Status:** awaiting decision · **Severity:** serious · **D-032**

Eight fields the architecture needs are absent from the cartridge format and are
read from `Manifest::raw_json()`: `working_directory`, `integration_areas`,
`integration_arity`, `processor`, parameter `default`/`default_from`,
`executable`, `external.acquisition`, `conflicts_with`.

Amendment: document each, with its body, its default when absent, and the note
that sqcart does not validate them — a malformed one falls back rather than
failing, except `processor`, which is a hard `capability.unsatisfied`.

### AUD-K-007 — §2.8.7 does not say the engine contributes built-in parameters
**Status:** awaiting decision · **Severity:** serious · **D-034, D-038**

The template parameter contract is validated against the *effective* set:
declared defaults, then engine built-ins, then workflow values — in that order,
so `default_from` can reference `project_name`.

Amendment: §2.8.7 lists the built-ins, states the layering order and the
put-if-absent rule, and says validation sees the effective set.

### AUD-K-008 — §2.14.1 has no mechanism for requiring a capability
**Status:** awaiting decision · **Severity:** serious · **D-035**

The registry was enumerable and undemandable until `requires_capabilities` was
added to the cartridge envelope. §2.14.1 requires unsatisfied capabilities to
fail at resolution but never says how one is declared.

Amendment: §2.14.1 documents the field, the `name` / `name@range` token form,
and the distinction from `requires_features` — reader versus consumer, open
versus resolve, newer sqcart versus newer generator.

### AUD-K-009 — §2.8.2 does not admit partial version operands
**Status:** awaiting decision · **Severity:** serious · **D-036**

`^1.0`, `>=0.1` and `~1.2` now parse, padding to three components. A manifest's
own `version` is not padded.

Amendment: §2.8.2 states the padding rule and why it is not optional — callers
write `if (range && range->satisfied_by(v))`, so an unparsed range means the
constraint silently never applies.

### AUD-K-010 — §2.9 does not say who owns an integration point's substrate
**Status:** awaiting decision · **Severity:** serious · **D-037**

NDK detection and native-app-glue belong to `template.android.cpp`, not to
`kit.opengl`, because the template promises to build with no rendering kit.

Amendment: §2.9 states the general rule — an integration point's *substrate*
belongs to whoever cannot be absent — with the concrete case as the example.

### AUD-K-011 — §2.7 does not distinguish build settings from generation parameters
**Status:** awaiting decision · **Severity:** serious · **D-039**

`minSdkVersion`, `targetSdkVersion` and the ABI are build variables, not
template parameters. Baking them at generation would mean regenerating to
retarget — which, with §2.7.11 unimplemented, means losing the workspace.

Amendment: §2.7 states the test — if a value can change over a project's life
without changing what the project *is*, it belongs in the build.

### AUD-K-012 — §2.9 does not warn about reorganising a host toolchain
**Status:** awaiting decision · **Severity:** serious · **D-040**

A kit adds an external sysroot with `-idirafter`, never `-I`, and names
libraries by path rather than by `-l` with `-L`. Both were corrections after
on-device build failures.

Amendment: §2.9 states that a kit contributes *additions* to a host toolchain
and must not reorganise it, with both failure modes as evidence.

### AUD-K-013 — §2.8.1 does not say an identity names the artifact
**Status:** awaiting decision · **Severity:** minor · **D-041**

`template.termux.cpp` became `template.terminal.cpp`; the platform belongs in
`platforms`, not in the identity.

Amendment: §2.8.1 adds the rule. `template.android.cpp` keeps its name because
Android is what it *produces*.

### AUD-K-014 — §2.5.6 does not say a workflow may assume no platform
**Status:** awaiting decision · **Severity:** minor · **D-042**

Amendment: §2.5.6 notes that an empty platform set means no restriction, and
that host detection is the wrong default because the generation host is not
necessarily the target.

### AUD-K-015 — §2.1 does not say how a host locates its installation
**Status:** awaiting decision · **Severity:** minor · **D-043**

By marker — walking up for a directory containing both `lua/workflows` and
`resources`, then `<prefix>/share/squared-pg` — never by directory name.

Amendment: §2.1 or §1.2 states it, with the failure it replaced: recognising
the names `build` and `bin` broke every other build directory.

### AUD-K-016 — §1.7 prefers fish; the tooling is bash
**Status:** awaiting decision · **Severity:** minor · **D-044**

Amendment: §1.7 states bash as canonical, fish as an optional translation, and
the reason — two bugs shipped in a fish script that could not be executed where
it was written.

---

## L — Specification absent for planned work

The sysroot and tier proposal (`docs/developer/design/sysroot-and-tiers.md`)
introduces concepts the vault does not contain. These are not divergences;
they are gaps that must be filled *before* implementation, or the same
after-the-fact-decision pattern repeats.

### AUD-L-001 — the sysroot is not a specified concept
**Status:** awaiting decision · **Severity:** serious

A host environment containing the generator, a sandbox and a project area.
Needs a section stating that it is **configuration, never discovery** — §2.7.2
forbids ambient state, and a sysroot is ambient unless it arrives explicitly.

### AUD-L-002 — workspace tiers are not specified
**Status:** awaiting decision · **Severity:** serious

`sandbox` and `project`, their invariants, and the fact that most of them are
documentation rather than engine-enforceable. §2.7.3 is the natural home.

### AUD-L-003 — promote and demote are not specified
**Status:** awaiting decision · **Severity:** serious

Both mutate an existing workspace, which §2.7.11 does not cover and the engine
refuses. The design proposes a narrower path — rename, additive writes,
provenance-checked replacement — which needs specifying as distinct from
regeneration rather than folded into it.

Demotion is destructive and the spec must say what it may never do: no silent
`.git` deletion, refuse on a dirty tree, archive rather than remove.

### AUD-L-004 — workspace conformance is not specified
**Status:** awaiting decision · **Severity:** serious

`workspace.verify` as an operation distinct from `workspace.inspect`, and the
invariant list it checks. §2.16's register is the natural relative — it already
names a detection mechanism per invariant (D-028), and this is where several of
those mechanisms would live.

### AUD-L-005 — the metadata record version is not versioned in the spec
**Status:** awaiting decision · **Severity:** serious · **Q-24**

A `tier` field takes the record from v1 to v2. §2.7.3 describes the record but
not its evolution. Q-24 (metadata format migration) becomes live the moment a
field is added.

The honest reading of a v1 record under a v2 engine is "tier unknown", not
"tier sandbox"; the spec should say so, because the alternative is a v1
workspace silently acquiring sandbox restrictions it never agreed to.

---

## M — Numbering and register integrity

### AUD-M-001 — D-028 was assigned twice
**Status:** **resolved during this pass** · **Severity:** blocking

Audit 01 added D-028 ("Invariant register names a detection mechanism per
invariant", resolving Q-25). The first implementation pass, unaware of it,
began its own block at D-028.

Sixteen decisions were renumbered D-029..D-044 and every citation updated
across the vault, the developer documentation and the source comments. The
audit's D-028 and its references in §2.16, `Spec status` and `spec-audit-01`
are untouched.

Recorded here rather than fixed silently: a decision register that has
renumbered once needs the fact visible, or a reader following an old citation
lands on the wrong decision.

### AUD-M-002 — `Spec status` does not reflect sixteen new decisions
**Status:** awaiting decision · **Severity:** serious

The status dashboard was accurate when audit 01 was applied. Every §2.7, §2.8,
§2.9 and §2.14 row now has an unapplied amendment against it.

Amendment: add a column, or a note per row, naming the pending decisions. A
dashboard that does not show known pending work is worse than none.

### AUD-M-003 — the open questions register has grown without triage
**Status:** awaiting decision · **Severity:** minor

Q-26 through Q-29 were added during implementation. Q-23 (framework
acquisition) is now partly answered by the framework layering the project
author described, and Q-21 (template composition) has been measured and found
weaker than assumed — one file of twenty is duplicated between the two
templates.

Amendment: update both entries with what is now known, rather than leaving them
as they were first written.

---

## Recommended order

1. **AUD-K-001, K-002, K-003** — the three blocking divergences. The spec
   currently contradicts shipped, tested behaviour in each.
2. **AUD-M-002, M-003** — the registers, so the dashboard is trustworthy before
   more work lands.
3. **AUD-K-004…K-016** — mechanical transcription; the decision text already
   says what to write.
4. **AUD-L-001…L-005** — before any sysroot code, not after.

Class K is not thinking work. It is an afternoon of transcription that has been
deferred for a fortnight, and every deferral makes the vault a slightly less
reliable description of the thing it describes.
