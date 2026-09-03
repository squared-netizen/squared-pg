# Control surface

The `Engine` class. Developer counterpart:
[lifecycle](../../developer/engine/lifecycle.md),
[architecture](../../developer/engine/architecture.md).

```cpp
#include <squared/pg/engine.hpp>
```

## `EngineConfig`

| Field | Type | Meaning |
|---|---|---|
| `resource_roots` | `vector<path>` | directories scanned for resources, **in this order** |
| `engine_version` | `Version` | reported to manifests; defaults to this build's version |
| `strict_roots` | `bool` | fail initialization when a root is absent (default `false`) |
| `fast_durability` | `bool` | relax fsync to journal transitions only (default `false`) |

Root order matters: it decides which of two conflicting resources is reported
first. Everything else about resource location is an implementation detail you
should not depend on.

## `Engine::create`

```cpp
static std::unique_ptr<Engine> create(EngineConfig config);
```

Returns an uninitialized engine in state `created`. Never fails, never touches
the filesystem, never reads configuration.

**You must shut down what you create.** The destructor will do it if you forget,
but relying on that means you never see the result.

## `Engine::initialize`

```cpp
[[nodiscard]] OperationResult initialize();
```

Registers services, builds the resource index, exposes the control surface.

**Precondition:** state is `created`.
**Postcondition:** `ready` on success, `failed` on failure.
**Errors:** `resource.identity.duplicate`, `resource.root.missing` (only with
`strict_roots`), `internal.service.missing`.

Warnings are normal here. A malformed third-party resource is reported and
skipped, so check `warnings()` even on success.

```cpp
auto engine = Engine::create(std::move(config));
const auto started = engine->initialize();
if (!started.succeeded()) {
    for (const auto& e : started.errors()) log(e.code, e.message);
    return 1;
}
for (const auto& w : started.warnings()) log(w.message);
```

## `Engine::execute`

```cpp
[[nodiscard]] OperationResult execute(std::string_view operation, const Value& parameters);
```

Synchronous. Does not return before its state changes are complete.

**Precondition:** state is `ready`. Calling from inside an operation is
rejected — the engine is `operating` then, not `ready`.

**Errors:** `lifecycle.invalid_state`, `validation.operation.unknown`,
`validation.parameter.invalid`, plus whatever the operation itself may return
(listed per operation in [operations](operations.md), and in the descriptor's
`error_codes`).

Never throws. Never terminates the process. Every failure is a value.

## Introspection

```cpp
std::span<const OperationDescriptor> operations() const noexcept;
std::span<const Capability>          capabilities() const noexcept;
const ResourceIndex&                 resources() const noexcept;
const OperationResult&               last_result() const noexcept;
Version                              version() const noexcept;
LifecycleState                       state() const noexcept;
```

`operations()` is the whole callable surface. Enumerate it rather than
hardcoding names — this is how the Lua binding and the CLI both work, and it is
why they never fall out of date.

`last_result()` is readable in states where `execute` is not, including `failed`
and `shutdown`.

## `Engine::shutdown`

```cpp
[[nodiscard]] OperationResult shutdown();
```

Safe from any valid state, including partial initialization failure. Releases
services in reverse dependency order.

After shutdown the instance is `destroyed` and must not be reused. Your process
may carry on — the engine never assumes ownership of it.

## Lifecycle at a glance

| Call | created | initializing | ready | operating | failed | shutdown | destroyed |
|---|---|---|---|---|---|---|---|
| `initialize` | yes | no | no | no | no | no | no |
| `operations`/`capabilities` | no | no | yes | yes | yes | no | no |
| `execute` | no | no | yes | no | no | no | no |
| `last_result` | no | no | yes | yes | yes | yes | no |
| `shutdown` | yes | no | yes | no | yes | no | no |

## Threading

An instance is **not thread-safe** and belongs to the thread that created it.
Several instances may coexist in one process provided no two target the same
workspace.
