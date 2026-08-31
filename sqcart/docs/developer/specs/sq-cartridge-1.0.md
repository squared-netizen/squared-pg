# Squared Cartridge Specification, Version 1.0

**Status:** Draft
**Format identifier:** `squared-cartridge`
**`format_version`:** `1`
**File extension:** `.sq`
**Media type:** `application/vnd.squared.cartridge+zip` (provisional)

---

## 0. Scope and relationship to squared-pg

This document specifies a container format. It does not specify the
generator, the engine, or the Squared framework runtime.

The container is the single physical representation for every named,
versioned, manifest-identified resource in the Squared ecosystem:

- generator resources consumed by `squared-pg` — templates, kits,
  packages, asset bundles;
- generator extensions — plugins;
- distributable applications produced by the Squared framework —
  cartridges.

This specification satisfies the resource identity model required by
§2.4.4 of the Generator Architecture: a resource is identified by its
manifest, never by its filesystem path.

### 0.1 Independence requirement

The Squared framework MUST NOT depend on `squared-pg` (§2.7.6). Both
systems need to read this format. Therefore:

- This specification is normative and standalone.
- Neither implementation may treat the other as its reference.
- The reader is expected to be a small standalone component
  (`sqcart`) depending only on a ZIP codec and a JSON parser, vendored
  independently by each consumer.

Nothing in this document permits a cartridge to carry generator
implementation details into a generated project.

---

## 1. Design rationale

The format is modelled on the Java Archive specification, which solved
this problem correctly in outline and incorrectly in several specific
places. Hindsight lets us keep the outline and discard the mistakes.

### 1.1 Retained from JAR

| Property | Rationale |
|---|---|
| Ordinary ZIP container | Universal tooling. Inspectable with `unzip`, mountable, streamable, random-access. |
| Reserved metadata directory | A well-known root directory tells a reader how to interpret the rest without out-of-band knowledge. |
| Explicit declared entry point | The equivalent of `Main-Class`. A container that runs must say what runs. |
| Forward compatibility by ignoring unknown metadata | Lets v1 readers survive v1.x additions. |
| Per-entry digests as the integrity primitive | Integrity attaches to content, not to the container envelope. |
| One container serving many roles | `jar`/`war`/`ear`/`apk` are one format. Role belongs in metadata. |

### 1.2 Corrected from JAR

| JAR behaviour | Consequence | Correction here |
|---|---|---|
| Bespoke manifest grammar with 72-byte continuation lines | Hand-parsing, subtle encoding bugs, poor tooling | JSON, UTF-8, parsed with a conventional parser |
| `Class-Path` pulls dependencies by relative filesystem path | Fragile, non-relocatable, unresolvable offline | Dependencies declared by manifest identity only; resolution is the Package/Kit/Template Service's job |
| Signed archives accept unsigned added entries | Signature bypass by injection | The content digest covers the **complete** entry set; any addition, removal or modification invalidates it |
| Entry order, timestamps and compression unspecified | Reproducible builds impossible; hashes unstable | Canonical ordering and fixed timestamps for writers; reproducibility defined over **content**, not archive bytes |
| Multi-Release added a decade late as versioned directories | Retrofit with awkward precedence rules | Target overlay mechanism reserved from v1; base payload must always be complete |
| Path safety unspecified | "Zip slip" traversal, duplicate-name confusion, zip bombs | Strict, normative path and limit rules in §3 |
| Role encoded in the file extension | `.war` vs `.jar` divergence, extension-based trust | Single `.sq` extension; role is `kind` in the manifest and nothing is trusted by name |
| Sealed packages, trusted-library attributes | Complex trust semantics few implemented correctly | Deferred entirely; no trust semantics in v1 |

### 1.3 Non-goals for v1

- Native executable code loading.
- Signature verification and trust policy (layout reserved, behaviour undefined).
- Delta or patch cartridges.
- Network resolution of any kind. The format is offline-first by construction.

---

## 2. Terminology

