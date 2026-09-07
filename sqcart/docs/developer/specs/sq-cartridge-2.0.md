# Squared Cartridge Specification, Version 2.0

**Status:** Draft
**Format identifier:** `squared-cartridge`
**`format_version`:** `2`
**File extension:** `.sq`
**Media type:** `application/vnd.squared.cartridge+zip` (provisional)
**Supersedes:** version 1.0. See §0.2.

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

### 0.2 Relationship to version 1.0

Version 2.0 is **not backward compatible** and no compatibility path is
provided. A conforming reader implements exactly one `format_version`
and MUST refuse any other, in either direction (§12.1 step 4).

Three changes account for the break:

- **Kind bodies are replaced by `consumers` (§5.6).** Version 1.0
  defined six typed bodies — `template`, `kit`, `package`,
  `asset-bundle`, `plugin`, `cartridge` — and every field in every one
  of them was a `squared-pg` or Squared framework concept. A reader
  implementing this specification therefore had to know the generator's
  vocabulary, which made §0.1 false in practice: a framework runtime
  linking a conforming reader acquired `integration_areas` and
  `project_types` whether it wanted them or not. Version 2.0 moves all
  of it into namespaced sections a reader carries and never opens.
- **`kind` is an opaque token (§5.2).** It was a closed set of six. A
  closed set obliges a reader to know the ecosystem's roles, which is
  the same defect one level up.
- **`tree` is a required envelope member (§5.1).** Version 1.0 gave the
  payload root to templates only, as a kind body field, and left every
  other kind to a convention the reader could not see.

The break is deliberate and was taken while the ecosystem was small
enough for the migration to be mechanical. `tree` is required rather
than defaulted because a defaulted field is invisible in the manifests
an author learns the format from.

What this costs is worth stating plainly. Version 1.0 could check that
a cartridge's entry module named a present entry, that an asset
bundle's declared paths existed, and that a template's ownership globs
classified every file exactly once. Those were useful checks and they
are gone, because none of them can be made without knowing a
consumer's schema. They belong to the consumer now, which can give a
better diagnostic anyway — it knows what the reference was for.

Readers MUST NOT attempt to upgrade a version 1.0 manifest in place.
The correct response to one is `cartridge.format_unsupported`, naming
the version found and the version implemented.

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

The manifest has a fixed **envelope**, common to every kind and fully
defined here, and an optional **`consumers`** object carrying sections
this specification does not define (§5.6).

That division is the whole of version 2.0. The envelope is what any
reader can act on without knowing who the cartridge is for; everything
else is addressed to someone in particular and carried unopened.

### 5.1 Envelope

```json
{
  "format": "squared-cartridge",
  "format_version": 2,

  "kind": "kit",
  "id": "kit.sdl3",
  "version": "1.2.0",
  "tree": "tree/",

  "title": "SDL3 Integration Kit",
  "description": "Bridges the Squared framework to SDL3.",

  "requires_features": [],

  "authors": [ { "name": "Example Studio" } ],
  "license": "MIT",

  "consumers": {
    "squared_pg": {
      "requires": { "engine": ">=0.1.0 <0.2.0" }
    }
  }
}
```

| Field | Type | Status | Notes |
|---|---|---|---|
| `format` | string | REQUIRED | MUST equal `"squared-cartridge"` |
| `format_version` | integer | REQUIRED | MUST equal `2` for this specification |
| `kind` | string | REQUIRED | Opaque role token, §5.2.1 |
| `id` | string | REQUIRED | Resource identity, §5.2 |
| `version` | string | REQUIRED | Semantic version, §5.3 |
| `tree` | string | REQUIRED | Payload root, §5.1.1 |
| `title` | string | OPTIONAL | Human-facing display name |
| `description` | string | OPTIONAL | Short prose description |
| `requires_features` | array of string | OPTIONAL | Container features the READER must implement, §5.4 |
| `authors` | array of object | OPTIONAL | `name`, optional `email`, `url` |
| `license` | string | OPTIONAL | SPDX identifier where one applies |
| `presentation` | object | OPTIONAL | Paths to thumbnail, icon, and display hints |
| `provenance` | object | OPTIONAL | §5.5 |
| `consumers` | object | OPTIONAL | Consumer sections, §5.6 |

Members not listed here are ignored and preserved (§5.4). A reader MUST
NOT reject a manifest for carrying a member it does not recognise, and
MUST NOT ascribe meaning to one — in particular, a top-level member
named after a kind carries no significance in this version.

