# sqcart — Functional Specification

**Status:** Draft
**Applies to:** the `sqcart` component, all targets (`sqcart::read`, `sqcart::write`, `sqcart::lua`, `sqcart-bootstrap`)
**Does not specify:** the container format (that is normative in the format
spec) or generator behaviour (that is the engine services' concern)

---

## 0. Related documents

| Document | What it specifies |
|---|---|
| `sq-cartridge-2.0.md` | The `.sq` container format — bytes, layout, manifest schema. Normative. |
| `sqcart-component.md` | Where sqcart sits architecturally — placement, layering, bootstrap rule, dogfood chain. |
| `sqcart/include/sqcart/sqcart.hpp` | The concrete C++20 API — exact signatures. |
| `tools/sqcart-isolation.fish` | The build-time gate that enforces §2.4 of this document. |
| **This document** | What the software does — testable requirements, tracing to the format spec, ready to drive test-suite construction. |

This document does not restate wire-format rules. Where a requirement here
exists *because of* a format-spec rule, it cites the section (`§3.3`, etc.)
rather than repeating it. Terminology — Cartridge, Reader, Writer, Payload,
Kind, and the RFC 2119 keywords MUST/SHOULD/MAY — is as defined in the format
spec §2.

Requirement IDs use the scheme `FR-<AREA>-<N>`. They are stable identifiers:
once assigned, an ID is never reused for a different requirement, even if the
requirement is later removed.

---

## 1. Purpose and scope

### 1.1 Purpose

This document enumerates, as atomic and testable statements, what `sqcart`
does in response to every input it can be given. It exists so that:

- an implementer can build against numbered requirements rather than prose;
- a test author has a checklist, not a guess, for what the test suite needs
  to cover;
- a reviewer can ask "which FR does this diff satisfy" and get an answer.

### 1.2 Scope

In scope: everything the four `sqcart` targets do — opening, validating,
reading, digesting, extracting and writing cartridges, plus the Lua binding
and the bootstrap CLI.

Out of scope, and deliberately absent from every requirement below:

- resource resolution, dependency graphs, version solving (engine services,
  §2.4.3 of the Generator Architecture);
- generation, template processing as *behaviour*, workflow sequencing;
- cross-cartridge conflict detection — e.g. whether two *different* kits'
  declared `integration_areas` collide is an engine question (§2.7.4), not
  a question sqcart answers about one cartridge in isolation;
- signature verification and trust policy (format spec §10, reserved);
- target overlay resolution (format spec §11, reserved);
- network access of any kind, anywhere.

A function is out of scope for sqcart if the Squared framework runtime —
which has never heard of a template, a kit, or a generation pipeline — would
have no use for it. This is the same test `sqcart/AGENTS.md` gives human
reviewers, restated here as the boundary of this document.

---

## 2. Overall description

### 2.1 Product perspective

```text
                    squared-pg      framework runtime    launcher    tooling
                        |                  |                |          |
                        +------------------+--------+-------+----------+
                                                    |
                                         sqcart public API
                                                    |
                              +----------------------+----------------------+
                              |                                             |
                          sqcart::read                                sqcart::write
                       (required, default ON)                     (opt-in, tooling only)
                              |                                             |
                              +----------------------+----------------------+
                                                    |
                                        container + JSON backends
                                           (miniz, yyjson)
```

sqcart answers one question — *is this a valid cartridge, what does it
declare, and what are its bytes* — and nothing downstream of that answer.

### 2.2 Consumers

| Consumer | Links | Typical operations |
|---|---|---|
| `squared-pg` engine (Resource, Template, Kit, Package, Asset Services) | `sqcart::read` | open, validate, read entries, inspect kind bodies |
| Squared framework runtime | `sqcart::read` | open a `cartridge`-kind archive at application startup, read the entry point, extract payload |
| Platform launcher (Android/Termux) | `sqcart::read` | open, inspect manifest for presentation metadata, extract |
| Packaging tooling / CI | `sqcart::read` + `sqcart::write` | pack, verify, digest, list |
| `squared-pg` Lua workflows and standalone scripted tools | `sqcart::lua` | same operations, from Lua |
| Release engineering | `sqcart-bootstrap` CLI | pack/list/verify before a working generator exists |

### 2.3 Operating environment

Termux on `arm64-android` and desktop Linux, both with a C++20 toolchain.
`std::expected` (C++23) is not assumed available; see `expected.md`. No
network access is assumed or used at build, test, or run time (§1.13 of the
Repository Conventions).

### 2.4 Constraints

- **FR-CON-1.** No file under `sqcart/` SHALL include anything outside the
  `sqcart/` tree.
- **FR-CON-2.** `sqcart/CMakeLists.txt` SHALL configure, build, and pass its
  tests with no parent tree present. This is enforced mechanically by
  `tools/sqcart-isolation.fish`, not by convention alone.
- **FR-CON-3.** No backend type (`mz_zip_archive`, `yyjson_doc`, or
  equivalent) SHALL appear in any header under `sqcart/include/`.
- **FR-CON-4.** No exception SHALL cross the public API boundary. Every
  fallible public operation returns `Result<T>`.
- **FR-CON-5.** `squared-pg` SHALL link `sqcart` from vendored source at
  build time and SHALL NOT acquire it as a `.sq` cartridge (component spec
  §2). No requirement in this document may be satisfied by a design that
  requires a cartridge reader to read the cartridge containing the
  cartridge reader.

### 2.5 Assumptions and dependencies

- miniz and yyjson are available, either vendored under
  `sqcart/third_party/` or supplied by the host build per
  `SQCART_USE_EXTERNAL_*`.
- The host filesystem may be case-sensitive or case-insensitive; sqcart
  MUST behave correctly under both (format spec §3.3).
- Unless a function is explicitly documented otherwise, sqcart types are not
  internally synchronised: concurrent `const` access is safe, concurrent
  mutation is not.

---

## 3. Functional requirements

### 3.1 Opening a cartridge — `Cartridge::open`, `Cartridge::open_memory`

- **FR-OPEN-1.** `open()` SHALL accept either a `.sq` archive or an exploded
  directory containing a readable `SQ-INF/manifest.json`, and SHALL detect
  which form it was given without requiring the caller to specify (format
  spec §3.5).
- **FR-OPEN-2.** `open()` SHALL default to `Conformance::strict`.
- **FR-OPEN-3.** Under `strict` conformance and an archived cartridge,
  `open()` SHALL reject an archive whose first entry is not
  `SQ-INF/manifest.json` stored with method 0 (format spec §3.2), returning
  `container_invalid`.
- **FR-OPEN-4.** `open()` SHALL run the validation pipeline in the order
  defined by format spec §12.1 up to and including stage 8 (payload
  presence). Stage 9 (compatibility resolution) SHALL NOT be performed by
  `open()`; it requires a resolution context sqcart does not have.
- **FR-OPEN-5.** On the first pipeline stage that fails, `open()` SHALL
  return the corresponding `Result` failure and SHALL NOT proceed to later
  stages. Failures are not accumulated across stages.
- **FR-OPEN-6.** `open_memory()` SHALL apply identical validation to
  `open()`. The returned `Cartridge` SHALL be valid only for the lifetime of
  the caller-supplied `span`; this is a documented precondition, not a
  runtime-checked one.
- **FR-OPEN-7.** Under `Conformance::lenient`, `open()` SHALL perform only
  stages 1–4 of §12.1 (container, paths, limits, manifest well-formedness).
  Stages 5–8 SHALL be skipped, and any violations they would have caught
  SHALL instead surface later, if at all, as diagnostics from `validate()`
  (§3.9) rather than as an `open()` failure.

### 3.2 Path validation — `validate_entry_path`, internal to `open`/`extract_to`

- **FR-PATH-1.** A path SHALL be rejected with `path_unsafe` if it: has a
  leading `/`; contains a `.` or `..` segment; contains NUL, a backslash, a
  `:` drive designator, or a C0/C1 control character; is empty; or (except a
  tolerated directory entry) ends in `/` — per format spec §3.3.
- **FR-PATH-2.** Within one cartridge, two entries whose paths differ only
  by ASCII case, or only by Unicode normalisation form, SHALL both be
  rejected with `path_unsafe`, even though neither path is independently
  invalid.
- **FR-PATH-3.** A path exceeding `Limits::max_path_bytes` or
  `Limits::max_path_depth` SHALL be rejected with `limit_exceeded`, not
  `path_unsafe` — the two error codes are distinguished by cause, not
  merely by outcome, so a caller can tell "malicious" from "too big."
- **FR-PATH-4.** `validate_entry_path()` SHALL be usable standalone, without
  an open `Cartridge`, applying exactly the same rules as path validation
  performed during `open()` and `Writer::add()`. A writer preparing entries
  before a container exists SHALL be able to rely on this function alone.
- **FR-PATH-5.** Path validation SHALL occur before any byte of the
  corresponding entry is read into memory. A cartridge with one invalid path
  among ten thousand valid ones SHALL fail at path-check time, not after
  partially decompressing the archive.

### 3.3 Resource limits — `Limits`

- **FR-LIM-1.** Every limit in format spec §3.4 SHALL be enforced with the
  specified default when the caller does not override `OpenOptions::limits`.
- **FR-LIM-2.** No limit SHALL be disableable by setting it to zero or an
  unbounded sentinel; a `Limits` field of `0` SHALL be treated as "reject
  everything" for that dimension, not "no limit."
- **FR-LIM-3.** Compression-ratio limits (`max_entry_ratio`,
  `max_archive_ratio`) SHALL be evaluated using declared sizes from the
  central directory before full decompression begins, so that a crafted
  small archive claiming an enormous uncompressed size is rejected without
  first attempting to allocate or decompress that size.
- **FR-LIM-4.** Exceeding any limit SHALL produce `limit_exceeded` and SHALL
  NOT produce a partially materialised result — no directory created, no
  file written, for that operation.

### 3.4 Manifest parsing — `Manifest::parse`, `Cartridge::manifest`

- **FR-MAN-1.** The manifest SHALL be parsed as UTF-8 without a byte order
  mark; a leading BOM SHALL cause `manifest_malformed`.
- **FR-MAN-2.** A manifest missing any envelope field marked REQUIRED in
  format spec §5.1 (`format`, `format_version`, `kind`, `id`, `version`)
  SHALL cause `manifest_malformed`.
- **FR-MAN-3.** A manifest whose `format` is not exactly `"squared-cartridge"`
  SHALL cause `manifest_malformed`, not `format_unsupported` — the latter is
  reserved for a recognised format at an unsupported version.
- **FR-MAN-4.** A manifest whose `format_version` this build does not
  implement SHALL cause `format_unsupported`, **at parse time, before any
  other member is read**. This build implements exactly `2`. There is no
  compatibility path for format 1 in either direction.
- **FR-MAN-5.** `kind` SHALL be validated for shape only: one identity
  segment matching `[a-z][a-z0-9_]*`. A malformed token SHALL cause
  `kind_invalid`. An **unrecognised** token SHALL NOT: this library does
  not enumerate the ecosystem's roles, and a cartridge of a kind it has
  never seen SHALL open, index and report normally (format spec §5.2.1).

  *Retired.* This requirement previously rejected a manifest carrying a
  body that did not match its `kind`. There are no kind bodies to be
  foreign, and a `consumers` section addressed to a namespace that is not
  yours is the mechanism working, not a masquerade.
- **FR-MAN-6.** `consumers` sections SHALL be carried without
  interpretation. A conforming implementation SHALL check that `consumers`
  is an object, that each key is a well-formed consumer identity, and that
  each value is an object; it SHALL check nothing else about a section's
  contents. `Manifest::consumer()` SHALL return the section as JSON text
  with member order preserved, and `Manifest::consumers()` SHALL enumerate
  identities. An absent section SHALL be reported as absent and never
  synthesised as empty (format spec §5.6).
- **FR-MAN-7.** Unrecognised members of any manifest object SHALL be
  ignored, not rejected (format spec §5.4, default tier), and SHALL remain
  retrievable via `Manifest::raw_json()`.
- **FR-MAN-8.** Every token in `requires_features` SHALL be checked against
  `OpenOptions::supported_features`. If any declared token is absent from
  the supplied set, `open()` SHALL fail with `feature_unsupported` naming
  the missing token, and SHALL NOT partially honour a manifest whose
  critical requirements are not fully met.
- **FR-MAN-9.** `Manifest::parse()` SHALL accept a manifest in isolation,
  without a surrounding container, applying the same rules as manifest
  validation during `open()`.
- **FR-MAN-10.** The structural constraints a manifest must satisfy SHALL
  match `docs/developer/specs/schema/sq-manifest-2.schema.json` exactly,
  whether or not the implementation runs that schema literally. A manifest
  the schema accepts and sqcart rejects, or vice versa, is a defect in
  whichever side disagrees with the schema — the schema is the tie-breaker.

### 3.5 Kind bodies — retired

FR-KIND-1 through FR-KIND-5 required this library to know a template's
ownership globs, a kit's integration areas, an asset entry's path, a
cartridge's permission default and a Lua entry's bytecode target.

Every one of them was a rule about a *consumer's* schema, and holding them
here is what made format spec §0.1 false in code: a framework runtime
linking this library acquired the generator's vocabulary whether it wanted
it or not.

They are not weakened or relocated within this document. They are the
consuming project's requirements now, verified by its own conformance
tests. What this library owes a consumer is FR-MAN-6: the section arrives
intact, addressed, and unopened.

The loss is real and worth naming. FR-KIND-3 in particular — every declared
asset path is present in the cartridge — was a genuinely useful check that
nothing here replaces. A consumer can make it better, because it knows what
the reference was for.

### 3.6 Entry access — `entries`, `contains`, `find`, `read`, `read_into`

- **FR-ENTRY-1.** `entries()` SHALL return every payload and `SQ-INF/` entry
  in a stable order for a given cartridge — the central directory order for
  an archive, and lexicographic path order for an exploded directory — such
  that two calls against the same unmodified cartridge return identical
  sequences.
- **FR-ENTRY-2.** `contains()` and `find()` SHALL return `false` /
  `nullptr` for a syntactically invalid path rather than raising an error;
  path validity is a precondition of `open()`, already satisfied for every
  entry the cartridge actually holds, so a query for an impossible path is
  simply a miss.
- **FR-ENTRY-3.** `read()` SHALL return the complete decompressed content of
  an entry or fail; it SHALL NOT return a truncated buffer under any
  circumstance, including when the entry exceeds `Limits`. Exceeding the
  limit SHALL fail with `limit_exceeded` before any bytes are copied to the
  caller.
- **FR-ENTRY-4.** `read_into()` SHALL fail if `out` is smaller than the
  entry's uncompressed size, and SHALL NOT write any bytes into `out` in
  that case — no partial writes.
- **FR-ENTRY-5.** Reading an entry whose declared CRC-32 does not match its
  decompressed content SHALL fail with `container_invalid`, distinct from
  `integrity_failed`, which is reserved for `SQ-INF/hashes.json` mismatches
  specifically.

### 3.7 Integrity — `content_digest`, `declared_digests`, `verify_integrity`

- **FR-INT-1.** `content_digest()` SHALL implement format spec §9.2 exactly:
  SHA-256 over `path LF hex-sha256(content) LF` lines, entries sorted
  byte-wise ascending by path, excluding `SQ-INF/hashes.json` and everything
  under `SQ-INF/signatures/`.
- **FR-INT-2.** `content_digest()` SHALL be independent of the archive's
  compression method and compression level. Two archives with identical
  payload content but different deflate settings SHALL produce identical
  digests.
- **FR-INT-3.** `declared_digests()` SHALL parse `SQ-INF/hashes.json` into
  an `EntryDigestMap` and SHALL return `resource_error` (via
  `container_invalid` or `manifest_malformed` as appropriate) if the file
  is present but malformed. If the file is absent, `declared_digests()`
  SHALL return an empty map, not an error — absence of declared digests is
  not itself a fault.
- **FR-INT-4.** `verify_integrity()` SHALL recompute the digest of every
  entry named in `declared_digests()` and fail with `integrity_failed`,
  naming the first mismatching entry, on any disagreement.
- **FR-INT-5.** When `SQ-INF/hashes.json` is absent, `verify_integrity()`
  SHALL succeed vacuously (there is nothing declared to contradict). A
  caller requiring mandatory integrity checking SHALL check
  `declared_digests()` for non-emptiness itself; sqcart does not invent a
  requirement the format does not impose. *(Flagged for review — see §10.)*
- **FR-INT-6.** `OpenOptions::verify_integrity = true` SHALL apply the same
  rules as an explicit `verify_integrity()` call, performed during `open()`
  before it returns successfully.

### 3.8 Extraction — `extract_to`

- **FR-EXT-1.** `extract_to()` SHALL re-validate every entry path
  immediately before writing it, independently of validation already
  performed at `open()` time. This defense-in-depth SHALL hold even though
  it is provably redundant for a cartridge that passed `open()` unmodified —
  it exists for cartridges opened under `Conformance::lenient` and for any
  future code path that constructs a `Cartridge` without going through the
  full pipeline.
- **FR-EXT-2.** `extract_to()` SHALL NOT write any file outside `dest` under
  any input, including a manifest or entry list crafted after a successful
  but lenient `open()`.
- **FR-EXT-3.** With `overwrite = false` (the default), `extract_to()` SHALL
  fail before writing anything if any destination path already exists —
  the check SHALL precede all writes, not interleave with them.
- **FR-EXT-4.** `extract_to()` is not itself transactional across multiple
  entries: on a mid-extraction failure, entries already written SHALL
  remain on disk, and the returned error SHALL identify the entry that
  failed. Callers requiring all-or-nothing semantics across a whole
  extraction — as the engine's Transaction Service does per format spec
  §2.3.13 — SHALL wrap `extract_to()` in their own staging-and-rename
  strategy. sqcart does not duplicate the engine's transaction model.
- **FR-EXT-5.** `extract_to()` SHALL NOT restore Unix permission bits,
  symbolic links, or any other external file attribute recorded in the
  archive, per format spec §3.1's rule that such attributes carry no
  meaning in v1.

### 3.9 Validation pipeline — `validate`

- **FR-VAL-1.** `validate()` SHALL run stages 1–8 of format spec §12.1 and
  SHALL NOT attempt stage 9 (compatibility resolution).
- **FR-VAL-2.** `ValidationReport::conforming` SHALL be `true` if and only
  if `diagnostics` contains no entry of `Severity::error`.
- **FR-VAL-3.** An unrecognised file under `SQ-INF/` SHALL produce a
  `Severity::info` diagnostic and SHALL NOT affect `conforming`.
- **FR-VAL-4.** For a `template`-kind cartridge, `validate()` SHALL check
  that every path materialisable from `tree/` matches exactly one of
  `ownership.generated` / `.user` / `.shared`; a path matching zero or more
  than one SHALL produce a `Severity::error` diagnostic naming the path and
  the conflicting classifications.
- **FR-VAL-5.** `validate()` operates on one cartridge in isolation. It
  SHALL NOT accept, and SHALL NOT require, information about any other
  cartridge. Detecting that two independently selected kits' declared
  `integration_areas` collide is out of scope for this function — see §1.2 —
  and no future revision of this requirement may add a multi-cartridge
  parameter to `validate()`; that capability belongs in an engine service.

### 3.10 Error reporting

- **FR-ERR-1.** Every fallible public function SHALL return `Result<T>`.
  Any internal C++ exception SHALL be caught and converted to an `Error`
  before crossing the public API boundary; none SHALL propagate to the
  caller.
- **FR-ERR-2.** `Error::id` SHALL be populated whenever the manifest parsed
  successfully, even if a later pipeline stage is what ultimately failed —
  for example, `payload_missing` after a valid manifest SHALL still carry
  the cartridge's `id`.
- **FR-ERR-3.** `Error::entry` SHALL be populated whenever the failure is
  attributable to one specific entry, and SHALL be absent when it is not
  (e.g. `format_unsupported`, which is a property of the whole manifest).
- **FR-ERR-4.** `Error::recoverable` SHALL be set per the following table,
  reflecting whether an alternative workflow action is meaningfully
  available at the point of failure:

  | Code | Recoverable | Rationale |
  |---|---|---|
  | `container_invalid` | false | Nothing to salvage |
  | `path_unsafe` | false | Security boundary |
  | `limit_exceeded` | false | Input rejected outright |
  | `manifest_missing` | false | No cartridge to act on |
  | `manifest_malformed` | false | No cartridge to act on |
  | `format_unsupported` | true | A different version may satisfy the caller |
  | `feature_unsupported` | true | A fallback path may exist |
  | `kind_invalid` | false | Wrong kind for the request |
  | `payload_missing` | false | Cartridge is internally inconsistent |
  | `native_code_prohibited` | false | Security boundary |
  | `integrity_failed` | false | Trust boundary |
  | `io_failed` | true | Often transient |
  | `internal` | false | Library defect; not the caller's to fix |

### 3.11 Writer (`SQCART_ENABLE_WRITER`)

- **FR-WRITE-1.** `Writer::create()` SHALL truncate an existing file at
  `out` only once `finish()` is called successfully; a `Writer` that is
  never finished SHALL NOT leave a partial file at `out`.
- **FR-WRITE-2.** `set_manifest()` SHALL apply the full manifest validation
  of §3.4–§3.5 before accepting the manifest, and SHALL reject a manifest
  that would fail `Manifest::parse()`.
- **FR-WRITE-3.** `add()` and `add_file()` SHALL apply
  `validate_entry_path()` to every path and SHALL reject a path already
  added to the same `Writer` — no silent overwrite within one write
  session.
- **FR-WRITE-4.** `add_tree()` SHALL reject a symbolic link encountered
  while walking `root`, rather than following it or silently skipping it.
  The caller SHALL resolve or exclude symlinks before calling `add_tree()`.
- **FR-WRITE-5.** `finish()` SHALL fail if no manifest has been set.
- **FR-WRITE-6.** With `WriteOptions::canonical = true` (the default),
  `finish()` SHALL produce output meeting every rule in format spec §9.3:
  entries sorted by the §9.2 ordering, `SQ-INF/manifest.json` first and
  Stored, every entry timestamp fixed to `WriteOptions::timestamp`, and no
  directory entries emitted.
- **FR-WRITE-7.** `finish()` SHALL return the `ContentDigest` of the archive
  it just produced, computed identically to how `Cartridge::content_digest()`
  would compute it if the result were reopened. **This is the load-bearing
  requirement for milestone M0's reproducibility claim (component spec
  §8.1, point 3):** the digest algorithm SHALL be one implementation shared
  by reader and writer, not two independently maintained ones that happen
  to agree today.