The key words MUST, MUST NOT, REQUIRED, SHOULD, SHOULD NOT, MAY and
OPTIONAL are to be interpreted as described in RFC 2119.

- **Cartridge** — an instance of this format, in either archived or exploded form.
- **Reader** — an implementation that opens and validates a cartridge.
- **Writer** — an implementation that produces a cartridge.
- **Payload** — every entry not under `SQ-INF/`.
- **Kind** — the declared role of a cartridge (§6).

---

## 3. Container format

### 3.1 ZIP profile

A cartridge in archived form MUST be a ZIP archive conforming to
PKWARE APPNOTE 6.3.x, restricted as follows.

Writers MUST:

- use only compression method `0` (Stored) or `8` (Deflate);
- set general purpose bit 11 (UTF-8 filename encoding) on every entry;
- record accurate CRC-32, compressed size and uncompressed size in
  both the local header and the central directory;
- emit no encrypted entries (general purpose bit 0 MUST be clear);
- emit no data descriptors (general purpose bit 3 MUST be clear);
- emit no extra fields other than the ZIP64 extended information field;
- omit directory entries.

Readers MUST:

- treat the central directory as authoritative;
- reject an entry whose local header disagrees with the central
  directory on name, method, CRC-32 or sizes;
- support ZIP64;
- reject any compression method other than 0 or 8;
- ignore external file attributes, including Unix mode bits. Symbolic
  links, device nodes and executable bits carry no meaning in v1 and
  MUST NOT be materialised.

If a writer emits directory entries anyway, they MUST be zero-length,
Stored, and end in `/`. Readers MUST NOT depend on their presence and
MUST NOT infer structure from their absence.

### 3.2 First-entry rule

`SQ-INF/manifest.json` MUST be the first entry in the archive and MUST
use compression method `0` (Stored).

This allows a reader to identify and parse the manifest from the head
of the file, without scanning to the end for the central directory. It
matters on constrained hosts — reading a manifest off a Termux
filesystem or an Android asset stream should not require a seek to EOF.

A reader MAY additionally locate the manifest through the central
directory, but a cartridge that violates the first-entry rule is
non-conforming and MUST be rejected at strict conformance (§12).

### 3.3 Entry path rules

An entry path is the ZIP entry name interpreted as UTF-8.

A conforming entry path MUST:

- use `/` as its only separator;
- be relative — no leading `/`;
- contain no `.` or `..` segment;
- contain no NUL byte, no backslash, no `:` drive designator, and no
  C0 or C1 control character;
- be non-empty and not end in `/` (except a tolerated directory entry);
- be valid, NFC-normalised UTF-8.

A conforming cartridge MUST NOT contain:

- two entries with byte-identical paths;
- two entries whose paths differ only by ASCII case;
- two entries whose paths differ only by Unicode normalisation form.

Logical paths are **case-sensitive**. The two collision rules above
exist so that a case-sensitive logical model can be materialised safely
onto a case-insensitive host filesystem, which is the normal situation
on Android and macOS.

Readers MUST reject a cartridge violating any rule in this section
before extracting or mapping any entry. Path validation is a
precondition of access, not a step during extraction.

### 3.4 Resource limits

Readers MUST enforce a default limit for each of the following, and MAY
allow a host policy to raise them. They MUST NOT default to unbounded.

| Limit | Default |
|---|---|
| `SQ-INF/manifest.json` uncompressed size | 1 MiB |
| Any single `SQ-INF/` file | 4 MiB |
| Entry path length | 1024 bytes |
| Path depth | 32 segments |
| Entry count | 65 536 |
| Total uncompressed size | 2 GiB |
| Per-entry compression ratio | 200:1 |
| Whole-archive compression ratio | 100:1 |

Exceeding a limit MUST produce a structured error (§13) and MUST NOT
produce a partially materialised result.

### 3.5 Exploded form

A cartridge MAY exist as a directory tree rather than an archive. A
directory is an exploded cartridge if it contains a readable
`SQ-INF/manifest.json`.

