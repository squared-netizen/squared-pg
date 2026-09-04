---
title: Spec open questions
tags: [spec, meta, dashboard]
---

# Spec open questions

Back to [[Specification Index]]. Resolutions are recorded in [[Spec decisions]].

## Resolved

All twenty questions raised during the first review pass have preliminary
answers, written into the sections and recorded as decisions. Q-25, raised while
writing the resolutions, is now also resolved (D-028). Preliminary means
*decided and specified*, not *unchallengeable* — each is reopenable with a new
decision entry.

| # | Question | Resolution | Decision |
|---|---|---|---|
| Q-01 | Embedder access contradiction | C++ API is the boundary; Lua is first consumer | D-025 |
| Q-02 | Dependency direction | `third_party` ← `engine` ← `lua` ← hosts; `resources` inert | D-026 |
| Q-03 | Manifest schema | JSON `manifest.json`, common envelope + type block | D-021 |
| Q-04 | Identifier and version grammar | dotted lowercase segments; SemVer 2.0.0; `id@constraint` | D-022 |
| Q-05 | Engine capability registry | named, versioned, compiled-in, enumerable | D-023 |
| Q-06 | Transaction mechanics | same-filesystem staging + write-ahead journal | D-024 |
| Q-07 | Duplicate §2.4.6 | synchronous core normative; job wrapper optional | D-007 |
| Q-08 | Service vs operation duplication | operations are documented service compositions | D-008 |
| Q-09 | Stringly-typed dispatch | typed C++ registry canonical, Lua form is sugar | D-009 |
| Q-10 | Broken §2.14 references | corrected to §2.15 | D-010 |
| Q-11 | Hybrid eager/lazy determinism | immutable resource index | D-011 |
| Q-12 | Resource Service validation boundary | envelope vs type block | D-012 |
| Q-13 | Lua filesystem prohibition too broad | scoped to generator-owned resources | D-013 |
| Q-14 | Engine domain MAY/MUST contradiction | MAY deleted | D-014 |
| Q-15 | Threading and concurrency | single-threaded, workspace-exclusive | D-015 |
| Q-16 | Lua trust boundary | two-tier trusted/sandboxed | D-016 |
| Q-17 | Error identifiers | category enum + stable dotted code | D-017 |
| Q-18 | Generator-internal asset home | `resources/generator/` | D-018 |
| Q-19 | Coroutines | not in v1 | D-019 |
| Q-20 | Merged-file regions | `merged` reserved, not implemented in v1 | D-020 |
| Q-25 | Invariant testability | invariant register names a detection mechanism per invariant; I-10 partially checkable, gap in §2.16.3 | D-028 |

## Open

These arose while writing the resolutions and are deliberately left open.

### Q-21 · Template composition in v1?
[[2.8 Templates]]

Composition is specified but optional. The four reference templates
(`headless`, `terminal`, `ncurses`, `android.cpp`) share substantial structure,
which argues for composition; but a v1 that requires each template to be
self-contained is simpler and cannot produce composition-order bugs. Decide when
the reference templates are authored.

### Q-22 · Build-system abstraction depth
[[2.11 Packages]] · [[2.7 Project generation model]]

Packages declare build integration as structured data, which the generator turns
into CMake. Android additionally needs Gradle. Is there one build-integration
model with per-system emitters, or does each kit own its build emission? Current
text assumes emitters; confirm when `kit.android` is authored.

### Q-23 · Squared framework acquisition
[[2.7 Project generation model]]

The spec requires framework integration to be reproducible and offline, but does
not say where the framework itself comes from — vendored in `resources/packages/`,
a sibling checkout, or a declared external path. Needs deciding before the first
end-to-end generation.

### Q-24 · Workspace metadata format stability
[[2.7 Project generation model]] · [[2.15 Error and transaction model]]

`.squared/` is versioned, but there is no migration story for the metadata format
itself. What happens when a newer generator opens an older workspace record?
Proposal: forward-compatible reader, refuse-and-report on major mismatch.

---

## Raised by the first implementation pass

- **Q-26** — the `sandboxed` trust tier (§2.14.4) is declared in `workflow.json`
  but not enforced: the host does not yet construct a restricted Lua
  environment. A tier that is declared but unenforced is worse than none,
  because it invites the assumption that it works. Close this before
  third-party workflows are a real thing.
- **Q-27** — §2.8.1's identifier grammar and cartridge format §5.2's disagree on
  separators and segment counts (D-029). The engine accepts both. Decide which
  is normative and amend the other.
- **Q-28** — the reference workflow defaults `platforms` to `["termux"]` and
  `framework` to `"1.0.0"`. Both are workflow policy under §2.5.6, but the
  framework default in particular is a placeholder standing in for Q-23 (where
  the Squared framework comes from), and it should not outlive it.
