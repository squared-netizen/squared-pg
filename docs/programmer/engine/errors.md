# Errors

Developer counterpart: [transactions](../../developer/engine/transactions.md).

Every operation returns an `OperationResult`. There is no second channel: if you
have the result, you have everything the engine knows. You never need to read a
log or inspect the filesystem to find out what happened.

```cpp
struct EngineError {
    ErrorCategory category;     // closed enum — branch on this
    std::string   code;         // stable dotted string — diagnose with this
    std::string   message;      // human-readable; never parse it
    std::string   operation;
    std::string   resource;
    std::string   path;
    Value         diagnostics;  // structured detail
    bool          recoverable;
};
```

## Two identifiers, on purpose

`category` is **closed**, so you can switch on it exhaustively and the compiler
tells you when a new one appears.

`code` is **open within its category**, so a new specific failure can be added
without breaking code that branches on the category.

Branch on one of those two. Do not branch on `message` — its wording is
explicitly not part of the contract.

## Categories

```text
configuration  validation   resource      template
kit            package      asset         framework
filesystem     transaction  compatibility capability
lifecycle      internal
```

`internal` means a defect in the engine. It is never used for something you
could have caused, because there would be nothing you could do about it.

## Codes

| Code | Meaning |
|---|---|
| `configuration.input.missing` | required inputs absent; `diagnostics.missing` names each |
| `configuration.name.invalid` | project name is not a C identifier |
| `configuration.identity.invalid` | a resource reference is malformed |
| `configuration.type.invalid` | a config field has the wrong shape |
| `configuration.json.malformed` | invalid JSON, with byte offset |
| `validation.operation.unknown` | no such operation; `diagnostics.known_operations` lists them |
| `validation.parameter.invalid` | missing or ill-typed parameters; each is named |
| `validation.path.unsafe` | a resource entry does not name a path inside the workspace |
| `resource.identity.duplicate` | two resources share an identity and version |
| `resource.root.missing` | a declared root is absent (`strict_roots` only) |
| `resource.payload.unreadable` | a payload could not be opened or read |
| `template.not_found` | `diagnostics.available` lists what is indexed |
| `template.version.unsatisfiable` | `diagnostics.considered` lists the versions tried |
| `template.parameter.missing` | `diagnostics.missing` names every one |
| `template.parameter.invalid` | `diagnostics.invalid` names each with its expected type |
| `template.ownership.invalid` | a generated path falls inside the working directory |
| `template.kit.required` | the template requires a kit that was not selected |
| `kit.not_found`, `kit.version.unsatisfiable` | as above |
| `kit.compatibility.template` | the kit does not support this template |
| `kit.compatibility.platform` | the kit does not support a requested platform |
| `kit.compatibility.conflict` | a kit declares an incompatibility with another selected kit |
| `kit.integration_point.conflict` | two contributors to a single-arity area, or the same path |
| `kit.integration_point.undeclared` | a kit writes an area the template does not declare |
| `package.*`, `asset.*` | the same shapes for those kinds |
| `framework.compatibility.<kind>` | a resource requires a different framework version |
| `framework.version.invalid` | the framework version is not valid SemVer |
| `filesystem.workspace.exists` | the output location already exists |
| `filesystem.permission` | could not open a path for writing |
| `filesystem.space` | a write ended before completion |
| `filesystem.directory.create`, `filesystem.read`, `filesystem.remove` | as named |
| `transaction.commit_failed` | the staged workspace could not be landed |
| `capability.unsatisfied` | a resource requires a capability this build lacks |
| `lifecycle.invalid_state` | the call is illegal in the current state |
| `internal.service.missing` | a required core service is not registered |

## Recoverable

`recoverable == true` means a different selection may succeed — a different
template version, a different kit, a corrected parameter. It does not mean
"retry the same thing".

Resolution failures, compatibility failures and validation failures are
recoverable. Filesystem and transaction failures are not.

## Diagnostics are the point

```text
category:  package
code:      package.version.unsatisfiable
resource:  package.squared.gui
message:   no version of package.squared.gui satisfies '^2.0'
diagnostics:
  constraint: ^2.0
  considered: [1.4.2, 1.3.0, 1.0.0]
```

"No compatible version found" is not actionable. The list of versions actually
considered is. Every error the engine raises tries to carry the evidence you
need to act on it.

## Branching

```lua
local result = engine.project.generate(config)
if result.ok then
  return 0
end

if result.error.code == "template.not_found" then
  choose_another_template(result.error.diagnostics.available)
elseif result.error.category == "compatibility" then
  drop_a_kit()
elseif result.error.category == "filesystem" then
  report_and_stop(result.error)
end
```

`result.error` is an alias for the first entry of `result.errors`, for the
common single-error case.