- **FR-WRITE-8.** With `write_hashes = true` (the default), `finish()`
  SHALL emit `SQ-INF/hashes.json` containing a digest for every entry added,
  computed by the same per-entry digest function `content_digest()` uses to
  recompute — not a different one.
- **FR-WRITE-9.** A `Writer` SHALL be single-use: after `finish()` (an
  rvalue-qualified method) it is spent, and the type system SHALL make a
  second call impossible to express, not merely documented as an error.

### 3.12 Lua binding (`SQCART_ENABLE_LUA`)

- **FR-LUA-1.** `sqcart.open(path)` SHALL return `(cartridge, nil)` on
  success or `(nil, error_table)` on failure — the Lua idiom — never raise
  a Lua error for an ordinary cartridge-level failure.
- **FR-LUA-2.** `cartridge.manifest` SHALL expose a read-only Lua table
  built only from the boundary data types format spec §2.6.5 permits
  (strings, numbers, booleans, arrays, maps); it SHALL NOT expose a handle,
  pointer, or any value requiring native lifetime management on the Lua
  side.
- **FR-LUA-3.** `cartridge.manifest.consumers` SHALL be a map from consumer
  identity to that section's JSON **as a string**, mirroring the C++
  `Manifest::consumer()`. The binding SHALL NOT decode a section into a Lua
  table: doing so would require this library to decide how a consumer's
  JSON maps onto Lua values, which is a decision belonging to the consumer.
  A Lua caller that wants a table decodes the string with its own reader.

  This requirement is moot in practice — `sqcart::lua` is deprioritised
  indefinitely for want of a concrete consumer — and is recorded so that a
  future implementation does not reintroduce the coupling by reflex.
