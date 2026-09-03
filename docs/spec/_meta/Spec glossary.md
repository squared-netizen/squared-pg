---
title: Spec glossary
tags: [spec, meta]
---

# Spec glossary

Back to [[Specification Index]].

Terms with fixed meaning. Drift between these is a defect.

## Project and workspace

| Term | Meaning |
|---|---|
| **Workspace** | The complete generated directory tree. Synonymous with *generated project*. |
| **Generated project** | Same as workspace. Preferred when contrasting with the generator. |
| **Working directory** | The subtree the user authors application code in. Reference convention `sq_app/`. |
| **`SquaredProject`** | The engine's in-memory project model (a C++ type). |
| **Squared project** | The generated artifact, not the model. |

## Resources

| Term | Meaning |
|---|---|
| **Resource** | Any generator-owned, manifest-identified unit: template, kit, package, or asset. |
| **Template** | Defines the project foundation — *what kind of project*. |
| **Kit** | Bridges Squared to an external framework or platform. |
| **Package** | Reusable Squared framework functionality. |
| **Asset** | Data consumed by the generated application. |
| **Manifest** | `manifest.json` at a resource root; gives the resource its identity and metadata. |
| **Resource identity** | The manifest-declared identifier. Never a filesystem path. |
| **Resource index** | The immutable identity→manifest map built during engine initialization. |

## Engine

| Term | Meaning |
|---|---|
| **Capability** | Something the engine can do. Provided, never decided. |
| **Orchestration** | Deciding which capabilities run, when, in what order. Lua's job. |
| **Service** | A registered, persistent engine capability provider. |
| **Operation** | A named, typed, coordinated action composed from services. |
| **Control surface** | The public engine API. Preferred term; "public API" is a synonym. |
| **Engine capability token** | A declared, versioned engine feature a manifest may require. |

## Generation

| Term | Meaning |
|---|---|
| **Resolution** | Identifying and validating a resource without writing anything. |
| **Materialization** | Writing a resolved resource's content into the workspace. |
| **Instantiation** | Converting a resolved template into workspace structure. |
| **Generation plan** | The complete ordered operation and path list, produced without mutation. |
| **Provenance** | The recorded relationship between a generated path and the resource that produced it. |
| **Ownership class** | The regeneration policy attached to a generated path. |
| **Finalization** | The commit point at which workspace ownership transfers to the user. |
| **Regeneration** | Applying generator operations to an existing workspace. |
| **Journal** | The on-disk record enabling crash recovery of an interrupted transaction. |

## Ownership classes

Defined normatively in [[2.7 Project generation model]].

`generated` · `seeded` · `user` · `metadata` · `merged` *(reserved, not
implemented in v1)*