Exploded and archived forms are semantically equivalent. A reader that
supports one SHOULD support both.

The rules of §3.3 apply to the relative paths within an exploded
cartridge. Symbolic links inside an exploded cartridge MUST NOT be
followed outside the cartridge root, and SHOULD be rejected.

This is the mechanism that reconciles the format with the repository
layout required by §1.13. A development checkout stores generator
resources exploded:

```text
resources/kits/sdl3/
├── SQ-INF/
│   └── manifest.json
├── bridge/
└── build/
```

Distribution and content-addressed caching store them archived:

```text
resources/cache/kit.sdl3-1.2.0.sq
```

Neither form is canonical. Identity comes from the manifest in both
cases. The repository layout remains a maintenance convenience and does
not become part of the public generator contract, exactly as §2.4.4
requires.

---

## 4. The `SQ-INF/` directory

`SQ-INF/` is reserved. Payload MUST NOT be placed under it.

| Path | Status | Purpose |
|---|---|---|
| `SQ-INF/manifest.json` | REQUIRED | Identity, kind and metadata |
| `SQ-INF/hashes.json` | OPTIONAL | Per-entry content digests (§9) |
| `SQ-INF/thumbnail.png` | OPTIONAL | Presentation image |
| `SQ-INF/icon.png` | OPTIONAL | Presentation icon |
| `SQ-INF/licenses/` | OPTIONAL | Licence texts and notices |
| `SQ-INF/signatures/` | RESERVED | Signature material (§10) |
| `SQ-INF/targets/` | RESERVED | Target overlays (§11) |
| `SQ-INF/*` (other) | OPTIONAL | Implementation-defined |

Readers MUST ignore unrecognised files under `SQ-INF/`. Validators
SHOULD report them as informational diagnostics.

Names not listed above are reserved for future versions of this
specification. Third parties SHOULD namespace their additions, for
example `SQ-INF/x-vendor-name/`.

---

## 5. The manifest

`SQ-INF/manifest.json` is a JSON document encoded as UTF-8 **without**
a byte order mark. It MUST be a JSON object.

The manifest has a fixed **envelope** common to all kinds, and one
**kind body** keyed by the value of `kind`.

### 5.1 Envelope

```json
{
  "format": "squared-cartridge",
  "format_version": 1,

  "kind": "kit",
  "id": "kit.sdl3",
  "version": "1.2.0",

  "title": "SDL3 Integration Kit",
  "description": "Bridges the Squared framework to SDL3.",

  "engine": { "id": "squared-pg", "version": ">=0.1.0 <0.2.0" },
  "requires_features": [],

  "authors": [ { "name": "Example Studio" } ],
  "license": "MIT",

  "kit": { }
}
```

| Field | Type | Status | Notes |
|---|---|---|---|
| `format` | string | REQUIRED | MUST equal `"squared-cartridge"` |
| `format_version` | integer | REQUIRED | MUST equal `1` for this specification |
| `kind` | string | REQUIRED | One of the values in §6 |
| `id` | string | REQUIRED | Resource identity, §5.2 |
| `version` | string | REQUIRED | Semantic version, §5.3 |
| `title` | string | OPTIONAL | Human-facing display name |
| `description` | string | OPTIONAL | Short prose description |
| `engine` | object | OPTIONAL | Consuming-engine compatibility |
| `requires_features` | array of string | OPTIONAL | Critical feature tokens, §5.4 |
| `authors` | array of object | OPTIONAL | `name`, optional `email`, `url` |
| `license` | string | OPTIONAL | SPDX identifier where one applies |
| `presentation` | object | OPTIONAL | Paths to thumbnail, icon, and display hints |
| `provenance` | object | OPTIONAL | §5.5 |
| *`<kind>`* | object | REQUIRED | The kind body, §6 |

A manifest MUST NOT contain a kind body other than the one matching its
`kind`. A reader encountering a foreign kind body MUST reject the
cartridge; this prevents a resource from masquerading as two roles.

