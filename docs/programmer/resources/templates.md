# Authoring a template

A template defines **what kind of project is being created**: the directory
structure, the working directory, the build foundation, and the ownership class
of every path it produces.

Start from [manifests](manifests.md) for the shared envelope.

## Layout

```text
resources/templates/template.mine/
├── SQ-INF/manifest.json
└── tree/
    ├── sq_app/src/main.cpp
    ├── Makefile
    └── mk/squared_generated.mk
```

## Template body

```json
"template": {
  "project_types": ["application"],
  "platforms": ["termux", "linux", "desktop"],
  "tree": "tree/",

  "working_directory": "sq_app",
  "processor": "substitute",

  "layers": {
    "application": ["sq_app"],
    "framework":   ["sq_kit"],
    "build":       ["Makefile", "mk"]
  },

  "integration_areas": ["app.include", "build.make.fragments"],
  "integration_arity": {
    "app.include": "multi",
    "build.make.fragments": "multi"
  },

  "parameters": [
    { "name": "project_name", "type": "identifier", "required": true },
    { "name": "description",  "type": "string", "required": false,
      "default": "A terminal application." }
  ],

  "requires": {
    "framework": ">=1.0.0 <2.0.0",
    "packages": [],
    "kits": { "required": ["kit.terminal"], "optional": ["kit.lua"] }
  },

  "executable": ["run.sh"],

  "ownership": {
    "generated": ["mk/squared_generated.mk"],
    "user": ["**"],
    "shared": []
  }
}
```

## Working directory

Exactly one, and it is the promise your template makes: the user never needs to
edit anything outside it, and nothing generator-owned is written inside it. The
engine enforces the second half — a `generated` path inside it fails with
`template.ownership.invalid`.

Reference convention: `sq_app`.

## Integration areas

Named anchors kits may contribute to. A kit writing an area you do not declare
is refused before anything is written.

`integration_arity` says whether an area admits one contributor or many.
Unlisted areas default to `single`, so two kits writing the same one is a
conflict — conservative on purpose.

## Parameters

Types: `string`, `identifier`, `java_package`, `path`, `bool`, `integer`,
`enum`.

A parameter may declare `default` (a literal) or `default_from` (another
parameter's name). Defaults are pure derivations — they never read the
environment.

The engine validates against your contract and **never prompts**. Missing or
invalid parameters produce one error naming every offender.

## Splitting build files

This is the pattern to copy:

```text
Makefile                   seeded     — the user owns it, and includes:
mk/squared_generated.mk    generated  — the generator owns it entirely
```

The generator gets a file it may rewrite freely; the user gets one it will never
touch. That is the answer to "this file needs to be both user-edited and
generator-maintained", and it is much safer than marker-comment regions.

If your generated fragment defines targets, set `.DEFAULT_GOAL` in the
Makefile — make's default goal is the first target it *sees*, which would
otherwise be one of yours.

## Checklist

- [ ] `id` begins with `template.` and has two to eight segments
- [ ] `working_directory` is declared and no `generated` path falls inside it
- [ ] every path has an intended ownership class, and `user: ["**"]` is the base
- [ ] `{{ }}` references only declared or built-in parameters
- [ ] `requires.kits.required` lists the kits the tree assumes
- [ ] `sqpg plan` shows what you expect
- [ ] the generated project builds and runs
- [ ] the generated project references nothing in the generator tree
