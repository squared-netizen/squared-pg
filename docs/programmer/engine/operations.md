# Operations

Every registered operation. Enumerate `engine->operations()` or run
`sqpg operations` for the live list — this page is the prose version of the same
registry.

Developer counterpart: [generation](../../developer/engine/generation.md).

Operations whose id begins with a service name are the **service layer**;
`project.*` composites are the **operation layer**. Both reach the same
registry, and no capability exists only at the operation layer.

---

## `engine.describe`

Engine version, lifecycle state, registered services, capabilities, resource
count, durability mode.

No parameters. Cannot fail.

---

## `engine.operations`

The registry itself: an array of descriptors, each with `id`, `summary`,
`parameters`, `required_services`, `required_capabilities`,
`mutates_workspace`, `error_codes`.

No parameters. Cannot fail.

---

## `resource.list`

| Parameter | Type | Required |
|---|---|---|
| `kind` | string | no — `template`, `kit`, `package`, `asset`, `plugin` |

Returns `{ resources: [ { id, kind, version, title, description } ] }`.

Locations are deliberately absent. Resources are addressed by identity; a path
you can see is a path you will end up depending on.

**Errors:** `validation.parameter.invalid` for an unknown kind.

---

## `template.resolve`, `kit.resolve`, `package.resolve`, `asset.resolve`

| Parameter | Type | Required |
|---|---|---|
| `id` | identifier | **yes** — `kit.terminal` or `kit.terminal@^1.0` |
| `platforms` | array | no |
| `framework` | string | no |
| `template` | identifier | no — the template a kit must be compatible with |

A lone string binds to `id`, so `engine.template.resolve("template.x")` works.

Returns the record, plus `entry_count`, plus `integration_areas` where declared.
`template.resolve` additionally returns `working_directory` and `parameters`.

**Errors:** `<kind>.not_found`, `<kind>.version.unsatisfiable`,
`<kind>.kind.mismatch`, `<kind>.compatibility.platform`,
`kit.compatibility.template`, `framework.compatibility.<kind>`.

All are recoverable and carry actionable detail: `not_found` lists what *is*
available, `version.unsatisfiable` lists the versions considered.

**The engine never substitutes an alternative.** If a fallback is appropriate,
that is your decision to make.

---

## `config.validate`

Checks a generation request without resolving anything. Same parameters as
`project.generate`.

**Errors:** `configuration.input.missing` (names every missing input, not the
first), `configuration.name.invalid`, `configuration.identity.invalid`,
`configuration.type.invalid`, `framework.version.invalid`.

---

## `project.resolve`

Phases 2–6: resolve the template, kits, packages and assets, then
cross-validate. Mutates nothing.

Same parameters as `project.generate`. Returns the resolved template, kits and
working directory.

**Errors:** everything the `*.resolve` operations can return, plus
`kit.integration_point.conflict`, `kit.integration_point.undeclared`,
`kit.compatibility.conflict`, `template.kit.required`,
`template.parameter.missing`, `template.parameter.invalid`.

---

## `project.plan`

Phase 7. Produces the complete generation plan and **creates nothing**.

Same parameters as `project.generate`. Returns:

```text
project_name, workspace, working_directory,
template, template_version, kits[], packages[], assets[],
platforms[], framework_version,
steps[] { action, phase, path, ownership, origin, origin_version, bytes },
step_count, diagnostics[]
```

Executing this plan produces the workspace it describes. Use it for dry runs,
previews and tooling.

**Errors:** everything `project.resolve` can return, plus
`template.ownership.invalid`, `validation.path.unsafe`,
`capability.unsatisfied`, `resource.payload.unreadable`.

---

## `project.generate`

Phases 2–18: resolve, plan, then generate inside a transaction.

| Parameter | Type | Required |
|---|---|---|
| `name` | string | **yes** — a C identifier |
| `output` | path | **yes** — workspace location |
| `template` | identifier | **yes** |
| `kits` | array | no — applied in the order given |
| `packages` | array | no |
| `assets` | array | no |
| `platforms` | array | no |
| `framework` | string | no |
| `parameters` | object | no — template parameter values |

Returns `workspace`, `working_directory`, `files_written`,
`directories_created`, `template`, `kits`, `metadata_file`, `durability`.

**Errors:** everything `project.plan` can return, plus
`filesystem.workspace.exists`, `filesystem.permission`, `filesystem.space`,
`transaction.commit_failed`.

**This build generates new workspaces only.** An existing output location is
refused with `filesystem.workspace.exists`; regenerating in place needs the
journalled update path, which is not implemented.

The whole workspace is built in a sibling temporary directory and landed with
one rename. Either it exists complete, or it does not exist.

---

## `workspace.inspect`

| Parameter | Type | Required |
|---|---|---|
| `workspace` | path | **yes** |

Returns the parsed `.squared/metadata.json`: generator version, project
identity, resolved resources, layer map, and the provenance table.

**Errors:** `filesystem.read`, `configuration.json.malformed`.