- **FR-LUA-4.** `error_table` SHALL contain `category`, `code`, `message`,
  `entry` (or `nil`), and `recoverable`, matching `Error` field-for-field,
  per format spec §2.6.12.
- **FR-LUA-5.** No function exposed by `sqcart.lua` SHALL scan a directory,
  select among multiple cartridges, or perform any operation named with the
  vocabulary `resolve`, `select`, `materialize`, `generate`, or `workflow`.
  `sqcart/AGENTS.md` additionally requires that no generator workflow
  module `require("sqcart")` at all; this requirement constrains what the
  binding may offer even to a caller that does.

### 3.13 Bootstrap CLI (`SQCART_ENABLE_BOOTSTRAP_CLI`)

- **FR-CLI-1.** The bootstrap CLI SHALL implement exactly three
  subcommands: `pack`, `list`, `verify`. It SHALL NOT grow a fourth without
  a revision to this document and to component spec §7.1, which fixes this
  limit deliberately to prevent a second, competing implementation of
  generator logic (Repository Conventions §1.7).
- **FR-CLI-2.** `pack <dir> -o <out.sq>` SHALL produce canonical output
  (`WriteOptions::canonical = true`) unless the user explicitly opts out.
- **FR-CLI-3.** `list <in.sq>` SHALL print one line per entry, in the order
  `entries()` returns, in a stable machine-parseable format (path,
  uncompressed size, compressed size, method), so that CI can parse it
  without a JSON dependency.