### 5.2 Identity

`id` is a dotted identifier. Each segment MUST match
`[a-z][a-z0-9_]*` and segments are joined with `.`. There MUST be at
least two segments and no more than eight. `id` is compared
byte-for-byte; there is no case folding and no aliasing.

For generator-resource kinds, `id` MUST begin with the kind token:

```text
template.android.cpp
kit.sdl3
package.squared.gui
asset.font.default
plugin.holodisk
```

For `kind: "cartridge"`, `id` MUST be a reverse-DNS identifier under a
domain the author controls:

```text
org.example.starfall
```

The pair `(id, version)` is the complete identity of a cartridge.
Nothing else — not the filename, not the directory, not the archive
hash — participates in identity.

### 5.3 Versioning

`version` MUST be a Semantic Versioning 2.0.0 version string.

Version *ranges* appear in compatibility fields and use a restricted
comparator grammar:

```text
range      := clause ( " " clause )*
clause     := op version
op         := ">=" | ">" | "<=" | "<" | "=" | "~" | "^"
```

Clauses separated by spaces are conjunctive. Disjunction is expressed
as an array of range strings. There is no wildcard syntax; `^1.2.0` and
`~1.2.0` carry their usual SemVer meanings.

`format_version` is an integer owned by this specification and is
unrelated to `version`. A reader MUST reject a cartridge whose
`format_version` it does not implement.

### 5.4 Compatibility model

Two tiers, because "ignore what you don't recognise" is safe for
additive metadata and unsafe for semantic changes.

**Default tier.** A reader MUST ignore unrecognised members of any
manifest object. This is what allows v1.1 to add fields without
breaking v1.0 readers.

**Critical tier.** `requires_features` is an array of feature tokens.
A reader MUST refuse to load the cartridge unless it implements every
token listed. Tokens are opaque strings assigned by this specification.

```json
"requires_features": ["compression.zstd", "signing.v2"]
```

The rule is that a writer relying on behaviour a v1.0 reader does not
have MUST declare it here. Silence means "everything I depend on is in
v1.0". This replaces JAR's blanket ignore-unknown rule, which had no
way to say "this one actually matters".

No feature tokens are defined by v1.0.

### 5.5 Provenance

For kinds produced by generation rather than authored by hand, the
manifest MAY record how it was produced.

```json
"provenance": {
  "generator": { "id": "squared-pg", "version": "0.3.1" },
  "template": "template.android.cpp@1.4.0",
  "kits": ["kit.android@1.1.0", "kit.sdl3@1.2.0"],
  "packages": ["package.squared.core@1.0.2"],
  "framework": "1.0.0",
  "generated_at": "1980-01-01T00:00:00Z"
}
```

`provenance` MUST NOT contain machine-specific paths, hostnames,
usernames, or temporary filesystem locations (§2.7.2). It exists to
support update workflows and debugging, not to describe a build machine.

---

## 6. Kinds

A cartridge declares exactly one kind. The kind determines the expected
payload layout and the schema of the kind body. Payload layout is a
convention enforced by the kind body's declarations, not by hard-coded
directory names in the reader.

### 6.1 `cartridge` — a distributable Squared application

Payload convention: `game/`, `assets/`, optional `docs/`.

```json
"cartridge": {
  "entry": {
    "module": "game/main.lua",
    "type": "lua-source",
    "lua_abi": "lua54"
  },
  "framework": { "version": ">=1.0.0 <2.0.0" },
  "packages": ["package.squared.core@^1.0.0"],
  "display": { "orientation": "landscape" },
  "permissions": []
}
```

`entry.module` MUST reference an entry present in the cartridge.
`permissions` is REQUIRED to be present and MAY be empty; an absent
`permissions` array MUST be treated as empty, never as "unrestricted".

### 6.2 `template` — a generated-workspace foundation

Payload convention: `tree/` holding the workspace skeleton.

