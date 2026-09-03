---
title: Spec status
tags: [spec, meta, dashboard]
---

# Spec status

Back to [[Specification Index]].

## Item 1 — Repository conventions

| Section | Status |
|---|---|
| [[1.1 Repository identity]] | `final` |
| [[1.2 Root files]] | `final` |
| [[1.3 Engine]] | `final` |
| [[1.4 Lua]] | `final` |
| [[1.5 Resources]] | `draft` | tree not yet provisioned; see note |
| [[1.6 Third-party dependencies]] | `final` |
| [[1.7 Tools]] | `final` |
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
| [[2.7 Project generation model]] | `review` | |
| [[2.8 Templates]] | `review` | Q-21 composition scope |
| [[2.9 Kits]] | `review` | Q-22 build emission |
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