- **FR-CLI-4.** `verify <in.sq>` SHALL run `validate()` at strict
  conformance and exit non-zero if and only if `conforming` is `false`.
  Diagnostics SHALL be printed to stderr; stdout SHALL remain reserved for
  future machine-readable output.
- **FR-CLI-5.** The bootstrap CLI SHALL NOT read any environment variable
  to alter its behaviour beyond standard path variables (`HOME`, `TMPDIR`),
  and SHALL make no network connection under any invocation.

---

## 4. External interface requirements

The exact function signatures are normative in `sqcart/include/sqcart/
sqcart.hpp` and are not repeated here. This section states which interface
shape each requirement group implies, so the header and this document can
be checked against each other without one restating the other.

| Interface | Shape | FR groups |
|---|---|---|
| C++ read API | `Result<T>` return, move-only owning types, `span`-based zero-copy reads where the backend allows it | FR-OPEN, FR-PATH, FR-LIM, FR-MAN, FR-KIND, FR-ENTRY, FR-INT, FR-EXT, FR-VAL, FR-ERR |
| C++ write API | `Result<T>`, `Writer` move-only and rvalue-qualified `finish()` | FR-WRITE |
| Lua binding | value+error return idiom, boundary data types only | FR-LUA |
| CLI | POSIX-style subcommands, stdout/stderr separation, process exit code as the success signal | FR-CLI |
| Build interface | CMake options `SQCART_USE_EXTERNAL_MINIZ`, `SQCART_USE_EXTERNAL_YYJSON`, `SQCART_ENABLE_WRITER`, `SQCART_ENABLE_LUA`, `SQCART_ENABLE_BOOTSTRAP_CLI`, `SQCART_BUILD_TESTS` | FR-CON |