```json
"template": {
  "project_types": ["application"],
  "platforms": ["android"],
  "tree": "tree/",
  "processing": { "engine": "sq-template-v1", "delimiters": ["{{", "}}"] },
  "parameters": [
    { "name": "project_name", "type": "string", "required": true,
      "pattern": "^[A-Za-z_][A-Za-z0-9_]*$" },
    { "name": "package_name", "type": "string", "required": true }
  ],
  "requires": {
    "framework": ">=1.0.0",
    "kits":     { "required": ["kit.android"], "optional": ["kit.sdl3"] },
    "packages": ["package.squared.core"]
  },
  "ownership": {
    "generated": ["CMakeLists.txt", "sq_app_android/**"],
    "user":      ["sq_app/src/**", "sq_app/include/**"],
    "shared":    ["README.md", ".clang-format"]
  }
}
```

`ownership` is normative and is the manifest-level expression of
§2.7.10 and §2.7.11. Patterns are glob patterns over paths relative to
the generated workspace root. Every materialised path MUST match
exactly one classification; overlapping patterns are a validation error.
A regeneration workflow MUST NOT overwrite a path classified `user`.

A template MUST NOT declare parameters it does not use, and MUST NOT
obtain missing parameter values by any means — the engine returns a
structured validation error and Lua decides (§2.7.3).

### 6.3 `kit` — external framework or platform integration

Payload convention: `bridge/`, `platform/`, `build/`.

```json
"kit": {
  "external": { "id": "sdl3", "version": ">=3.0.0" },
  "provides": ["graphics", "input", "windowing"],
  "platforms": ["desktop", "android"],
  "compatible_templates": ["template.android.cpp", "template.desktop.cpp"],
  "requires": {
    "framework": ">=1.0.0",
    "packages": ["package.squared.core"],
    "kits": []
  },
  "integration_areas": [
    "build.cmake.targets",
    "app.entrypoint",
    "platform.android.manifest"
  ],
  "ownership": { "generated": ["sq_bridge/**"] }
}
```

`integration_areas` is REQUIRED. It is the declared set of generated
project areas the kit writes to, and it is the mechanism by which
§2.7.4's conflict rule becomes checkable: if two selected kits declare
the same integration area and neither declares the other compatible,
the engine MUST report a structured compatibility error **before** any
filesystem mutation.

### 6.4 `package` — reusable Squared framework functionality

Payload convention: `include/`, `src/`, `build/`.

```json
"package": {
  "provides": ["graphics-api", "rendering-services"],
  "platforms": ["desktop", "android"],
  "requires": {
    "framework": ">=1.0.0",
    "packages": ["package.squared.core@^1.0.0"],
    "kits": []
  },
  "layout": { "include": "include/", "src": "src/", "build": "build/" },
  "build": { "system": "cmake", "targets": ["Squared::Graphics"] },
  "features": {
    "vulkan": { "default": true },
    "opengl": { "default": false }
  }
}
```

### 6.5 `asset-bundle` — reusable project data

Payload convention: `assets/`.

```json
"assets": {
  "requires": { "packages": ["package.squared.graphics"] },
  "entries": [
    { "id": "asset.texture.player",
      "path": "assets/textures/player.png",
      "type": "texture", "format": "png",
      "platforms": ["desktop", "android"] }
  ]
}
```

Each declared asset carries its own identity. This is what lets Lua
request `asset.texture.player` without knowing which bundle provides it
or where it sits on disk. An `entries` element whose `path` is absent
from the cartridge is a validation error.

### 6.6 `plugin` — a `squared-pg` extension

Payload convention: `lua/`, optional `resources/`.

```json
"plugin": {
  "extends": "squared-pg",
  "api_version": 1,
  "entry": "lua/init.lua",
  "provides_services": ["template-processor.mustache"],
  "requires": { "engine_capabilities": ["service.template.processor.v1"] }
}
```

This is the packaging half of the extension service model in §2.4.3. A
plugin declares its identity, version, compatibility and exposed
services, which is exactly what that section requires of an extension
service. A missing optional plugin MUST be reported and MUST NOT
prevent the engine reaching the Ready state.

