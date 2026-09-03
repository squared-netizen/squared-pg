---
section: "2"
title: Generator architecture
status: review
tags: [spec, architecture, moc]
---

# 2. Generator architecture

> [!info] Navigation
> Index: [[Specification Index]] · Prev item: [[1 Repository conventions]]

## Purpose

Defines the runtime architecture of `squared-pg`: a native C++20 engine providing
capabilities, a Lua 5.4 control layer providing orchestration, and the generation
model that turns configuration into a user-owned workspace.

The governing idea: **the engine provides capability, Lua provides
orchestration.** Every section below is a consequence of that split.

## Sections

### Architecture core
- [[2.1 Architectural model]]
- [[2.2 Engine responsibility and role]]
- [[2.3 Generation pipeline]]
- [[2.4 Engine lifecycle]]
- [[2.5 Lua control layer]]
- [[2.6 Engine-Lua boundary]]

### Generation model
- [[2.7 Project generation model]]

### Resource definitions
- [[2.8 Templates]]
- [[2.9 Kits]]
- [[2.10 Assets and generator resources]]
- [[2.11 Packages]]

### Boundaries and cross-cutting concerns
- [[2.12 Generated-project boundary]]
- [[2.13 Dependency graph]]
- [[2.14 Extensibility model]]
- [[2.15 Error and transaction model]]
- [[2.16 Architectural invariants]]

## Division of labour

**§2.8–2.11** define what a resource *is* — manifest, identity, versioning,
discovery, validation. **§2.7** defines how resources are *composed into a
project*. **§2.12** states the boundary guarantee; §2.7.10 owns the mechanism
behind it. (D-006, D-006a)

**§2.3** is the conceptual, workflow-facing pipeline. **§2.7.9** is the normative
engine phase order. (D-027)

## Related

- [[Spec decisions]] · [[Spec open questions]] · [[Spec glossary]]