---

## 5. Non-functional requirements

### 5.1 Performance

- **FR-PERF-1.** Opening a cartridge for metadata inspection (manifest and
  entry listing) SHALL NOT require decompressing any payload entry.
- **FR-PERF-2.** Under the first-entry rule (FR-OPEN-3), manifest parsing
  SHALL be possible by reading only up to the end of the manifest entry —
  it SHALL NOT require seeking to the end of the archive to locate the
  central directory first. This is the concrete performance property format
  spec §3.2 exists to guarantee, relevant when the source is a slow
  sequential stream (an Android asset stream, a Termux pipe).

### 5.2 Portability

- **FR-PORT-1.** The `sqcart::read` target SHALL build and pass its tests
  under both GCC and Clang at the C++20 standard level, with and without
  `__cpp_lib_expected` available (see `expected.md`).
- **FR-PORT-2.** No path-handling code SHALL assume a case-sensitive host
  filesystem or a case-insensitive one; both SHALL be handled correctly
  per FR-PATH-2.

### 5.3 Reliability

- **FR-REL-1.** Given identical inputs (bytes and `OpenOptions`), every
  read-side operation SHALL produce identical results across runs and
  across host platforms.
- **FR-REL-2.** No operation SHALL leave a `Cartridge`, `Manifest`, or
  `Writer` in a state where a subsequent well-formed call produces
  undefined behaviour; a failed call SHALL leave the object usable for
  further calls or, where the API makes the object single-use (`Writer`),
  SHALL make further use a compile error.