Plugins are Lua and data only in v1. See §8.

---

## 7. Lua code

Lua bytecode is not portable. It is tied to a Lua version, to
`LUAI_MAXSHORTLEN` and integer/float width, and in practice to the
build. A format that ships bytecode as the only form of a module is a
format that breaks when the host Lua changes.

Rules:

1. Lua **source** is the normative representation. For every Lua module
   in a cartridge, the source form MUST be present.
2. Bytecode MAY additionally be present as a load accelerator. It MUST
   declare `lua_abi` and a `target` triple.
3. A reader MUST verify that bytecode matches its own Lua ABI and
   target before loading it, and MUST fall back to source on any
   mismatch rather than failing.
4. A reader MUST NOT load bytecode from an unverified cartridge as a
   trust decision. Lua bytecode is not memory-safe and the standard
   loader does not validate it.

```json
"entry": {
  "module": "game/main.lua",
  "type": "lua-source",
  "lua_abi": "lua54",
  "bytecode_cache": {
    "arm64-android": "game/.cache/main.arm64-android.luac",
    "x86_64-linux":  "game/.cache/main.x86_64-linux.luac"
  }
}
```

Bytecode is a cache, and the format says so structurally.

---

## 8. Native code

A v1 cartridge MUST NOT contain loadable native code: no `.so`, `.dll`,
`.dylib`, no platform executables, no Lua C modules.

C and C++ **source** is expected and permitted — kits and packages
consist largely of it. The prohibition is on artefacts a runtime could
load directly, not on text a generated project will compile.

Readers MUST reject a cartridge containing an entry matching the
prohibited set. A future capability model may relax this; it will be
gated behind a `requires_features` token so that v1.0 readers refuse
such cartridges rather than silently ignoring the capability.

---

## 9. Integrity

### 9.1 `hashes.json`

```json
{
  "algorithm": "sha256",
  "entries": {
    "SQ-INF/manifest.json": "9f2c…",
    "bridge/sdl3_bridge.cpp": "1a08…"
  },
  "content_digest": "c7b1…"
}
```

Digests are lowercase hex over the **uncompressed** bytes of each entry.

### 9.2 Content digest

The content digest is defined over the complete entry set, excluding
only the files that cannot contain their own digest:

1. Take every entry except `SQ-INF/hashes.json` and any entry under
   `SQ-INF/signatures/`.
2. Sort by entry path, byte-wise ascending over the UTF-8 encoding.
3. For each, emit the line `<path> LF <hex sha256 of content> LF`.
4. `content_digest` = SHA-256 over the concatenation of those lines.

Two consequences matter.

Because the digest is computed over content and paths rather than over
archive bytes, **reproducibility is a property of the payload, not of
the compressor**. Two cartridges built by different zlib versions at
different levels have the same content digest. This is the fix for the
reproducible-build problem JAR left open, and it avoids the impossible
task of specifying deflate output byte-for-byte.

Because the digest covers the *complete* set rather than a per-entry
list a verifier walks, **adding an entry changes the digest**. This
closes the JAR signing hole where an attacker appends unsigned entries
to a signed archive and the verifier, checking only listed entries,
reports success.

### 9.3 Canonical writing

For byte-stable output where a build system wants it, writers SHOULD:

- order entries by the sort of §9.2, with `SQ-INF/manifest.json` first;
- set every entry timestamp to `1980-01-01T00:00:00` local-as-UTC, or
  to a caller-supplied fixed epoch;
- emit no extra fields other than ZIP64 when required;
- emit no directory entries.

This is a SHOULD. The content digest is the normative identity.

---

## 10. Signatures (reserved)

v1.0 defines the layout and no behaviour.

```text
SQ-INF/signatures/
├── <keyid>.ed25519.pub
└── <keyid>.ed25519.sig
```

A future version will define signatures as detached Ed25519 signatures
over the `content_digest` of §9.2 together with a context string. It
will not define signatures over "the JSON", because semantically
identical JSON is not byte-identical.

