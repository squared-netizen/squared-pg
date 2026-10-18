# template.term.edu.cpp - implementation notes

Consumer-facing counterpart: [../../programmer/templates/template.term.edu.cpp.md](../../programmer/templates/template.term.edu.cpp.md)
Related: [kit.edu_cpp](../kits/kit.edu_cpp.md)

## Purpose

A smaller sibling of `template.terminal.cpp`. Every deviation below exists to
shorten what a first-time programmer has to read or understand. Lessons are not
part of it: they are a separate, optional kit, so the base workspace stays as
small as possible.

## Layout

```text
resources/templates/template.term.edu.cpp/
  SQ-INF/manifest.json   squared-cartridge format 2, kind "template"
  tree/                  seeded content, run through the `substitute` processor
```

Placeholders used: `{{project_name}}` (Makefile, README), `{{description}}`,
`{{generator_version}}`, `{{template_id}}`, `{{template_version}}` and
`{{kit_list}}` (README only). The README prints `{{kit_list}}` so a learner can
see whether lessons were installed without the template needing conditional
content. The Makefile's own `$(...)`, `$@` and `$<` contain no `{{`, so the
processor leaves them alone.

## Design patterns

None applied, deliberately. The deliverable is a procedural teaching program;
a pattern in the generated code would be one more thing to explain. The
`App` class (pimpl) in `template.terminal.cpp` was left out for the same reason.

The generator-side ownership model (seeded vs generated) is inherited from
`template.terminal.cpp`; it is documented with the generator, not here.

## Deviations from template.terminal.cpp

| Dropped | Why |
|---|---|
| Required `kit.terminal` | A teaching template should need nothing but the standard library. The first version required it and `sqpg new` rejected that, because the kit then listed only `template.terminal.cpp`. Now the kit is *optional* here: `kit.terminal` 0.2.0 lists this template in `compatible_templates`. |
| `mk/squared_generated.mk` | Nothing in the Makefile needs project-wide generated settings; a never-included file would only confuse. |
| `sq_app/include/app.hpp`, `App` class | Pimpl and `argc/argv` plumbing hide the first program. `main.cpp` is a plain `main`. |
| `run.sh` | One way to run: `make run`. |
| `.clang-format` | Formatting is not a lesson. |
| `test` target and `sq_app/test/` | Out of scope for beginners. |
| `debug` target, sanitizers | Compiler-support dependent, noisy on Termux. `-g` is always on. |
| `lua-status`, `kit.lua` | Not a teaching goal. |
| Recursive source glob (`find`) | `$(wildcard sq_app/src/*.cpp)` is readable and make-native. Flat only. |

## Makefile

The Makefile is roughly two thirds comments, on purpose: it is a teaching
document that also builds. Comments explain make's rule model, each variable,
each flag, and each automatic variable at the point it is used.

- Source discovery uses make's built-in `$(wildcard)`, not `$(shell find)`:
  no subprocess, no sort, and it is in the standard make manual.
- Header dependencies use `-MMD -MP` and `-include $(OBJ:.o=.d)`. Without
  them, editing a header leaves stale objects and produces baffling behaviour,
  which is the worst failure for a learner. Two lines, kept. Verified: editing
  an included header rebuilds the dependent file.
- Comments avoid a trailing backslash (make would continue the comment onto
  the next line) and contain no `{{` other than `{{project_name}}`.
- Kit hook: `-include $(wildcard mk/kit_*.mk)` near the end, and `CPPFLAGS` includes
  `$(SQ_KIT_CPPFLAGS)`. This is the same fragment mechanism as
  `template.terminal.cpp`, cut to those two lines, so a kit written for one works
  in the other. `CPPFLAGS` uses recursive `=` because the fragments are read after
  it is defined; changing it to `:=` silently drops every kit's include path.
  With no kit, `mk/` does not exist and the variable is empty. Verified with and
  without a kit. `SQ_KIT_LDLIBS` is deliberately not wired: a kit needing a
  library would have to be refused or the Makefile extended.
- Build output is `build/{{project_name}}`, matching `template.terminal.cpp`.

Complexity: `wildcard` is O(files in one directory); make's dependency graph
walk is linear in objects. Irrelevant at this scale; recorded for completeness.

## Standard library vs custom

No custom utility was introduced.

## Invariants and limits

- Exactly one `main` across `sq_app/src/*.cpp`.
- Flat source tree. A learner who adds a subdirectory gets a link error
  (missing symbols), not a clear message. Known limitation.

## Manifest

Derived from `template.terminal.cpp`'s manifest. Differences:

- `kits.required` is empty; `kits.optional` names `kit.edu_cpp` and
  `kit.terminal` (reserved, read by nothing, kept for the reader).
- `integration_areas` is `app.include`, `app.entrypoint`, `build.make.fragments`
  and `edu.lessons` (all `multi` except the entrypoint). `edu.lessons` exists so
  `kit.edu_cpp` has a declared point to write to (spec 2.9.3: a kit must not
  write a point the template does not declare). `build.make.fragments` lets
  `kit.terminal` land its `mk/kit_terminal.mk`. `workspace.lua` is omitted.
- `executable` is empty (no `run.sh`).
- `ownership.generated` is empty: every file is seeded.
- `layers` lists `sq_app`, and `Makefile` plus `mk` for build.

## Open items for the decision log

- Whether edu templates should omit `mk/squared_generated.mk` (affects
  `workspace.verify` and `sqpg doctor` expectations).
- Whether flat source globbing is acceptable under the "build globs" rule in
  `template.terminal.cpp`'s `AGENTS.md`.
- Whether a template should have to declare an integration area for a kit that
  only adds text files. Declared here to follow spec 2.9.3 literally; if the
  engine does not need it, remove `edu.lessons` from both manifests.
