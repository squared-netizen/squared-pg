---
title: Repository AGENTS.md (adopted)
tags: [spec, meta, tooling]
---

# Repository `AGENTS.md` — adopted

Back to [[Specification Index]].

The content once proposed here was **adopted verbatim** and is the live file at
`AGENTS.md` in the repository root. This note is kept as the historical record of
what changed relative to the earlier file and why; the canonical text is the
root `AGENTS.md`, not this note.

## What changed from the earlier file

- **C++20 exclusively.** The earlier file said "C++17 or C++20 (per-project,
  check `CMakeLists.txt`)". `squared-pg` is one project with one standard;
  ambiguity here produces inconsistent code.
- **The specification is named as a guideline.** Agents previously had no pointer
  to the spec vault. The adopted file describes the spec as a strong guideline
  that is weakly enforced during the initial implementation — the repository is
  the ground truth, and the spec is amended to match (marked temporary; see the
  note in the adopted file). Architecture questions still have answers; they
  should be looked up, not re-derived.
- **Offline is stated as a hard constraint.** It is the project's defining
  property and was absent.
- **Deliverable format** (zip + fish installer) is stated.
- **Type aliasing rules** are included; they were project policy but undocumented.
- **Structure preserved.** The six-step workflow, tool preferences, and
  completion report are good and were kept largely intact.

The full, current text lives at `AGENTS.md` in the repository root.
