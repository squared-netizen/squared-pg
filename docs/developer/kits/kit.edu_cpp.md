# kit.edu_cpp - implementation notes

Consumer-facing counterpart: [../../programmer/kits/kit.edu_cpp.md](../../programmer/kits/kit.edu_cpp.md)
Related: [template.term.edu.cpp](../templates/template.term.edu.cpp.md)

## Purpose

Holds the teaching content that used to be seeded by `template.term.edu.cpp`,
so the template stays minimal and the lessons can be added, removed or versioned
independently.

## Is this a kit?

Spec 2.9.1 defines a kit as a bridge to an external framework or platform. This
kit bridges nothing: it is content. It is a kit anyway because the only other
resource kinds (package, asset bundle) are for framework code and binary assets,
and a kit is the one optional, per-workspace, user-selected addition a template
accepts. It declares no `external` block, which the spec reads as "wraps
nothing". Revisit if an asset-bundle kind becomes the better fit for static
text.

## Layout

```text
resources/kits/kit.edu_cpp/
  SQ-INF/manifest.json   squared-cartridge format 2, kind "kit"
  tree/lessons/cpp/      payload, mapped one-for-one into the workspace
```

## Decisions

| Decision | Why |
|---|---|
| Path `lessons/cpp/`, not `lessons/` | Namespaced by language so a later `kit.edu_python` can add `lessons/python/` without a path conflict (`kit.integration_point.conflict`). |
| Outside `sq_kit/` | `sq_kit/` is generated and rewritten; these files are meant to be annotated. |
| Outside `sq_app/` | The working directory is the learner's code, not reading material. |
| Ownership `user` (recorded `seeded`) | Never overwritten, so notes survive. The trade-off: a corrected lesson in a later kit version does not reach existing workspaces. Acceptable until the update workflow (spec 2.7.11) exists. |
| Integration area `edu.lessons` | Spec 2.9.3 requires a kit to declare every point it writes and the template to declare it first. Declared in both manifests, arity `multi`. |
| No `requires.framework` | The kit has no framework dependency, and absent means unconstrained. |
| No `platforms` | Text works everywhere; absent admits every platform. |
| No build fragment | The substrate rule (spec 2.9.3): the template builds without the kit, and the kit changes nothing about the build. |

## Manifest fields

`compatible_templates` is the name the engine reads (spec 2.9.2). Note that the
`sqpg new` error text prints it as `supported_templates`; the manifest field is
still `compatible_templates`.

## Design patterns

None. Static payload.

## Standard library vs custom

Not applicable; no code.

## Invariants and limits

- Lesson code blocks must compile warning-free with `-std=c++20 -Wall -Wextra`
  and use only the standard library, because the template offers nothing else.
  Verify by extracting each fenced `cpp` block and compiling it whenever a lesson
  changes.
- Lessons contain no `{{`. If kit payload is run through `substitute`, a literal
  brace pair in C++ code would be treated as a placeholder.
- A lesson that needs a kit (for example the terminal helpers from
  `kit.terminal`) cannot live here: `kit.terminal` is not compatible with the
  teaching template.

## Unverified

- Whether the engine requires a kit's paths to fall under a declared
  integration area, or only checks declared areas against the template's list.
  `sqpg plan` on a workspace using the kit will show it.
