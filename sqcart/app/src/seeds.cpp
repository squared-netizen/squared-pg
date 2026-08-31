// SPDX-License-Identifier: MIT
//
// Embedded manifest seeds for `sqcart create`.
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

constexpr std::array<SeedEntry, 6> kSeeds{{
    SeedEntry{"asset-bundle", R"raw({
  "format": "squared-cartridge",
  "format_version": 1,

  "kind": "asset-bundle",
  "id": "{{id}}",
  "version": "{{version}}",

  "title": "{{title}}",
  "description": "Created by sqcart from a plain directory.",

  "assets": {
    "entries": [
{{asset_entries}}
    ]
  }
}
)raw"},
    SeedEntry{"cartridge", R"raw({
  "format": "squared-cartridge",
  "format_version": 1,

  "kind": "cartridge",
  "id": "{{id}}",
  "version": "{{version}}",

  "title": "{{title}}",
  "description": "Created by sqcart from a plain directory.",

  "cartridge": {
    "entry": {
      "module": "{{entry_module}}",
      "type": "lua-source",
      "lua_abi": "lua54"
    },
    "packages": [],
    "permissions": []
  }
}
)raw"},
    SeedEntry{"kit", R"raw({
  "format": "squared-cartridge",
  "format_version": 1,

  "kind": "kit",
  "id": "{{id}}",
  "version": "{{version}}",

  "title": "{{title}}",
  "description": "Created by sqcart from a plain directory.",

  "kit": {
    "external": {
      "id": "{{external_id}}",
      "version": ">=0.0.0"
    },
    "provides": [],
    "platforms": ["all"],
    "compatible_templates": [],
    "requires": {
      "packages": [],
      "kits": []
    },
    "integration_areas": ["TODO.declare.integration.area"],
    "ownership": {}
  }
}
)raw"},
    SeedEntry{"package", R"raw({
  "format": "squared-cartridge",
  "format_version": 1,

  "kind": "package",
  "id": "{{id}}",
  "version": "{{version}}",

  "title": "{{title}}",
  "description": "Created by sqcart from a plain directory.",

  "package": {
    "provides": [],
    "platforms": ["all"],
    "requires": {
      "packages": []
    },
    "build_system": "none",
    "build_targets": []
  }
}
)raw"},
    SeedEntry{"template", R"raw({
  "format": "squared-cartridge",
  "format_version": 1,

  "kind": "template",
  "id": "{{id}}",
  "version": "{{version}}",

  "title": "{{title}}",
  "description": "Created by sqcart from a plain directory.",

  "template": {
    "project_types": ["application"],
    "platforms": ["all"],
    "tree": "tree/",
    "parameters": [],
    "requires": {
      "packages": [],
      "kits": { "required": [], "optional": [] }
    },
    "ownership": {
      "generated": [],
      "user": ["**"],
      "shared": []
    }
  }
}
)raw"},
    SeedEntry{"plugin", R"raw({
  "format": "squared-cartridge",
  "format_version": 1,

  "kind": "plugin",
  "id": "{{id}}",
  "version": "{{version}}",

  "title": "{{title}}",
  "description": "Created by sqcart from a plain directory.",

  "plugin": {
    "extends": "squared-pg",
    "api_version": 1,
    "entry": "{{entry_module}}",
    "provides_services": [],
    "requires": {
      "engine_capabilities": []
    }
  }
}
)raw"},
}};

}  // namespace

std::optional<std::string_view> seed_for(std::string_view kind)
{
    const auto it = std::find_if(kSeeds.begin(), kSeeds.end(),
                                 [&](const SeedEntry& e) { return e.kind == kind; });
    if (it == kSeeds.end()) {
        return std::nullopt;
    }
    return it->json;
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