### 5.4 Security

- **FR-SEC-1.** Path traversal defenses (§3.2) SHALL be applied at both
  `open()` and `extract_to()` independently (FR-EXT-1), not once and
  cached, so that no code path can reach a filesystem write from an
  unvalidated path.
- **FR-SEC-2.** A cartridge containing a prohibited entry per format spec
  §8 (`.so`, `.dll`, `.dylib`, a platform executable, a Lua C module) SHALL
  be rejected with `native_code_prohibited` at `open()` under strict
  conformance, before any entry is extracted.
- **FR-SEC-3.** No successful integrity check (§3.7) SHALL be reported, and
  no cartridge SHALL be described as "verified," on the basis of digest
  matching alone — signature verification is unimplemented in this version
  (format spec §10), and no function in this API SHALL imply otherwise
  through its naming or return value.

### 5.5 Maintainability

- **FR-MAINT-1.** `tools/sqcart-isolation.fish` run with no arguments SHALL
  pass against any commit that also passes this document's requirements;
  a failing isolation gate is a defect against FR-CON-1/2, regardless of
  which other requirement motivated the change that broke it.

### 5.6 Offline operation

- **FR-OFF-1.** No target defined by `sqcart/CMakeLists.txt`, and no
  operation in any target's runtime behaviour, SHALL make a network
  connection.

