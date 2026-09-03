# Lua binding layer

Programmer counterpart:
[writing a workflow](../../programmer/lua/workflows.md).
Specification: §2.6.

## What it is allowed to do

Convert values, forward a call to the operation registry, convert the result
back. Nothing else. §2.6.2 forbids the binding from becoming a workflow layer,
and the pressure to add "just one convenience" here is constant. The test: could
the addition have been a Lua function? If yes, it belongs in `lua/`.

## Generated from the registry

`open_engine` walks `Engine::operations()` and installs a closure per
descriptor, splitting `project.generate` into `engine.project.generate`. The
`engine.` namespace is flattened, so `engine.describe()` rather than
`engine.engine.describe()`.

This is §2.6.3's requirement: the set of callable names cannot drift from the
set of registered operations, because it *is* that set. Adding an operation to
the registry makes it callable from Lua with no edit here.

The operation identity travels as an upvalue rather than being reconstructed
from the table path, so a workflow that copies a function out of the table still
calls the operation it came from.

## String sugar

`engine.template.resolve("template.x")` binds the lone string to the
descriptor's first *required* parameter. §2.5.3 and §2.6.4 both write it that
way, and the alternative — forcing `{id = "template.x"}` — would make the
specification's own examples not work.

It produces exactly the parameter table the explicit form would, so the sugar
has no second meaning.

## Value conversion

**Integers stay integers.** §2.6.5 requires version components, counts and sizes
to cross as integers and forbids silent narrowing. `lua_isinteger` is checked
before `lua_tonumber`.

**Sequence or map.** A Lua table becomes an `Array` when its keys are exactly
`1..n`, and an `Object` otherwise. Lua draws no distinction, so the boundary has
to pick one by inspection — it is the only approach that round-trips both
shapes.

**The empty table is ambiguous** and there is no correct answer. It converts to
`Array`, and `validate_parameters` accepts an empty array where an object is
expected and vice versa. Without that, a workflow passing `parameters = {}`
would be rejected for a distinction Lua cannot express. This is documented
rather than clever: the check is in `type_matches` in `error.cpp`.

**Functions, userdata and threads become null.** Deliberate: converting them
would mean exposing native memory, which is what the boundary exists to prevent.

## No handles

§2.6.6 is about opaque handles with explicit lifetimes and stale-handle
detection. None of that is needed here, because nothing native crosses — every
value is copied into a Lua table. There is no native lifetime for a script to
get wrong.

That is worth stating explicitly, because §2.6.6 reads like a requirement to
build a handle system. It is a requirement to *not leak pointers*, and copying
satisfies it more simply.

## Errors never throw

`engine.execute` returns the result table on failure exactly as on success.
§2.6.12 forbids exceptions crossing into Lua and forbids terminating the host.
The C++ side already returns `OperationResult` rather than throwing, so the
binding has nothing to catch.

`luaL_error` is used only for genuine misuse of the binding itself — calling
`engine.execute` with a non-table, non-string first argument. That is a bug in
the workflow, not an engine failure, and a Lua error is the right report for it.

## A bug worth remembering

The initial implementation read object values at stack index `-3` after pushing
a key copy, when the value was at `-2`. Every configuration value silently
became its own key name, and the symptom was a validation error claiming `kits`
was a string. Lua stack arithmetic after `lua_next` is worth double-checking;
the fix is commented in place.