A v1.0 reader MUST ignore this directory and MUST NOT report a
cartridge as verified. Signing arrives when a launcher exists that
enforces a trust policy, and not before.

---

## 11. Targets and variants (reserved)

Multi-target support is designed in now and left unimplemented, because
JAR demonstrated the cost of retrofitting it.

Reserved mechanism: target-specific overlays under
`SQ-INF/targets/<triple>/`, mirroring payload paths, applied over the
base payload after base resolution.

Invariant, normative in v1: **the base payload MUST be complete and
usable on its own.** An overlay may replace or add; it may not be
required for the cartridge to function. A cartridge that depends on an
overlay MUST declare a `requires_features` token, and none is defined
in v1.0, so such a cartridge is non-conforming.

Target triples use the form `<arch>-<os>`, for example `arm64-android`,
`x86_64-linux`, `aarch64-linux`.

---

## 12. Validation and conformance

### 12.1 Validation order

Validation is ordered so that cheap structural rejection precedes any
allocation proportional to attacker-controlled input.

1. Container — ZIP profile (§3.1), first-entry rule (§3.2).
2. Paths — every entry path against §3.3, before any read.
3. Limits — §3.4, enforced during read, not after.
4. Manifest — well-formed JSON, envelope schema, `format_version`.
5. Features — `requires_features` against reader capability.
6. Kind — kind body schema, single-kind rule.
7. Payload — declared paths exist; prohibited entries absent (§8).
8. Integrity — `hashes.json` if present.
9. Compatibility — engine, framework and dependency ranges. Requires
   a resolution context and is therefore the Package, Kit and Template
   Services' concern rather than the reader's.

Steps 1–8 are decidable from the cartridge alone. Step 9 is not, which
is why it belongs to the engine services and not to `sqcart`.

### 12.2 Conformance levels

- **Strict** — every MUST enforced. Required for anything entering a
  generation pipeline or being executed.
- **Lenient** — steps 1–4 enforced; later violations reported as
  diagnostics. For inspection and authoring tools only.

A reader MUST default to strict and MUST require an explicit caller
opt-in for lenient.

---

## 13. Error taxonomy

Cartridge errors map onto the engine error categories of §2.4.8.

| Code | Engine category | Meaning |
|---|---|---|
| `cartridge.container_invalid` | `resource_error` | Not a conforming ZIP, or profile violation |
| `cartridge.path_unsafe` | `resource_error` | §3.3 violation |
| `cartridge.limit_exceeded` | `resource_error` | §3.4 violation |
| `cartridge.manifest_missing` | `resource_error` | No `SQ-INF/manifest.json` |
| `cartridge.manifest_malformed` | `validation_error` | Not valid JSON, or envelope violation |
| `cartridge.format_unsupported` | `compatibility_error` | Unknown `format_version` |
| `cartridge.feature_unsupported` | `compatibility_error` | Unimplemented `requires_features` token |
| `cartridge.kind_invalid` | `validation_error` | Unknown kind, or foreign kind body |
| `cartridge.payload_missing` | `validation_error` | Manifest references an absent entry |
| `cartridge.native_code_prohibited` | `validation_error` | §8 violation |
| `cartridge.integrity_failed` | `resource_error` | Digest mismatch |

Every error MUST carry the cartridge `id` where the manifest parsed
successfully, and the offending entry path where one applies. Errors
cross the Lua boundary as structured results, never as exceptions
(§2.6.12).

---

## 14. Implementation notes

These are non-normative and belong to `squared-pg`'s developer tree.

### 14.1 Dependencies

The reader needs a ZIP codec and a JSON parser. Both are already
vendored: **miniz 3.1.2** and **yyjson 0.12.0**. The ZIP profile of
§3.1 was chosen partly because miniz implements exactly Stored and
Deflate — the restriction costs nothing and removes a whole class of
"which codec do I need" failures on a Termux host. This is the
justification required by the project's standard-library-first rule:
neither capability is in the C++20 standard library.