---

## 6. Use case scenarios

**UC-1 — Engine resolves a kit.** The Kit Service calls
`Cartridge::open("resources/cache/kit.sdl3-1.2.0.sq")`. sqcart runs
FR-OPEN-1 through FR-OPEN-5, returns a `Cartridge`, and the engine reads
`manifest().consumer("squared_pg")`, parsing `integration_areas` from it
(FR-MAN-6) to check
against other selected kits — a comparison sqcart does not perform itself
(FR-VAL-5).

**UC-2 — Framework runtime loads an application.** At startup, the runtime
calls `open()` on a `cartridge`-kind archive, reads `as_cartridge()->entry`
(FR-KIND-4, FR-LUA-3 equivalent in C++), and calls `extract_to()` into a
sandboxed run directory (FR-EXT-1 through FR-EXT-5). The runtime links only
`sqcart::read`; it never sees `Writer`.

**UC-3 — Packaging tool packs a template.** A CI job calls
`Writer::create()`, `set_manifest()`, `add_tree("tree/")` over an exploded
`template.android.cpp` directory, and `finish()`. FR-WRITE-4 rejects any
symlink the source tree happens to contain. The returned digest (FR-WRITE-7)
is recorded as a build artifact.

**UC-4 — Validator lints before publishing.** `sqcart-bootstrap verify
kit.sdl3.sq` runs `validate()` at strict conformance (FR-VAL-1, FR-CLI-4)
and exits non-zero on any `Severity::error` diagnostic, without needing a
generator present at all.

**UC-5 — M0 reproducibility check.** A cartridge produced by
`sqcart-bootstrap pack` and the same source tree produced by the generated
`sqcart` CLI (built by `squared-pg` itself, per the dogfood chain) are each
digested with `content_digest()`. FR-WRITE-7 and FR-INT-2 together
guarantee the two digests are equal despite the two binaries potentially
choosing different deflate implementations.

---

## 7. Data requirements

