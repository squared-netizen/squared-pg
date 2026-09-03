# Authoring a kit

A kit is a **bridge**. It connects Squared to an external framework, middleware
system or platform:

```text
Squared Application → Squared Framework → Kit → External Framework → Platform
```

A kit does **not** define project structure — that is the template's job — and
must not assume it owns the workspace.

## Layout

```text
resources/kits/kit.mine/
├── SQ-INF/manifest.json
└── tree/
    ├── sq_kit/include/squared/kit/mine.hpp
    └── mk/kit_mine.mk
```

Contribute under paths the template's integration areas cover, and name your
build fragment `kit_<name>.mk` so the template's wildcard include finds it.

## Kit body

```json
"kit": {
  "external": {
    "id": "sdl3",
    "version": ">=3.0.0",
    "acquisition": "system"
  },

  "provides": ["graphics", "input"],
  "platforms": ["termux", "linux"],
  "compatible_templates": ["template.termux.cpp"],

  "requires": {
    "framework": ">=1.0.0 <2.0.0",
    "packages": [],
    "kits": []
  },

  "integration_areas": ["app.include", "build.make.fragments"],
  "conflicts_with": ["kit.sfml"],

  "ownership": {
    "generated": ["sq_kit/**", "mk/kit_mine.mk"],
    "user": ["sq_workspace/**"]
  }
}
```

## Acquisition

| Value | Meaning |
|---|---|
| `package` | supplied by a generator package, offline |
| `system` | expected on the build host |
| `vendored` | the kit ships it into the generated project |

**Generation never requires network access, whichever you choose.** A `system`
kit may require host software at *build* time, and if it does, its build
fragment should detect it and fail informatively rather than cryptically —
`kit.lua`'s `mk/kit_lua.mk` is the worked example: it probes pkg-config, falls
back, and still lets the project build when nothing is found.

## Ownership

Your `generated` paths are yours to rewrite. Anything you seed for the user —
a script workspace, a config file — should be `user`, and the generator will
never touch it again.

**Never overwrite a user-authored file just because your kit originally put a
file at that path.**

## Compatibility

Declare `compatible_templates`, `platforms` and `requires.framework`. All three
are checked at resolution, before any mutation, so a mismatch is reported while
the workspace still does not exist.

`conflicts_with` is honoured even when no integration area collides.

## Two kits, one file

Two resources contributing the same workspace path is a conflict
(`kit.integration_point.conflict`), detected during cross-validation. Give your
files distinct names — this is why the build-fragment convention is
`kit_<name>.mk`.

## Header-only is a gift

`kit.terminal` adds one include path and nothing else, so a workspace using only
it builds on a bare Termux with a compiler and make. If your kit can be
header-only, make it header-only.

## Checklist

- [ ] `id` begins with `kit.`
- [ ] `compatible_templates` names every template you support
- [ ] `integration_areas` lists everything you write, and the template declares them all
- [ ] `acquisition` is accurate, and a `system` kit's build fragment detects and reports
- [ ] contributed paths are namespaced so two kits cannot collide
- [ ] ownership marks user-facing seeds as `user`
- [ ] the generated project builds both with and without your kit
