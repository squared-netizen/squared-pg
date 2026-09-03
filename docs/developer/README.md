# Developer documentation

For people **maintaining or extending** squared-pg. Implementation, data
structures, algorithms, patterns, invariants and known limitations.

The public API is documented in
[`docs/programmer/`](../programmer/README.md); this tree mirrors its structure
and each page links to its counterpart.

## Pages

### Engine
- [Architecture](engine/architecture.md) — how the pieces fit, and why
- [Patterns](engine/patterns.md) — every design pattern used, chosen over what, deviating how
- [Lifecycle](engine/lifecycle.md) — the state machine and service registration
- [Resources](engine/resources.md) — discovery, indexing, resolution, and the sqcart relationship
- [Generation](engine/generation.md) — plan building, substitution, ownership classification
- [Transactions](engine/transactions.md) — staging, durability, and why Termux decided the design
- [Limitations](engine/limitations.md) — what this build does not do, and what is deliberately deferred
- [Standard library deviations](engine/deviations.md) — where a stdlib solution was passed over, with justification

### Lua
- [Binding layer](lua/binding.md) — conversion, registry generation, ownership

### Resources
- [Manifest handling](resources/manifests.md) — schema divergence and how extension fields are read

## Orientation

Read [Architecture](engine/architecture.md) first, then
[Patterns](engine/patterns.md). Together they explain most of what looks
arbitrary in the source.

If you are about to change generation behaviour, read
[Generation](engine/generation.md) and [Transactions](engine/transactions.md)
before you start; both contain invariants that are easy to break and hard to
notice.