`format`, `format_version` and `tree` MUST be validated before any
other member is read (§12.1). Every field after them is interpreted
under the rules of the version the manifest claims, so that claim is
not something a reader may defer.

#### 5.1.1 `tree` — the payload root

`tree` names the directory at which the cartridge's payload begins,
relative to the cartridge root. It has exactly two legal shapes:

- `"."` — the payload begins at the cartridge root.
- A relative directory path ending in `/`, for example `"tree/"` or
  `"a/b/"`.

A reader MUST refuse any other form. In particular it MUST refuse a
path without the trailing separator: `"tree"` is not an accepted
spelling of `"tree/"`, and a reader MUST NOT normalise one into the
other. Accepting both spellings would put two ways of writing one path
into the ecosystem and oblige every reader to canonicalise on the way
out, or let the two forms diverge in indexes, error messages and
provenance records.

A reader MUST additionally refuse a `tree` that:

- is absolute, or contains a `.` or `..` component, or contains an
  empty segment, or contains a character excluded from entry paths by
  §3.3. Entry paths are validated relative to this prefix, so a payload
  root that can escape the cartridge defeats every path check below it;
- begins with `SQ-INF/`. The reserved directory is never payload (§4).

`SQ-INF/` is excluded from the payload regardless of `tree`, including
when `tree` is `"."`. The exclusion is §4's, and does not depend on the
payload root happening to point elsewhere.

A `tree` naming a directory the cartridge does not contain is **not** an
error. A manifest-only cartridge is legal, and a reader that refused
one could not represent an asset bundle awaiting its assets. A
validator SHOULD report it as an informational diagnostic, because the
far more common cause is a misspelling that would otherwise surface as
a missing-file error somewhere with nothing pointing back at the
manifest.

**Why the envelope.** A reader must be able to find the payload without
knowing the kind, and must be able to do so for kinds this version has
not defined — which, since §5.2.1 makes `kind` opaque, is all of them.
Version 1.0 placed the field on templates alone and gave the other five kinds prose conventions
(`bridge/`, `platform/`, `include/`, `assets/`, `lua/`) that no reader
could act on, so implementations guessed: look for entries under
`tree/`, otherwise assume the root. That guess was correct for every
resource that then existed and silently wrong for any cartridge
shipping an unrelated `tree/` directory. The declaration replaces it.

### 5.2 Identity

`id` is a dotted identifier. Each segment MUST match
`[a-z][a-z0-9_]*` and segments are joined with `.`. There MUST be at
least two segments and no more than eight. `id` is compared
byte-for-byte; there is no case folding and no aliasing.

#### 5.2.1 `kind`

`kind` is a single identity segment: `[a-z][a-z0-9_]*`, no dots.

**This specification does not enumerate legal kinds, and a conforming
reader MUST NOT reject a `kind` it does not recognise.** A reader
validates the token's shape and nothing else.

Version 1.0 defined a closed set of six. The set was wrong to be
closed for the same reason the kind bodies were wrong to exist: which
roles the ecosystem has is the ecosystem's question, and a reader that
answers it cannot open a cartridge for a role invented after it
shipped. `kind` is closer to a media type than to a schema selector —
it tells a human or a tool what a cartridge claims to be, without
either needing to know the claim's contents.

Consumers MAY require particular kinds, and MUST report an unusable
one themselves rather than expecting the reader to have refused it.

Kind tokens in use at the time of writing, listed as information and
not as a constraint: `cartridge`, `template`, `kit`, `package`,
`asset_bundle`, `plugin`.

#### 5.2.2 Identifier prefixes

Prefix conventions — `kit.` for kits, `template.` for templates and so
on — are **ecosystem policy, not format rules**. Version 1.0 made them
normative here; enforcing them requires knowing which prefix belongs to
which role, which is the coupling this version removes.

A conforming reader MUST NOT require or infer any relationship between
`id` and `kind`. Consumers MAY impose one.

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

### 5.6 `consumers`

`consumers` is an OPTIONAL object carrying data addressed to consuming
tools. Its keys are consumer identities; its values are sections whose
contents this specification does not define and a conforming reader
does not interpret.

```json
"consumers": {
  "squared_pg": {
    "project_types": ["application"],
    "working_directory": "sq_app",
    "parameters": [
      { "name": "project_name", "type": "identifier", "required": true }
    ],
    "ownership": {
      "generated": ["mk/squared_generated.mk"],
      "user": ["sq_app/**"]
    }
  },
  "squared_framework": {
    "entry": { "module": "game/main.lua", "lua_abi": "lua54" }
  }
}
```