### 14.2 Standard library usage

`std::filesystem` for exploded-form traversal, `std::span` over mapped
entry bytes, `std::expected` for the result channel, `std::variant` for
the kind body, `std::ranges` for the sort in §9.2, `std::array` for
digest storage. No manual `new`/`delete`; the miniz archive handle is
owned by a `std::unique_ptr` with a custom deleter.

### 14.3 Design patterns

- **Facade** — `Cartridge` presents one interface over miniz, yyjson,
  path validation and limit accounting. Chosen because callers should
  never touch `mz_zip_archive` directly, which is the same reasoning
  §2.6 applies to `lua_State`.
- **Strategy** — one validator per kind, selected on the `kind` field.
  Chosen over a switch because kinds are open to extension and each
  validator has independent state. Deviation from textbook form: the
  strategies are stateless and held as function objects in a static
  registry rather than as constructed policy objects.
- **Chain of Responsibility** — the §12.1 validation pipeline. Chosen
  because ordering is normative and each stage must be able to halt the
  chain. Deviation: stages are a fixed `std::array`, not a linked list;
  the order is specified, so runtime reconfiguration is undesirable.
- **Factory Method** — manifest to typed resource descriptor
  (`TemplateDescriptor`, `KitDescriptor`, …) consumed by the
  corresponding engine service.

### 14.4 Type aliases

Per project convention, these cross a public API boundary or name a
domain concept and are aliased:

```cpp
using CartridgeId      = std::string;   // dotted identity, §5.2
using EntryPath        = std::string;   // validated per §3.3
using ContentDigest    = std::array<std::byte, 32>;
using EntryDigestMap   = std::map<EntryPath, ContentDigest>;
using CartridgeResult  = std::expected<Cartridge, CartridgeError>;
```

`ContentDigest` and `EntryPath` add information — a validated path is
not any old string. `EntryDigestMap` exceeds the nesting threshold and
appears across the integrity API.

---

## Appendix A — Reserved names

Reserved under `SQ-INF/`: `manifest.json`, `hashes.json`,
`thumbnail.png`, `icon.png`, `licenses/`, `signatures/`, `targets/`.

Reserved kind tokens: `cartridge`, `template`, `kit`, `package`,
`asset-bundle`, `plugin`.

Reserved `id` prefixes: `template.`, `kit.`, `package.`, `asset.`,
`plugin.`, `squared.`, `sq.`.

---

## Appendix B — Open decisions

Points where a project-level decision is still needed. Each has a
provisional answer above, marked here so it is not mistaken for settled.

1. **Reserved directory name.** `SQ-INF/` is used throughout.
   Alternatives considered: `SQUARED-INF/` (longer, clearer),
   `.sq/` (hidden by convention on Unix, which buys nothing and hurts
   discoverability).

2. **Single extension.** One `.sq` for all six kinds, role in the
   manifest. The alternative — `.sqt`, `.sqk`, `.sqp`, `.sqcar` — is
   friendlier at a shell prompt but reintroduces the `jar`/`war`/`ear`
   divergence and invites trust decisions based on filename. If human
   ergonomics matter more, per-kind extensions can be permitted as
   *aliases* with the manifest remaining authoritative.

3. **Whether generator resources ship archived at all.** §3.5 makes
   both forms equivalent, so this becomes a packaging policy rather
   than a format question. The repository can stay exploded
   indefinitely.

4. **Where the reader lives.** §0.1 argues for a standalone `sqcart`
   component vendored by both `squared-pg` and the Squared framework
   runtime. The alternative — two independent implementations from this
   document — is more faithful to the independence requirement but
   doubles the attack surface for path-safety bugs.

5. **Whether `cartridge` belongs in this specification at all.** The
   generator kinds are squared-pg's concern; the runtime application
   kind is the framework's. They share a container, which is the
   argument for one document. If the framework's cartridge needs
   diverge substantially, `cartridge` could split into a separate
   specification layered on this container.
