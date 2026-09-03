# Design patterns

Every pattern the engine uses, why it was chosen over the alternatives, and
where it deviates from the textbook form. Required by AGENTS.md §"Design
patterns" and by [§2.2 of the specification](../../spec/02%20Generator%20architecture/2.2%20Engine%20responsibility%20and%20role.md).

Programmer counterpart: [control surface](../../programmer/engine/control-surface.md).

---

## State — engine lifecycle

**Where.** `LifecycleState` in `engine/include/squared/pg/engine.hpp`;
enforcement in `Engine::execute`, `Engine::initialize` and `Engine::shutdown`.

**Why.** §2.4.11 defines seven states and a legality matrix over them. The
alternative is a scatter of `if (!initialized_)` guards at every service entry
point, which has two problems: the matrix stops being reviewable as a single
artefact, and every new entry point is a chance to forget one.

**Deviation.** No state objects and no virtual dispatch. The states have no
behaviour of their own — they only permit or forbid — so a `switch` in three
places is the honest encoding, and introducing seven classes to hold no data
would obscure rather than clarify.

**Consequence worth knowing.** Nested operation execution is rejected for free:
an engine inside an operation is `operating`, not `ready`, and `execute` admits
only `ready`. §2.4.11 requires that rejection and it costs no extra code.

---

## Service Locator — engine services

**Where.** `ServiceLocator` in `engine/src/services/services.hpp`; registration
in `Engine::Impl::register_services`.

**Why.** §2.4.3 names it directly. Ten services need to reach each other
without every one taking nine constructor parameters, and the registry is
enumerable, which `engine.describe` exposes.

**Deviation, and it matters.** The textbook Service Locator is a process-wide
singleton. This one is an instance member, because §2.4.12 permits several
engines in one process and §2.4.13 forbids hidden global state. A global
registry would make two engines in one process silently share services — and
"no two instances may target the same workspace" would become unenforceable.

**Deviation.** Lookup is `locator.get<T>()`, keyed on a `static constexpr
kServiceName` on each service type rather than on a string the caller types.
A typo becomes a compile error instead of a runtime null.

**Rejected.** Constructor injection throughout. It is better for testability in
the small, but the services form a graph with the Filesystem and Resource
services near the root, and threading them through by hand made
`register_services` unreadable for no gain the locator does not give.

---

## Command — operations and the generation plan

**Where.** `OperationDescriptor` + handler in `Engine::Impl`; `PlanStep` and
`GenerationPlan` in `engine/include/squared/pg/plan.hpp`.

**Why, for operations.** §2.4.6 requires the registry to be enumerable so that
Lua, tooling and documentation can discover operations without hardcoding them.
Reifying each operation as a descriptor plus a handler is what makes
`engine.operations` possible, and it is why the Lua binding is *generated* from
the registry rather than hand-listed (§2.6.3) — the two cannot drift.

**Why, for the plan.** §2.7.9 requires the engine to produce the complete
ordered set of operations and paths generation would produce, without mutating
anything, and then requires executing that plan to produce the workspace it
described. Building a list of `PlanStep` objects and having both `project.plan`
and `project.generate` consume the same list is what makes that guarantee
structural. A dry run computed by a second code path is a dry run that can lie.

**Deviation.** A `PlanStep` has no `execute()`. Execution lives in the
Transaction Service. Two reasons: ordering, journalling and rollback are one
decision and belong in one place rather than distributed across step
subclasses; and a plan must be producible by an engine holding no workspace
lock (§2.15.12), which a step that knows how to write itself would invite
someone to violate.

**Deviation.** Step content is resolved at plan time, not apply time — the step
carries the exact bytes. That is heavier in memory, and it is the reason
"executing a plan produces what the plan described" is true rather than
approximately true.

---

## Strategy — template processors

**Where.** The `processor` manifest field, read in `ProjectService::build_plan`;
`copy` and `substitute` are the two implementations.

**Why.** §2.8.8 names the pattern and requires that adding a processor need no
change to the Template Service core. Selection is by manifest declaration, so a
template chooses its own processing without the engine branching on identity.

**Deviation.** Two processors do not yet justify a polymorphic family, so this
is currently a branch, not a class hierarchy. The *seam* is in the right place —
the manifest field, the capability token, the unavailable-processor error — so
promoting it later is local. Registering an abstract `Processor` interface with
two implementations today would be the pattern without the benefit.

**Related.** Asset transformers are the same family keyed by asset type and
platform (§2.10.5). Not implemented; see [limitations](limitations.md).

---

## Factory — engine creation

**Where.** `Engine::create`, returning `std::unique_ptr<Engine>`.

**Why.** §2.4.1 requires an explicit runtime object with a well-defined
ownership boundary, and states there is no global instance and no singleton.
A private constructor plus a static factory makes "the component that created
it is responsible for shutting it down" the only expressible arrangement.

---

## Facade — Engine, and sqcart::Cartridge

**Where.** `Engine` over ten services; `sqcart::Cartridge` over the container
codec, JSON parser, path validator and limit accountant.

**Why.** §2.6.1 forbids the boundary from exposing internal classes, service
implementation details or filesystem internals. One narrow type in front of ten
is what lets the engine's internals be rearranged without a compatibility
change.

---

## Pimpl — Engine, Cartridge, Manifest

**Where.** `Engine::Impl`, and sqcart's equivalents.

**Why.** `engine.hpp` names no service type, no sqcart type and no yyjson type.
A consumer compiling against the public header pulls in `<filesystem>`,
`<memory>`, `<span>` and the project's own value types — nothing more. This is
what makes the public/private boundary in §1.3 real rather than declared.

**Cost.** One allocation per engine and one indirection per call. Both are
irrelevant at the frequency the control surface is called.

---

## Builder — plan construction

**Where.** `ProjectService::build_plan`.

**Why.** A `GenerationPlan` has a dozen fields that must be consistent with one
another, assembled from four resolved resource kinds. A constructor taking
twelve arguments would be unreadable and would not express that some fields are
derived from others.

**Deviation.** A free function rather than a builder class. The plan is built in
exactly one place and never partially, so a class that exists to be filled in
across several call sites would be ceremony.

---

## Not used, and why

**Observer.** No engine event needs more than one consumer. Diagnostics are
accumulated into the result (§2.4.7), which is the shape a workflow can act on;
a subscriber list would add a second reporting channel §2.15.6 explicitly says
should not exist.

**Object Pool.** Nothing is allocated at a frequency that justifies it. A
generation run is one pass over a few dozen files.

**Abstract Factory.** Resource kinds have distinct manifest bodies and distinct
validation, but they are resolved by four separate typed services on purpose
(D-012), not produced by a common factory. A shared factory would need a common
resource base class, and the four kinds have almost nothing in common past the
envelope — which sqcart already handles.