The manifest's structural shape is authoritative in
`docs/developer/specs/schema/sq-manifest-2.schema.json` (FR-MAN-10). This
document does not duplicate field-by-field constraints already expressed
there; where a functional requirement above depends on a specific field
(`KitBody::integration_areas`, `CartridgeBody::permissions`, and so on), the
schema is the source of truth for its type and cardinality, and this
document is the source of truth for what sqcart *does* in response.

---

## 8. Traceability matrix

| FR-ID(s) | Spec reference | Verifying test (existing / planned) |
|---|---|---|
| FR-OPEN-1..7 | format spec §3.2, §3.5, §12.1 | *planned:* `test_open.cpp` |
| FR-PATH-1..5 | format spec §3.3 | *planned:* `test_path.cpp` |
| FR-LIM-1..4 | format spec §3.4 | *planned:* `test_limits.cpp` |
| FR-MAN-1..10 | format spec §5 | *planned:* `test_manifest.cpp`; schema cross-check via `docs/.../schema/sq-manifest-2.schema.json` |
| FR-KIND-1..5 | *retired* — moved to the consuming project (§3.5) | n/a |
| FR-ENTRY-1..5 | format spec §3.1, §3.4 | *planned:* `test_entries.cpp` |
| FR-INT-1..6 | format spec §9 | *planned:* `test_digest.cpp` |
| FR-EXT-1..5 | format spec §3.1, §2.3.13 (engine transaction boundary) | *planned:* `test_extract.cpp` |
| FR-VAL-1..5 | format spec §12.1, §2.7.10/11 | *planned:* `test_validate.cpp` |
| FR-ERR-1..4 | format spec §13, §2.4.8, §2.6.12 | **existing:** `sqcart/tests/test_errors.cpp` |
| FR-WRITE-1..9 | format spec §9.3, component spec §8.1 (M0) | *planned:* `test_writer_canonical.cpp`, `test_writer_digest_parity.cpp` |
| FR-LUA-1..5 | format spec §2.6.5, §2.6.12 | *planned:* `test_lua_binding.lua` |
| FR-CLI-1..5 | component spec §7 | *planned:* `test_bootstrap_cli.fish` |
| FR-CON-1..5 | component spec §2, §4 | **existing:** `tools/sqcart-isolation.fish`; `sqcart/tests/test_expected.cpp` covers the C++20/23 half of FR-PORT-1 |
| FR-PERF-1..2 | format spec §3.2 rationale | *planned:* `test_streaming_open.cpp` (benchmark-style, pass/fail on a byte-read ceiling) |
| FR-SEC-1..3 | format spec §8, §10 | *planned:* `test_security.cpp` (malicious-archive fixtures) |

Rows marked *planned* are the test-suite backlog this document produces as
a side effect: implementing an FR without adding its listed test is an
incomplete change.

---

## 9. Non-goals (restated)

Signature verification, target overlays, a C ABI shim, and any form of
cross-cartridge resolution are explicitly not specified by any requirement
above. See component spec §12 and format spec §10–§11 for their reserved
status. A future revision of this document is required before any of them
becomes an FR.

---

## 10. Open issues

Points where this document makes a provisional call that should be
confirmed rather than assumed settled, in the same spirit as the format
spec's Appendix B.

1. **FR-INT-5 (vacuous success when `hashes.json` is absent).** The
   alternative is to make `verify_integrity()` fail when there is nothing
   to verify, forcing every caller wanting integrity assurance to check
   explicitly. The current call keeps the function's meaning literal
   ("nothing declared was contradicted") but risks a caller reading success
   as a stronger guarantee than it is. If this becomes a recurring source
   of confusion, a `requires_features: ["integrity.required"]` token is the
   more general fix, deferred rather than special-cased here.

2. **FR-ENTRY-1 (ordering guarantee).** Committing to central-directory
   order for archives and lexicographic order for exploded directories
   makes `entries()` deterministic, but the two orders differ from each
   other for the same logical cartridge in its two forms. If a consumer
   depends on `entries()` order matching between archived and exploded
   forms of the same cartridge, this requirement needs revisiting before
   it's load-bearing anywhere.

3. **FR-WRITE-4 (symlinks rejected, not resolved).** Refusing is the
   conservative choice for v1. A future version could resolve symlinks
   within the source tree automatically; that is a convenience feature,
   not a correctness one, and is deferred rather than decided against
   permanently.

4. **FR-CLI-3 (list output format).** "Machine-parseable" is asserted
   without pinning an exact column format. This should be nailed down
   (tab-separated, one record per line, is the likely choice) before
   `test_bootstrap_cli.fish` is written, since the test will otherwise be
   checking against whatever the first implementation happened to print.
