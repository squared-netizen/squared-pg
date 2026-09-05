# Manifest reference

Developer counterpart:
[manifest handling](../../developer/resources/manifests.md).

Every resource is a **cartridge**: either an exploded directory containing
`SQ-INF/manifest.json`, or a packed `.sq` archive. The engine detects which.

```text
resources/kits/kit.terminal/
├── SQ-INF/
│   └── manifest.json
└── tree/                    payload; mapped into the workspace
```

## Envelope

Every manifest, whatever its kind:

```json
{
  "format": "squared-cartridge",
  "format_version": 1,

  "kind": "kit",
  "id": "kit.terminal",
  "version": "0.1.0",

  "title": "Terminal Kit",
  "description": "...",

  "engine": { "id": "squared-pg", "version": ">=0.1.0 <0.2.0" },
  "requires_features": [],
  "license": "MIT",
  "authors": [{ "name": "..." }],

  "kit": { }
}
```

| Field | Required | Notes |
|---|---|---|
| `format`, `format_version` | yes | `"squared-cartridge"`, `1` |
| `kind` | yes | `template`, `kit`, `package`, `asset_bundle`, `plugin` |
| `id` | yes | must begin with the kind token |
| `version` | yes | SemVer 2.0.0 |
| `engine` | no | version range this engine must satisfy |
| `requires_features` | no | the **reader** must implement these, or the cartridge will not open |
| `requires_capabilities` | no | the **consumer** must satisfy these, or resolution fails |
| `<kind>` | yes | exactly one body, matching `kind` |

Unrecognised members are preserved and ignored, so a newer manifest stays
readable.

## Identity

```text
id = segment "." segment { "." segment }
segment = [a-z][a-z0-9_-]*
```

Two to eight segments, lowercase, each starting with a letter. Separators may
not double or end a segment. The first segment names the kind.

```text
template.terminal.cpp     kit.terminal     package.squared.gui
```

`*.squared_pg.*` is reserved for generator-internal resources.

## Version constraints

```text
1.2.3          exactly
^1.2.3         >=1.2.3 <2.0.0
~1.2.3         >=1.2.3 <1.3.0
>=1.0.0 <2.0.0 conjunctive clauses
*  or omitted  any
```

The highest satisfying version is selected. A pre-release is only selected when
a clause explicitly names one at the same `major.minor.patch`.

## Payload

Everything outside `SQ-INF/` is payload. Templates declare their payload root as
`template.tree`; other kinds use `tree/` when present, the cartridge root
otherwise. Payload paths map into the workspace one-for-one.

Paths are substituted too, so `tree/{{project_name}}.desktop` works.

## Ownership

```json
"ownership": {
  "generated": ["mk/squared_generated.mk", "sq_kit/**"],
  "user": ["**"],
  "shared": []
}
```

| Class | Regeneration |
|---|---|
| `generated` | may be overwritten |
| `user` → `seeded` | written once, never overwritten |
| `shared` → `seeded` | reserved; treated conservatively |
| unmatched | `seeded` |

Most specific pattern wins. Two equally specific patterns that disagree is a
manifest defect — the safer class is kept and a warning is emitted.

**A `generated` path may not fall inside the working directory.** That is what
makes "the generator never overwrites your code" a guarantee.

Globs: `*` within a segment, `**` across segments, `?` one character. A trailing
`/**` also matches the directory itself.

## Substitution

`{{ parameter_name }}` in any text payload. `{{"{{"}}` escapes a literal brace
pair. Binary payloads (a NUL byte in the first 8 KiB) are copied verbatim.

**An unknown parameter is an error, not an empty string.** A silent substitution
failure produces a project that looks right and does not build.

Only identifier-shaped names are treated as placeholders, so C++ brace
initialisation is untouched.

### Always available

| Name | Value |
|---|---|
| `project_name` | the requested name |
| `namespace` | defaults to `project_name` |
| `working_directory` | from the template manifest |
| `generator_version` | engine version |
| `template_id`, `template_version` | resolved template |
| `framework_version` | requested framework version |
| `kit_list` | applied kits, comma-separated, or `none` |
