# Standard library deviations

Every place a standard-library solution was passed over, and why. AGENTS.md
requires this list; §1.6 requires the same justification for external
dependencies.

## SHA-256 — vendored

**Passed over.** Nothing. There is no hash in the standard library.

**Used instead.** Brad Conte's public-domain implementation, vendored at
`third-party/crypto-algorithms/`.

**Why it is needed at all.** Provenance hashes are the mechanism behind §2.7.10
conflict detection. Without a content hash recorded at generation time, "the
user edited this file" and "this file is untouched" are indistinguishable, and
every update becomes a guess.

**Why not `std::hash`.** It is not cryptographic, is not stable across runs or
implementations, and produces 64 bits. A provenance record has to survive being
compared by a different build of the tool a year later.

**Verified.** `engine/tests/test_support.cpp` checks the FIPS 180-4 vectors for
the empty string and `"abc"` on every run.

## JSON — yyjson

**Passed over.** Hand-rolled parsing.

**Used instead.** yyjson, already vendored and already a build dependency
because sqcart uses it. Shared through `SQCART_USE_EXTERNAL_YYJSON` so one build
never links two copies.

**Why.** Manifests are JSON (D-021) and the metadata record is JSON. Hand-rolled
JSON parsing is a classic source of quiet acceptance bugs, and there was no
dependency to add — the copy was already there.

**Note.** Writing is hand-rolled (`support::json_write`) rather than done with
yyjson's mutable document API. The reason is ordering: `Value` preserves
insertion order, and §2.7.13 needs metadata to be byte-identical across
equivalent runs. Building a `yyjson_mut_doc` to serialise a structure already in
memory would add a conversion for no benefit.

## Glob matching — hand-written

**Passed over.** `std::regex`.

**Used instead.** `support::glob_match`, about sixty lines.

**Why.** Ownership rules and template `files` entries are globs (§2.8.6,
§2.7.10). Using `std::regex` would mean translating every glob to a regex first,
and the translation is where the bugs live: escaping the pattern's own
metacharacters, getting `*` to stop at `/` while `**` does not, deciding what
`a/**/b` means when the middle is empty. A direct matcher has one behaviour and
one place to document it.

`std::regex` is also notoriously slow to construct, and this runs once per
pattern per path.

## `std::expected` — substituted when absent

**Passed over.** Nothing, when the toolchain has C++23.

**Used instead.** `sqcart::expected` when it does not.

**Why.** `<expected>` is C++23 and this project targets C++20 (AGENTS.md). Two
substitutes under test would be worse than one, so `error.hpp` reuses the one
sqcart already ships and tests rather than adding a second. When the toolchain
provides `std::expected`, `Result<T>` is a plain alias and none of it is
compiled.

## `Object` as a vector of pairs, not `std::map`

**Passed over.** `std::map<std::string, Value>`.

**Used instead.** `std::vector<std::pair<std::string, Value>>`.

**Why, first.** Insertion order is preserved, so serialising a `Value` twice
produces identical bytes. §2.7.13 requires reproducible generated output and
metadata is generated output; `std::map` would reorder every record
alphabetically and, worse, would do it consistently enough that the problem
would not show up until someone compared against a hand-written expectation.

**Why, second.** `std::vector` is specified to support incomplete element types;
`std::map` is not, and `Value` is necessarily incomplete at that point.

**Cost.** Linear lookup. Objects here hold a handful of keys and are read a
handful of times, so a node-based container would trade allocations for a search
that was never the bottleneck.

## `std::filesystem` — used, with `error_code` throughout

Not a deviation, but worth stating: every `std::filesystem` call takes an
`std::error_code` rather than allowing an exception. §2.4.8 forbids exceptions
crossing the public API, and the throwing overloads make that a matter of
remembering to catch rather than a property of the code.
