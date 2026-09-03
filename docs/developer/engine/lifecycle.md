# Lifecycle implementation

Programmer counterpart:
[control surface](../../programmer/engine/control-surface.md).
Specification: §2.4.

## States

```text
created ──initialize──> initializing ──> ready ⇄ operating
   │                          │            │
   │                          └──> failed  │
   └──────────shutdown────────────┴────────┴──> shutdown ──> destroyed
```

`Engine::state()` is never anything else. The matrix in §2.4.11 is enforced in
three functions — `initialize`, `execute`, `shutdown` — and nowhere else.

## Creation

`Engine::create` allocates and returns `created`. It reads no configuration
file, resolves no resource, and touches no filesystem. That restraint is not
tidiness: §2.7.2 forbids generation from depending on ambient state, and a
constructor that reads the environment would put ambient state in every engine
before any workflow could see it.

## Initialization order

1. Register capabilities.
2. Register services.
3. Register operations.
4. Assert every required core service is present.
5. Build the resource index.
6. Mark `ready`.

Step 4 checks the *registry*, not the fact that `Impl` has the members. That
looks redundant today and stops looking redundant the moment someone moves a
service out of `Impl` — §2.4.3 requires a missing core service to prevent Ready,
and a check against the thing being asserted is the only one that survives a
refactor.

Step 5 is where a malformed third-party resource is reported and skipped, while
a *duplicate identity* fails initialization outright. That asymmetry is
deliberate and comes from §2.8.1: a broken kit is the kit author's problem and
should not brick the generator; two resources claiming one identity is
ambiguity, and resolving it silently would mean the same request generates
different projects depending on scan order.

## Execution

`execute` rejects in a fixed order: lifecycle state, then operation existence,
then parameters. All three fail before any handler runs, satisfying §2.4.6's
"invalid operations must fail before performing any partial modification".

Nested execution needs no special case. An engine inside an operation is
`operating`, and `execute` admits only `ready`.

## Shutdown

Services are members of `Impl`, so releasing them is `Impl`'s destruction — in
reverse declaration order, which is reverse dependency order (§2.4.10). The
registration order in `register_services` deliberately matches the declaration
order so the two descriptions cannot disagree.

`~Engine` calls `shutdown()` if the host did not. §2.4.9 forbids relying on
process termination to release resources, and a host that forgets is a host, not
a reason to leak.
