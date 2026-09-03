# Values

The data types that cross the boundary. Developer counterpart:
[binding layer](../../developer/lua/binding.md).

```cpp
#include <squared/pg/value.hpp>
```

## `Value`

A tagged union: `null`, `boolean`, `integer`, `number`, `string`, `array`,
`object`.

```cpp
Value config = Value::object();
config.set("name", "hello");
config.set("count", std::int64_t{3});
config.set("kits", Value::strings({"kit.terminal", "kit.lua"}));

std::string_view name = config.string_or("name", "");
std::int64_t count    = config.int_or("count", 0);
const Value* kits     = config.find("kits");
```

Accessors return `std::optional` and never coerce. `as_int()` on a `1.5` returns
nothing rather than `1`, and `as_int()` on `"7"` returns nothing rather than
parsing it.

`set()` on a null promotes it to an object; `push()` promotes to an array. That
keeps result assembly free of defensive branching.

## Integers are not doubles

Version components, counts and sizes cross as integers, and the boundary will
not silently narrow. An integer reads as a number (widening is lossless in this
range); a double never reads as an integer.

## Object order is preserved

`Object` is an ordered list of pairs, not a map. Serialising the same `Value`
twice produces identical bytes — which is what makes generated metadata
reproducible.

## From Lua

| Lua | `Value` |
|---|---|
| `nil` | null |
| `true` / `false` | boolean |
| integer | integer |
| float | number |
| string | string |
| table with keys exactly `1..n` | array |
| any other table | object |
| function, userdata, thread | null |

**The empty table is ambiguous.** Lua cannot distinguish an empty list from an
empty map, so `{}` becomes an empty array — and parameter validation accepts an
empty array where an object is expected, and vice versa. Passing
`parameters = {}` works.

## `Version` and `VersionRange`

SemVer 2.0.0, with the comparator range grammar:

```text
>=1.0.0 <2.0.0     conjunctive clauses, space-separated
^1.2.3             >=1.2.3 <2.0.0   (and ^0.2.3 => >=0.2.3 <0.3.0)
~1.2.3             >=1.2.3 <1.3.0
1.2.3              exactly
*  or  omitted     any
```

A pre-release is only selected when a clause explicitly names one at the same
`major.minor.patch`. `>=1.0.0` will not give you `2.0.0-alpha.1`.

## `ResourceId` and `ResourceRef`

`ResourceId` is a validated dotted identifier — a value of that type has been
checked, which a `std::string` with dots in it has not.

```text
template.termux.cpp
kit.terminal@^0.1.0
```

Segments are lowercase, start with a letter, and may contain digits, `_` and
`-`. Two to eight segments. The first names the kind and must agree with the
resource's actual kind.