#### What a reader MUST check

1. `consumers`, if present, is an object.
2. Each key is a consumer identity: dotted segments matching
   `[a-z][a-z0-9_]*`, at most 128 characters.
3. Each value is an object.
4. The whole is within the reader's `Limits` (§3.4) for nesting depth
   and size.

#### What a reader MUST NOT do

A reader MUST NOT interpret, validate, transform, reorder or reject a
section's contents. It MUST NOT require any particular key to be
present, and a section addressed to a consumer the reader has never
heard of is not an error — it is the mechanism working.

A reader MUST make each section retrievable by identity, and MUST
preserve member order within a section. Insignificant whitespace need
not be preserved.

#### Why an object, and why namespaced

The values are objects rather than string pairs because a consumer's
data is not flat. `parameters` is an array of objects and `ownership`
is an object of arrays; expressing either as key/value strings means
encoding JSON inside a JSON string, and the consumer then parses JSON
out of a string it got from a JSON parser. An object permits flat
key/value for consumers that want it while not forbidding structure to
those that don't.

Namespacing costs one level of nesting and removes a whole class of
collision. Two consumers both wanting a member called `entry` is not a
hypothetical; `squared-pg` and the Squared framework runtime are
different programs reading the same cartridge.

#### Absent is not empty

A consumer finding no section addressed to it MUST report that, rather
than proceeding with defaults it invented. "This cartridge was not
prepared for me" and "this cartridge asks nothing of me" are different
facts and lead to different, correct behaviours.

#### `engine` and `requires_capabilities` moved here

Both were envelope members through version 1.0 and survive only as
consumer data:

```json
"consumers": {
  "squared_pg": {
    "requires": {
      "engine": ">=0.1.0 <0.2.0",
      "capabilities": ["capability.project.plan@^1.0"]
    }
  }
}
```

`engine` carried an `id` naming the tool it was addressed to, which is
what a `consumers` key already says — the member was restating the
section it belonged inside. And every `requires_capabilities` token
that has been written names a `squared-pg` capability, in a namespace
a reader can shape-check but never resolve.

A reader therefore no longer validates a capability token's grammar. A
malformed one reaches the consumer, which owns the namespace and can
report which capability is missing and what provides it. A reader
could only ever report that a string it did not understand was shaped
wrongly.

**`requires_features` stays in the envelope**, and the contrast is the
point of §5.4:

| | Guards | Fails |
|---|---|---|
| `requires_features` | the **reader** | at `open()`, before anything else |
| a consumer's `requires` | the **consumer** | when that consumer resolves it |

A cartridge whose reader is too old cannot be opened at all, and only
this specification can define what a reader must implement. A cartridge
whose consumer is too old opens fine and must be refused later, by the
consumer, in code this document does not govern.

---

## 6. Kinds — retired

This section defined six kind bodies in version 1.0: `cartridge`,
`template`, `kit`, `package`, `asset-bundle` and `plugin`.

It is retained as a numbered heading, with no normative content, so
that every later section keeps the number it has carried since 1.0.
Cross-references to §7 through §14 appear in source comments, in
consuming projects and in this document; renumbering to close a gap
would invalidate all of them to save one line of explanation.

**`kind` is now an opaque token (§5.2.1) and kind bodies are now
consumer sections (§5.6).**

### 6.1 Where each body went

Every field of every version 1.0 kind body now belongs in a
`consumers` section:

| Version 1.0 kind body | Where it lives now |
|---|---|
| `template` | `consumers.squared_pg` |
| `kit` | `consumers.squared_pg` |
| `package` | `consumers.squared_pg` |
| `asset-bundle` | `consumers.squared_pg` |
| `plugin` | `consumers.squared_pg` |
| `cartridge` | `consumers.squared_framework` |

Their schemas are normative in the documents owned by those consumers,
not here. This specification has no opinion about whether a template
must declare `parameters`, because it does not know what a template is.


### 6.2 Payload layout

Version 1.0 also gave each kind a customary payload layout — `bridge/`
and `platform/` for kits, `include/` and `src/` for packages, and so
on. Those conventions were prose a reader could not act on, and the
guesswork they invited is what `tree` (§5.1.1) replaces.

