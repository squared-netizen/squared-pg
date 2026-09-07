// SPDX-License-Identifier: MIT
//
// The embedded manifest seed for `sqcart create`.
//
// One seed, not six. Format 1 shipped a seed per kind, each containing that
// kind's body -- which meant this tool carried the schema of every role in
// the ecosystem in string literals. Format 2 seeds the envelope alone and
// leaves `consumers` empty for whoever knows what belongs there.
//
// The canonical copies live in assets/seeds/*.json. They are embedded here
// rather than read at runtime because a CLI that must locate an assets
// directory breaks the moment it is moved, symlinked, or run from a
// different working directory -- and this tool has to work from an arbitrary
// cd on Termux with no install step.
//
// GENERATED-BY-HAND, CHECKED-BY-TEST: tests/test_seeds.cpp reads
// assets/seeds/*.json and asserts byte equality with these literals, so the
// two cannot drift silently. Edit the JSON, mirror it here, run the test.

#include "seeds.hpp"

#include <algorithm>
#include <array>

namespace sqcart::app {

namespace {

struct SeedEntry {
    std::string_view kind;
    std::string_view json;
};

constexpr std::array<SeedEntry, 1> kSeeds{{
    SeedEntry{"envelope", R"raw({
  "format": "squared-cartridge",
  "format_version": 2,

  "kind": "{{kind}}",
  "id": "{{id}}",
  "version": "{{version}}",
  "tree": ".",

  "title": "{{title}}",
  "description": "Created by sqcart from a plain directory.",

  "consumers": {}
}
)raw"},
}};

}  // namespace

std::optional<std::string_view> seed_for(std::string_view)
{
    // The kind no longer selects a seed -- there is only the envelope, and
    // the kind is substituted into it. The parameter is kept so callers read
    // unchanged and so the signature still says what the seed is *for*.
    return kSeeds.front().json;
}

std::vector<std::string_view> seed_kinds()
{
    std::vector<std::string_view> out;
    out.reserve(kSeeds.size());
    for (const auto& e : kSeeds) {
        out.push_back(e.kind);
    }
    return out;
}

}  // namespace sqcart::app
