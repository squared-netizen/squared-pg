---
title: Spec status
tags: [spec, meta, dashboard]
---

# Spec status

Back to [[Specification Index]].

## Item 1 — Repository conventions

| Section | Status |
|---|---|
| [[1.1 Repository identity]] | `final` | rewritten D-053; restructure-by-role is a todo milestone |
| [[1.2 Root files]] | `final` |
| [[1.3 Engine]] | `final` |
| [[1.4 Lua]] | `final` |
| [[1.5 Resources]] | `review` | tree provisioned; note corrected D-053 |
| [[1.6 Third-party dependencies]] | `final` |
| [[1.7 Tools]] | `final` | bash-only, D-044 as amended |
| [[1.8 Documentation]] | `final` |
| [[1.9 Generated files]] | `final` |
| [[1.10 Naming]] | `final` |
| [[1.11 Dependency direction]] | `final` |
| [[1.12 Development instructions]] | `final` |
| [[1.13 Repository invariant]] | `final` |

## Item 2 — Generator architecture

| Section | Status | Notes |
|---|---|---|
| [[2.1 Architectural model]] | `review` | |
| [[2.2 Engine responsibility and role]] | `review` | |
| [[2.3 Generation pipeline]] | `review` | conceptual; defers order to §2.7.9 |
| [[2.4 Engine lifecycle]] | `review` | de-duplicated, state matrix added |
| [[2.5 Lua control layer]] | `review` | |
| [[2.6 Engine-Lua boundary]] | `review` | |
| [[2.7 Project generation model]] | `review` | +2.7.3a environment/tiers, 2.7.3b record versioning, 2.7.10a build settings; 2.7.11 stubbed (D-033, D-051, D-054) |
| [[2.8 Templates]] | `review` | consumer schema normative (D-053); Q-21 composition scope still open |
| [[2.9 Kits]] | `review` | consumer schema normative, six field names corrected (D-053); Q-22 build emission still open |
| [[2.10 Assets and generator resources]] | `review` | |
| [[2.11 Packages]] | `review` | Q-22 |
| [[2.12 Generated-project boundary]] | `review` | guarantee only, per D-006a |
| [[2.13 Dependency graph]] | `review` | unblocks §1.11 |
| [[2.14 Extensibility model]] | `review` | |
| [[2.15 Error and transaction model]] | `review` | Q-24 metadata migration |
| [[2.16 Architectural invariants]] | `review` | Q-25 resolved via D-028; I-10 gap in §2.16.3 |

## Counts

- `final` — 12
- `draft` — 1
- `review` — 17
- `stub` — 0
- Open questions — 4 (Q-21 … Q-24)

## What this specification does not yet cover

- The reference template set is named but not authored.
- The Squared framework's own architecture is out of scope; only its integration
  contract is specified here.
- `docs/programmer/` and `docs/developer/` have no pages yet. Both are required
  by [[Repository AGENTS]] and must be updated in the same pass as the code they
  describe.

## Audit state

| Audit | Findings | Applied | Open |
|---|---|---|---|
| [[spec-audit-01]] | 14 | 14 | 0 |
| [[spec-audit-02]] | 23 | 23 | 0 |

audit-02 closed 2026-09-07 (D-053, D-054). Two findings were closed as
**obsolete** rather than applied — K-006 and K-008 both described a cartridge
format that had been superseded — and three amendments were found to be wrong
when checked against the code.

**The dashboard's own lesson.** AUD-M-002 filed this page as untrustworthy
because it showed no pending work while sixteen decisions sat unapplied. The
fix is not the rows above; it is the cadence rule now in
[[Spec conventions]]: **no new audit is opened until the previous one is
closed.** A register that accumulates faster than it drains stops being a
register and becomes a second backlog.

## Known gaps, carried forward

These are not findings. They are things known to be missing, listed here so
the dashboard shows pending work rather than hiding it.

| Gap | Where | Blocked on |
|---|---|---|
| Resource ownership checked in isolation | §2.16.5 | nothing; `sqcart` did this until format 2.0 removed its jurisdiction |
| Untracked-file count in `workspace.verify` | §2.16.5 | directory listing on `FilesystemService` |
| Regeneration / update path | §2.7.11 | write-ahead journal (§2.15.9) |
| Metadata migration forward | §2.7.3b | Q-24; reading is settled, rewriting is not |
| `runtime.model` has no consumer | §2.9.2 | `kit.sfml` |
| No tests for `lua/`, `app/`, `engine/lua/` | — | nothing; three destructive commands are untested |