Layout below the payload root is a consumer's concern. A consumer that
needs sub-roots declares them in its own section.

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
4. Manifest envelope — well-formed JSON; then, in order, `format`,
   `format_version` and `tree` (§5.1.1). A reader MUST reject a
   `format_version` it does not implement here, before reading any
   further member, and MUST NOT accept an older version by ignoring
   fields it lacks. Every member below this point is read on the
   strength of the manifest claiming a format the reader implements.
   The remaining envelope members follow.
5. Features — `requires_features` against reader capability.
6. Consumers — `consumers` is an object, its keys are well-formed
   identities, its values are objects (§5.6). Contents are not
   inspected. Version 1.0 checked a kind body schema and a single-kind
   rule here; both are retired with the bodies (§6).
7. Payload — prohibited entries absent (§8). Version 1.0 also checked
   that manifest-declared paths existed, which required knowing which
   members of a kind body were paths. That check is a consumer's now.
8. Integrity — `hashes.json` if present.
9. Compatibility — engine, framework and dependency ranges. Requires
   a resolution context and is therefore the Package, Kit and Template
   Services' concern rather than the reader's.

Steps 1–8 are decidable from the cartridge alone, **and decidable
without knowing any consumer**. That second property is what version
2.0 restored: several 1.0 checks were decidable from the cartridge yet
required a consumer's schema to perform, which made them look like
container rules while acting as coupling.

Step 9 is decidable from neither, which is why it belongs to the
consuming engine's services.

Step 4's ordering is normative rather than an optimisation. A reader
that defers the version check to a later validation pass parses a
foreign manifest to completion against the wrong version's rules, and
reports the mismatch only if the caller asks for validation at all —
so a cartridge from an unknown format opens successfully and fails
somewhere with no reference to the manifest.

### 12.2 Conformance levels

- **Strict** — every MUST enforced. Required for anything entering a
  generation pipeline or being executed.
- **Lenient** — steps 1–4 enforced; later violations reported as
  diagnostics. For inspection and authoring tools only. Note that
  step 4 includes the version check, so lenient conformance does not
  admit a foreign `format_version`: there is no mode in which a reader
  interprets a format it does not implement.

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
| `cartridge.kind_invalid` | `validation_error` | Malformed `kind` token (§5.2.1). Never raised for an *unrecognised* kind |
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
the consumer sections, `std::ranges` for the sort in §9.2, `std::array` for
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

**No kind tokens are reserved.** `kind` is opaque (§5.2.1) and this
specification does not enumerate roles. The tokens in use today are
listed there as information.

**No `id` prefixes are reserved.** Prefix conventions are ecosystem
policy (§5.2.2), enforced by consumers if at all. Version 1.0 reserved
`template.`, `kit.`, `package.`, `asset.`, `plugin.`, `squared.` and
`sq.`; enforcing them required knowing which prefix belonged to which
role.

Consumer identities are not reserved either, and collisions between
them are resolved socially rather than by this document.

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

5. **Whether the envelope still keeps too much.** *Settled during 2.0
   drafting, recorded because the reasoning generalises.* `engine` and
   `requires_capabilities` were kept on the argument that a foreign tool
   could act on them without knowing whose namespace they named. The data
   refuted it: `engine.id` was `"squared-pg"` in every manifest in
   existence, and every capability token was in that same namespace. Both
   moved into `consumers` (§5.6).

   The test that settled it is reusable: **if a member's value identifies
   the consumer it is addressed to, the member belongs inside that
   consumer's section.** `requires_features` passes the test — it names
   what a *reader* must implement, and readers are what this document
   defines. `presentation` and `provenance` also pass, though `provenance`
   is worth revisiting: its `generator`, `template` and `kits` members are
   squared-pg vocabulary, and it survives only because a foreign tool
   genuinely can display "made by X version Y" without understanding any
   of it.

6. **Whether the shared schema is a real loss.** Version 1.0's §6 was
   a contract two independent implementations could be written
   against, which is the JAR precedent: jar manifests do standardise
   attributes. Version 2.0 gives that up. Nothing now mechanically
   enforces that two tools agree on what a `squared_pg` section
   contains, and the consuming project's own conformance tests become
   the only enforcement. This was accepted deliberately; it should not
   be forgotten.

7. **Whether `cartridge` belongs in this specification at all.** The
   generator kinds are squared-pg's concern; the runtime application
   kind is the framework's. They share a container, which is the
   argument for one document. If the framework's cartridge needs
   diverge substantially, `cartridge` could split into a separate
   specification layered on this container.
