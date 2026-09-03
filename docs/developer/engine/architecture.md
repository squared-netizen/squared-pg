# Engine architecture

How the pieces fit. Programmer counterpart:
[control surface](../../programmer/engine/control-surface.md).

## Shape

```text
              host (sqpg, or an embedder)
                        |
                 Lua binding layer          engine/lua/
                        |
                 Engine  (facade)           engine/src/engine.cpp
                        |
        +---------------+---------------+
        |                               |
  operation registry              service locator
  (Command)                       (10 core services)
                                        |
                        +---------------+---------------+
                        |               |               |
                   Resource        Template/Kit/     Transaction
                   Service         Package/Asset      Service
                        |
                    sqcart  (cartridge reader)
                        |
              resources/  (inert data)
```

Dependencies run one way: `third-party/` ← `engine/` ← `lua/` ← hosts (D-026).
`resources/` is inert data with no outgoing dependencies. Generated projects are
sinks — nothing in the repository depends on them.

## Layers, and what each may know

| Layer | Knows about | Must not know about |
|---|---|---|
| Host (`app/`) | engine lifecycle, Lua state | templates, kits, argument grammar |
| Binding (`engine/lua/`) | `Value`, the operation registry | what any operation means |
| `Engine` | services, the registry | Lua, argv, terminals |
| Services | each other, `sqcart` | operations, workflows |
| `sqcart` | the container format | templates, generation, workflows |

The row that gets violated first is the binding. §2.6.2 forbids it from
becoming a workflow layer, and the pressure to add "just one convenience" there
is constant. The test is whether the addition could have been a Lua function; if
it could, it belongs in `lua/`.

## The one-way rule

Capability lives below, policy lives above. The engine can `apply_kit`; only
Lua decides *which* kit. Concretely, the engine never:

- selects a template, kit, package or asset;
- substitutes an alternative when resolution fails;
- prompts, or reads a switch;
- decides an order beyond what §2.7.9 fixes as the reference.

Every one of those is a compile-free rule, which is why they are also asserted
in `engine/tests/test_generate.cpp` rather than left to review.

## Why sqcart reads the manifests

The engine does not parse resource manifests itself. `ResourceService` opens
every resource through `sqcart::Cartridge`, which already enforces the container
profile, path safety, resource limits and the manifest envelope, and which is
linked from vendored in-tree source regardless.

A second manifest reader would be a second chance to disagree about what a
manifest means — and disagreement between the tool that packs cartridges and the
tool that consumes them is the failure mode with the worst symptoms.

The cost is a schema divergence from §2.8.3, documented in
[resources/manifests.md](../resources/manifests.md).

## Where state lives

An engine instance owns everything: services, the resource index, opened
payloads, the last result. There are no globals and no statics with mutable
state, which is what makes §2.4.12's "several instances may coexist" true.

The resource index is built once during initialization and never changes
(D-011). Payloads are opened lazily and cached in `ResourceService`. The *set*
of resources is fixed up front; only retrieval is deferred — which is what makes
hybrid loading deterministic rather than merely lazy.

## Threading

Not thread-safe, thread-affine, one `lua_State` per engine, one writer per
workspace (§2.4.12, D-015). No mutex appears anywhere in the engine, and that
is a decision rather than an omission: adding one would suggest a guarantee the
rest of the design does not make.
