---
title: squared-pg Functional Specification
status: living
tags: [spec, index, moc]
---

# squared-pg — Functional Specification

The canonical outline. Every section note links back here.

> [!abstract] What squared-pg is
> An offline-first project generator for applications built on the Squared
> framework. A native C++20 engine provides generation capabilities; a Lua 5.4
> control layer sequences them into workflows. The engine behaves like a
> programmable pipeline — fixed capability, programmable sequencing. The output is
> a self-contained developer workspace the user owns outright.

> [!tip] How to use this vault
> Each numbered section is one note carrying an `## Outline` block listing what
> that section defines. The outline is retained after writing as a completeness
> checklist. See [[Spec conventions]].

---

## Item 1 — Repository conventions

The shape of the repository itself.

- [[1.1 Repository identity]]
- [[1.2 Root files]]
- [[1.3 Engine]]
- [[1.4 Lua]]
- [[1.5 Resources]]
- [[1.6 Third-party dependencies]]
- [[1.7 Tools]]
- [[1.8 Documentation]]
- [[1.9 Generated files]]
- [[1.10 Naming]]
- [[1.11 Dependency direction]]
- [[1.12 Development instructions]]
- [[1.13 Repository invariant]]

Item MOC: [[1 Repository conventions]]

---

## Item 2 — Generator architecture

The runtime architecture and the generation model.

### Architecture core
- [[2.1 Architectural model]] — capability vs orchestration; who may call the engine
- [[2.2 Engine responsibility and role]] — what the engine does and may not do
- [[2.3 Generation pipeline]] — the conceptual, workflow-facing stages
- [[2.4 Engine lifecycle]] — creation, services, operations, results, shutdown
- [[2.5 Lua control layer]] — orchestration, input, workflows
- [[2.6 Engine-Lua boundary]] — the C++/Lua contract

### Generation model
- [[2.7 Project generation model]] — workspace, composition, ownership, regeneration

### Resource definitions
*What a resource is.* Composition into a project is §2.7.

- [[2.8 Templates]] — plus the manifest envelope and identity grammar shared by all four
- [[2.9 Kits]]
- [[2.10 Assets and generator resources]]
- [[2.11 Packages]]

### Boundaries and cross-cutting concerns
- [[2.12 Generated-project boundary]] — the guarantee made to every generated project
- [[2.13 Dependency graph]] — the authoritative graph
- [[2.14 Extensibility model]] — capabilities, extensions, workflow trust
- [[2.15 Error and transaction model]] — errors, journaling, crash recovery
- [[2.16 Architectural invariants]] — what must stay true, and how it is detected

Item MOC: [[2 Generator architecture]]

---

## Meta

- [[Spec conventions]] — normative language, formatting, status values
- [[Spec glossary]] — fixed-meaning terms
- [[Spec decisions]] — decision log, with rejected alternatives
- [[Spec open questions]] — resolved register plus what remains open
- [[Spec status]] — progress dashboard
- [[Repository AGENTS]] — the repository `AGENTS.md` (adopted; history and rationale)

---

## Reading paths

**New to the project** — [[2.1 Architectural model]] → [[2.7 Project generation model]] → [[2.12 Generated-project boundary]].

**Implementing the engine** — [[2.4 Engine lifecycle]] → [[2.15 Error and transaction model]] → [[2.6 Engine-Lua boundary]].

**Authoring a resource** — [[2.8 Templates]] §2.8.1–2.8.4 for the shared manifest rules, then the section for your resource type.

**Reviewing a change** — [[2.16 Architectural invariants]].

---

## State of the specification

All sections are written. Item 1 is `final`; Item 2 is `review`.

Twenty questions raised in the first review pass have preliminary answers,
recorded in [[Spec decisions]]. Five questions remain open — Q-21 through Q-25 in
[[Spec open questions]] — and each is deliberately deferred to the point where
implementation will decide it better than speculation can.

Not covered here: the reference templates are named but not authored, the Squared
framework's own architecture is out of scope, and `docs/programmer/` and
`docs/developer/` have no pages yet.
