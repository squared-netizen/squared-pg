# Capabilities

What a manifest may require of the engine. Specification §2.14.1.

A capability token is a named, versioned engine feature. Enumerate them with
`engine->capabilities()` or `sqpg describe`.

## This build provides

| Token | Version | Meaning |
|---|---|---|
| `capability.archive.exploded` | 1.0.0 | read exploded cartridge directories |
| `capability.archive.zip` | 1.0.0 | read `.sq` cartridge archives |
| `capability.metadata.provenance` | 1.0.0 | workspace metadata with a provenance table |
| `capability.project.generate` | 1.0.0 | transactional new-workspace generation |
| `capability.project.plan` | 1.0.0 | produce a generation plan without mutation |
| `capability.resource.index` | 1.0.0 | manifest-identity resource index |
| `capability.template.copy` | 1.0.0 | direct materialization, no substitution |
| `capability.template.substitute` | 1.0.0 | `{{ parameter }}` substitution in text payloads |
| `capability.transaction.rename` | 1.0.0 | same-filesystem staging landed by one rename |

## Requiring one

```json
"requires_capabilities": ["capability.template.substitute@^1.0"]
```

An unsatisfied requirement fails at **resolution**, never at materialization.
That is the rule that matters: a workflow must learn it cannot proceed before
anything is written.

## Versioning

Capability versions are part of the public contract. Removing or narrowing one
is a breaking change and will be documented with a migration path.
