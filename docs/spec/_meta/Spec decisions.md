---
title: Spec decisions
tags: [spec, meta]
---

# Spec decision log

Back to [[Specification Index]]. Open items live in [[Spec open questions]].

Newest first. Every decision records the alternatives rejected.

---

## D-028 — Invariant register names a detection mechanism per invariant
**Q-25** · [[2.16 Architectural invariants]]

The invariant register (§2.16.2) gives every invariant a detection mechanism, so
an invariant without a test is a documented gap rather than a silent assumption.
The one known weak spot — I-10 "no undocumented global state" — is only partially
checkable mechanically; the shortfall is recorded in §2.16.3 as an open limitation
until a suitable static check is adopted.

Rejected: keeping I-10 aspirational with no stated detection mechanism (an
untestable guarantee is documentation, not a guarantee).

## D-020 — `merged` ownership class is reserved, not implemented in v1
**Q-20** · [[2.7 Project generation model]]

In-place region merging is where generators corrupt user work. v1 supports
`generated`, `seeded`, `user` and `metadata` only. Templates needing a file the
user edits *and* the generator maintains MUST emit a `generated` fragment
`include`d by a `seeded` file.

Rejected: marker-comment regions (fragile across formatters); structured-key
merging (viable later, needs a format-aware writer per build system).

## D-019 — No coroutines in v1
**Q-19** · [[2.4 Engine lifecycle]]

The execution model is synchronous. Async asset loading was the only candidate
and does not justify the complexity on the Termux target. Revisit only with a
measured need.

## D-018 — Generator-internal resources get their own namespace
**Q-18** · [[1.5 Resources]] · [[2.10 Assets and generator resources]]

`resources/generator/` holds resources `squared-pg` consumes for itself.
`resources/assets/` remains project-bound application assets only.

Rejected: dumping them at `resources/` root (indistinguishable from misc data);
a `_internal` prefix inside `assets/` (violates namespace distinctness).

## D-017 — Errors carry a coarse category and a stable specific code
**Q-17** · [[2.15 Error and transaction model]]

Two fields. `category` is a closed enum for control flow; `code` is a stable
dotted string for diagnosis and documentation, e.g.
`category: package`, `code: package.version.unsatisfiable`.

Rejected: single flat enum (either too coarse to diagnose or too large to switch
on).

## D-016 — Two-tier Lua trust model
**Q-16** · [[2.14 Extensibility model]]

Workflows are `trusted` (repository `lua/`, user config directory) or
`sandboxed` (anything else). Sandboxed workflows run without `os.execute`,
`package.loadlib`, `debug`, and raw `io` writes; they reach the filesystem only
through engine services.

Rejected: no sandbox (unsafe once workflows are shareable); sandbox everything
(breaks the CLI's own workflows).

## D-015 — Engine instances are single-threaded and workspace-exclusive
**Q-15** · [[2.4 Engine lifecycle]] · [[2.15 Error and transaction model]]

An engine instance has thread affinity to its creating thread and is not
thread-safe. Multiple instances MAY coexist in a process. One `lua_State` per
engine. One writer per workspace, enforced by an advisory lock. The job wrapper
MAY use a worker thread but only one operation executes per engine at a time.

Rejected: internally locked thread-safe engine (cost without a use case on the
target platform).

## D-014 — Engine domain knowledge is MUST, not MAY
**Q-14** · [[2.2 Engine responsibility and role]]

The contradicting MAY is deleted.

## D-013 — Lua filesystem prohibition scoped to generator-owned resources
**Q-13** · [[2.5 Lua control layer]] · [[2.6 Engine-Lua boundary]]

Lua MAY read user-space input (config files, streams, stdin). Lua MUST NOT touch
generator resources or mutate the workspace except through engine services.

## D-012 — Validation split: envelope vs type block
**Q-12** · [[2.4 Engine lifecycle]]

Resource Service validates manifest envelope well-formedness, identity syntax,
and integrity. The owning typed service validates the type-specific block.

## D-011 — Immutable resource index
**Q-11** · [[2.4 Engine lifecycle]]

Initialization builds an immutable identity→manifest index. Lazy loading may
only fetch payloads for already-indexed identities. The index MUST NOT change
during a session. This is what makes hybrid eager/lazy loading deterministic.

## D-010 — Transaction references corrected
**Q-10** · [[2.4 Engine lifecycle]]

Transactions are [[2.15 Error and transaction model]]. §2.14 is extensibility.

## D-009 — Typed operation registry is canonical
**Q-09** · [[2.4 Engine lifecycle]]

A compiled, typed C++ operation registry is the core dispatch. The Lua table
form `engine.execute{operation=...}` is binding sugar over it. Preserves
compile-time checking inside the engine.

Rejected: stringly-typed core (loses type checking on the engine's own internals).

## D-008 — Operations are documented compositions of services
**Q-08** · [[2.4 Engine lifecycle]]

No capability may exist only at the operation layer. Every operation is
expressible as a documented sequence of service calls, which keeps the service
layer the single source of truth and makes operations testable as compositions.

## D-007 — Synchronous core execution, optional job wrapper
**Q-07** · [[2.4 Engine lifecycle]]

The duplicated §2.4.6 is unified. The synchronous model is normative. The job
wrapper is optional, MUST NOT change the operation contract, and MUST produce
identical behaviour.

## D-006 — Resource *definition* split from resource *composition*
[[2.7 Project generation model]] · [[2.8 Templates]] · [[2.9 Kits]] · [[2.10 Assets and generator resources]] · [[2.11 Packages]]

§2.8–2.11 define what a resource *is* — manifest, identity, versioning,
discovery, validation. §2.7 defines how resources are *composed into a project*.

## D-006a — §2.12 states the guarantee, §2.7 owns the mechanism
[[2.12 Generated-project boundary]] · [[2.7 Project generation model]]

Ownership classes and provenance stay in §2.7.10. §2.12 becomes the short,
checkable statement of the boundary guarantee: what a generated project may
never contain and may never require.

## D-005 — Ownership classes and file provenance
[[2.7 Project generation model]]

Five classes plus a provenance table recording path, class, originating resource
and version, and content hash. Promotes workspace metadata from SHOULD to MUST.

Rejected: timestamp comparison (unreliable across VCS and copies); never
overwriting anything (makes updates impossible).

## D-004 — Undeclared ownership class defaults to `seeded`
[[2.7 Project generation model]]

The safe default is never to overwrite.

## D-003 — Generation plan is reified and shared with execution
[[2.7 Project generation model]]

Plan and apply consume the same command list, so they cannot diverge.

## D-002 — Determinism split into resolution vs output
[[Spec conventions]]

## D-001 — Single declared user working directory
[[2.7 Project generation model]] · [[2.8 Templates]]

Reference convention `sq_app/`. No `generated`-class file may be written into it.
Derived directly from the project vision.

---

## Foundational decisions recorded during the completion pass

## D-021 — Manifests are JSON
**Q-03** · [[2.8 Templates]] and siblings

`manifest.json` at each resource root, parsed with the vendored yyjson. JSON is
already a build dependency, is offline, has an unambiguous grammar, and needs no
new vendored parser.

Rejected: Lua tables as manifests (executable data, defeats sandboxing and static
validation); TOML/YAML (new vendored dependency for no gain).

## D-022 — Identifier and version grammar
**Q-04** · [[2.8 Templates]] · [[2.11 Packages]]

Identifiers are `<type>.<segment>(.<segment>)*`, segments matching
`[a-z][a-z0-9]*(-[a-z0-9]+)*`. Versions are SemVer 2.0.0. Reference syntax is
`identity@constraint`. Selection takes the highest satisfying version.

## D-023 — Engine capability tokens
**Q-05** · [[2.14 Extensibility model]]

Named, SemVer-versioned, compiled-in, enumerable. Manifests declare
`requires_capabilities`. Unsatisfied requirements fail at resolution, never at
materialization.

## D-024 — Same-filesystem staging with a write-ahead journal
**Q-06** · [[2.15 Error and transaction model]]

Staging always occurs on the target filesystem, so cross-device rename cannot
arise. New projects build in a sibling temp directory and land with one rename.
Regeneration stages per file under `.squared/staging/` and applies through a
journal that is written and flushed before the apply phase begins. An interrupted
transaction is detected and resolved on next open.

Rejected: copy-then-delete (not atomic); `/tmp` staging (cross-device on Android).

## D-025 — The C++ public API is the boundary; Lua is its first consumer
**Q-01** · [[2.1 Architectural model]] · [[2.5 Lua control layer]]

Embedders MAY call the engine C++ API directly or host the Lua layer. The rule
formerly in §2.5.4 is restated as: the CLI MUST NOT implement generation policy
in native code.

## D-026 — Authoritative dependency direction
**Q-02** · [[2.13 Dependency graph]]

`third-party/` ← `engine/` ← `lua/` ← hosts. `resources/` is inert data with no
outgoing dependencies, reached only through the Resource Service. Generated
projects are sinks. Unblocks [[1.11 Dependency direction]]. (Directory naming
follows the repository's current `third-party/`; see [[1.6 Third-party dependencies]].)

## D-027 — §2.3 is conceptual, §2.7.9 sets the reference phase order
[[2.3 Generation pipeline]] · [[2.7 Project generation model]]

§2.3 describes the workflow-facing pipeline. §2.7.9 sets the *reference* phase
order, which a workflow SHOULD follow (Lua may legitimately reorder within the
resolve-before-mutate and transaction constraints). §2.3 links rather than
restates.

---

## Decisions recorded during the first implementation pass

> [!note] Stance
> These record where the implementation diverged from Item 2 as written. Per the
> repository `AGENTS.md`, the repository is ground truth during initial
> implementation and the specification is amended to match. Each entry names the
> sections that need amending.

## D-029 — The CLI host lives at `app/`; the Lua binding at `engine/lua/`
[[1.1 Repository identity]] · [[2.1 Architectural model]] · [[2.6 Engine-Lua boundary]]

§1.1 divides the repository into `engine/`, `lua/`, `third-party/`, `tools/` and
`docs/`, which leaves the CLI host and the binding layer without a home.

- **`app/`** — the `sqpg` host. It is a *consumer* of the engine (§2.1.6), not
  part of it; placing it under `engine/` would blur the line §2.2.5 draws.
- **`engine/lua/`** — the binding layer. §2.6.3 requires it to be generated from
  or validated against the operation registry, which makes it engine-side. It
  builds as a separate target so an embedder may link the engine alone and never
  pay for a Lua runtime.

Rejected: putting the binding in `lua/`, which is Lua source by §1.4 and would
have put C++ there; putting the CLI in `engine/`, which §2.2.5 forbids in spirit.

Amend §1.1 to list both.

## D-030 — Resources are cartridges; the manifest envelope is the cartridge format's
[[2.8 Templates]] · [[2.10 Assets and generator resources]]

§2.8.3 defines an envelope (`schema_version`, `type`, `requires_engine`,
`requires_capabilities`) and §2.8.1 an identifier grammar
(`[a-z][a-z0-9]*(-[a-z0-9]+)*`, unbounded segments). The repository already
contains manifests in the *cartridge* format (`format_version`, `kind`,
`engine`, `requires_features`; segments `[a-z][a-z0-9_]*`, two to eight), a
tested reader for them, and a CLI that writes them.

The engine implements the cartridge form and opens every resource through
`sqcart`. `ResourceId::parse` accepts both `_` and `-` as segment separators so
that identities written against either document validate.

Rationale: a second manifest reader would be a second chance to disagree about
what a manifest means, and disagreement between the tool that packs cartridges
and the tool that consumes them has the worst possible symptoms.

Rejected: implementing §2.8.3 alongside the cartridge envelope (two readers);
rewriting sqcart to §2.8.3 (discards a tested, shipped implementation).

Amend §2.8.1–2.8.3 to state that generator resources are cartridges and inherit
the container format's envelope and identity grammar.

## D-031 — Resource payload root
[[2.8 Templates]] · [[2.9 Kits]] · [[2.10 Assets and generator resources]]

Templates declare their payload root as `template.tree`. The cartridge format
gives kits, packages and asset bundles no equivalent field, so the engine's
convention is: `tree/` when the cartridge has entries beneath it, the cartridge
root otherwise. `SQ-INF/` is never payload.

Amend §2.9 and §2.10 to state the convention, or add a `tree` field to those
bodies in the format.

## D-032 — Architecture fields absent from the cartridge format are read from raw JSON
[[2.7 Project generation model]] · [[2.8 Templates]] · [[2.9 Kits]]

The generator architecture needs manifest fields the container format does not
model: `working_directory`, `integration_areas`, `integration_arity`,
`processor`, parameter `default`/`default_from`, `executable`,
`external.acquisition`, `conflicts_with`.

These are read from `Manifest::raw_json()`, which format §5.4 guarantees is
preserved verbatim. This keeps the divergence one-directional: the engine knows
more than the format, and the format never learns about generation — which is
the scope rule `sqcart/AGENTS.md` already states.

Where such a field is absent or malformed the engine falls back to a documented
conservative default (`working_directory` → `sq_app`, `processor` →
`substitute`, arity → `single`). An *unknown* processor is a hard
`capability.unsatisfied` failure, because §2.14.1 requires an unsatisfied
capability to fail at resolution.

## D-033 — v1 implements new-workspace generation only
[[2.7 Project generation model]] · [[2.15 Error and transaction model]]

`project.generate` refuses an existing workspace with
`filesystem.workspace.exists`. Regeneration (§2.7.11) requires the write-ahead
journal (§2.15.9), per-file staging, crash recovery across the four journal
states, and the `damaged` terminal state (§2.15.13).

Refusing loudly is the correct failure: a half-implemented journal would corrupt
user work in exactly the situations the journal exists to protect.

Already in place for it: provenance hashes written on every generation, a
reified plan (the same type an update plan needs), and enforced ownership
classes. Missing: the journal and the apply-in-place loop.

Package and asset *materialization* are deferred on the same principle —
resolution is implemented and reported, materialization is not, and there is no
package or asset in the repository to design it against.

## D-034 — The engine supplies built-in substitution parameters
[[2.7 Project generation model]] · [[2.8 Templates]]

The template parameter contract is validated against the **effective**
parameters: the template's declared defaults, then engine built-ins derived from
the request, then the workflow's values.

Built-ins: `project_name`, `namespace`, `working_directory`,
`generator_version`, `template_id`, `template_version`, `framework_version`,
`kit_list`. Every one derives from the request or the engine — never from the
environment or the clock, because §2.7.13 requires byte-identical output from
equivalent inputs.

Without this, every workflow would have to restate `project_name = name` to pass
a check the engine was about to satisfy itself. §2.8.7 should say that the
engine contributes built-ins and that validation sees the effective set.

## D-035 — `requires_capabilities` is an opaque array in the cartridge envelope
[[2.8 Templates]] · [[2.14 Extensibility model]] · supersedes part of D-030

§2.14.1 requires an unsatisfied capability to fail at resolution. Until now a
manifest had nowhere to state one: the capability registry was enumerable and
undemandable, and eight of the nine tokens could not be required by anything.

The cartridge format gains `requires_capabilities`: an array of opaque tokens
in the envelope, beside `requires_features`. sqcart validates **shape only** —
`name` or `name@range`, dotted lowercase, no duplicates — and exposes it as
`Manifest::requires_capabilities()`. It never interprets a token and must not:
a reader that vetted the vocabulary would need teaching every consumer's, and
every new generator capability would become a sqcart release.

The parallel with `requires_features` is deliberate, and so is the distinction:

| | Guards | Fails at | Fixed by |
|---|---|---|---|
| `requires_features` | the reader | open | a newer sqcart |
| `requires_capabilities` | the consumer | resolution | a newer consumer |

The precedent is a jar manifest's vendor-specific attribute sections: the
archiver carries them and never reads them, which is what keeps it a general
tool. This is the mechanism that lets sqcart stay one.

Rejected: adding a generator-specific `requires_capabilities` field that sqcart
*understands* (puts generator vocabulary in a general format); reading it from
raw JSON as D-032 does for the structural fields (a mistyped token would
silently pass, and silent passing is the worst failure mode for a mechanism
whose entire purpose is failing loudly).

Note the limit: this shape works because a capability requirement is a flat
list of strings. It does **not** generalise to the D-032 fields, two of which
are maps and one of which carries arbitrary JSON values. Those stay in the kind
body, read from raw JSON. Validating them, if it becomes worth doing, is a
generator-side JSON Schema — generator semantics checked by the generator.

## D-036 — Version range operands may be partial
[[2.8 Templates]]

`^1.0`, `>=0.1` and `~1.2` now parse, padding to `^1.0.0`, `>=0.1.0`, `~1.2.0`.
A manifest's own `version` field is **not** padded: `"version": "1.0"` remains
a defect.

This was a latent bug, not a feature request. Callers write
`if (range && range->satisfied_by(v))`, so an unparsed range meant the
constraint *silently never applied* — and the repository's own manifests wrote
`requires_engine: "^0.1"`, so the engine-compatibility warning had never once
fired. A compatibility check that quietly does nothing is worse than one that
errors, because nothing signals it.

## D-037 — NDK detection belongs to the template, not the rendering kit
[[2.8 Templates]] · [[2.9 Kits]]

`template.android.cpp` locates the NDK, exports `SQ_NDK_INC` / `SQ_NDK_LIB`,
and compiles `android_native_app_glue.c` from it. `kit.opengl` consumes those
and adds only `-lEGL -lGLESv3` plus its own header.

An earlier draft put detection in the kit. That is wrong for a reason worth
recording: the template requires **no** rendering kit — that is what makes
`kit.opengl` and a future `kit.sfml` alternatives rather than one being baked
in — so a project with no kit would have had no glue and could not build.
NativeActivity is the template's architecture; EGL is the kit's contribution
on top of it.

The general rule: an integration point's *substrate* belongs to whoever cannot
be absent. A second rendering kit inherits a working NativeActivity build
rather than re-solving detection, and there is one copy of the detection logic
rather than one per kit.

## D-038 — Declared parameter defaults are applied after engine built-ins
[[2.8 Templates]] · amends D-034

`effective_parameters` layers workflow values, then engine built-ins, then the
template's declared defaults — each put-if-absent, so an earlier layer still
wins.

Built-ins first is what lets a template write `"default_from": "project_name"`,
where `project_name` is engine-supplied rather than workflow-supplied.
`template.android.cpp` uses exactly that for `app_label`, and with the previous
order the reference resolved against nothing and substitution failed.

A template default for a name the engine also provides is now redundant rather
than in conflict, which is the right relationship: the engine's built-ins are
facts about the request, and a template cannot know better.

## D-039 — SDK levels are build settings, not generation parameters
[[2.7 Project generation model]]

`AndroidManifest.xml` carries no `<uses-sdk>`. `minSdkVersion`,
`targetSdkVersion` and the ABI are make variables in
`mk/squared_generated.mk`, passed to `aapt2 link` at package time.

Baking them at generation time would mean regenerating a project to retarget
it, which §2.7.11's absence makes worse than it sounds: with no in-place
update, regenerating means losing the workspace. Declaring them in both places
would create two sources of truth, and aapt2 takes the manifest's.

The general test: if a value can change over a project's life without changing
what the project *is*, it belongs in the build, not in the generated content.

## D-040 — A kit adds an external sysroot with `-idirafter`, never `-I`
[[2.9 Kits]]

`kit.opengl` reaches the NDK's Khronos headers with `-idirafter $(SQ_NDK_INC)`,
and names `libEGL.so` and `libGLESv3.so` by absolute path rather than as `-l`
names with `-L`.

Both are corrections made after the first on-device build failed.

**Headers.** Termux's clang ships a complete sysroot including every
`android/*` header, so the NDK is needed only for what Termux lacks. Adding it
with `-I` puts a second complete set of C headers *ahead of libc++*, and
`<cctype>` finds the NDK's `ctype.h` instead of libc++'s wrapper — libc++
detects the mismatch and stops the build. `-idirafter` appends to the end of
the search path, so an external sysroot supplies only headers nothing else
provides.

**Libraries.** `-L` fixes the libglvnd shadowing problem but creates a second
one: it also places the NDK's `libc`, `libm` and `libdl` stubs ahead of the
host's for every implicit `-l` the driver adds. Naming the two files outright
avoids both. A search order cannot go wrong when there is no search.

Generalises to every kit whose external dependency ships its own sysroot —
which is most of them, since that is what `acquisition: "system"` usually
means. A kit contributes *additions* to a host toolchain and must not
reorganise it.

## D-041 — A template's identity names what it produces, not where it runs
[[2.8 Templates]] · renames `template.termux.cpp`

`template.termux.cpp` becomes `template.terminal.cpp`, with
`platforms: ["termux", "linux", "macos", "desktop"]`.

The template was never Termux-specific — it is portable C++20 and `make`, and
it builds and runs unchanged on desktop Linux. Naming it for the platform it
was first written on made `sqpg list` tell a Debian user the tool was not for
them, which is an adoption failure caused entirely by a string.

The rule: **an identity names the artifact; a `platforms` list names the
hosts.** `template.android.cpp` keeps its name because Android is what it
produces, not merely where it was authored.

Renaming a resource identity is a breaking change. Done now, at 0.1.0 with one
consumer, because it only gets more expensive.

## D-042 — The workflow assumes no target platform
[[2.5 Lua control layer]] · [[2.7 Project generation model]]

`workflow.generate.default` no longer defaults `platforms` to `["termux"]`.
An empty platform set means "no restriction", so compatibility checks admit any
resource rather than filtering against a guess.

Detection from the host was considered and rejected. The host a project is
*generated* on is not necessarily the host it *targets* — `template.android.cpp`
is generated on Termux and targets Android — so `uname` would be wrong in
exactly the case that matters. A workflow that wants filtering says so.

## D-043 — The installation root is found by marker, not by directory name
[[2.1 Architectural model]]

`sqpg` locates its resources and workflows by walking upward from its own
executable until it finds a directory containing both `lua/workflows` and
`resources`, then falling back to `<prefix>/share/squared-pg`.

The previous version special-cased the directory names `build` and `bin`. That
meant `cmake -B build-cmake` — or CLion's default `cmake-build-debug`, or any
other name — produced a binary that could not find its own workflows. It was
found by the CMake build's smoke test failing while every other test passed,
which is the whole reason for having a second build path.

Recognising the thing being looked for survives a build directory called
anything. Executable location now also handles macOS (`_NSGetExecutablePath`)
and the BSDs (`KERN_PROC_PATHNAME`); `argv[0]` is the last resort rather than
the first, since for a binary on `PATH` it is a bare name that resolves to
nothing.

## D-044 — Repository tooling is bash; fish is an optional translation
[[1.7 Tools]]

`tools/*.fish` becomes `tools/*.sh`, written in bash 3.2-compatible form.
§1.7's preference for fish is amended: fish may be provided *alongside* a bash
script for readability, but the bash form is canonical and is what CI runs.

The reason is not taste. Two real bugs shipped in `tools/release.fish` because
neither the author nor CI could execute it:

- `version` is read-only in fish — it holds fish's own version — so
  `--argument-names root version` is a parse-time error in every function
  declaring it.
- fish does not expose a caller's local variables to the functions it calls,
  unlike bash, so a top-level `set --local root` was invisible inside every
  function and `$root` expanded to nothing.

Neither is exotic. Both would have been caught by running the script once. The
problem was that the script could not be run where it was written, and a
release script that has never been executed is a release script that does not
work — which is exactly what happened, twice.

Bash is present on every target host: Termux, Linux, macOS. The 3.2
compatibility constraint is real and comes from macOS, which still ships that
version and is in the CI matrix: no associative arrays, no `mapfile`, no
`${var,,}`.

`tools/fish-lint.py` stays. Any fish that is written should still be checked,
and it catches both classes above — including the scoping one, which no parser
can see because it is not a syntax error.

The installer inside a release artifact remains POSIX `sh`: it runs on a
stranger's machine, where every additional requirement is one more thing that
can be missing.

---

## D-045 — Workflow reporting routes through a host channel

[[2.14 Extensibility model]]

`squaredpg.report` writes to a host-provided channel rather than to
`io.stderr` directly.

Raised while resolving AUD-K-003. Enforcing the `sandboxed` trust tier would
mean removing `io` from a workflow's environment, which would break reporting
for *every* workflow, not only sandboxed ones. Routing now costs almost
nothing and removes the hardest dependency from work that may never happen.

It buys more than that. A channel is capturable, which makes report assertions
possible without scraping stderr, and it is what structured `--json` output
needs regardless.

---

## D-046 — A workspace's tier is its location, not a metadata field

[[2.7 Project generation model]]

`~/sqsysroot/sandbox/x` is in the sandbox because of where it is. Promotion is
a rename.

The alternative — a `tier` field with the directories as convention — makes
promotion a one-line metadata edit, which is suspiciously cheap for something
that changes what an agent may do. It also permits the layout to lie: a field
can disagree with the filesystem, and a directory cannot disagree with itself.

The cost is that a workspace moves out from under an open editor. Once per
project, on a deliberate act.

---

## D-047 — The workflow verbs are drivers, not implementations

[[2.5 Lua control layer]]

`format`, `lint`, `check`, `test`, `docs`, `dist` find the workspace and run
`make <verb>`. `sqpg lint` does not know what a linter is.

Three properties follow, and they are the reason:

- the engine keeps its no-external-runtime-dependency property, since a
  driver's whole job is executing other people's tools;
- a generated workspace stays independent of squared-pg — `make lint` works
  with no `sqpg` installed, which is §2.7.11's invariant;
- the vocabulary is fixed while the implementation is not.

`--explain` prints the command instead of running it, which is what stops the
layer becoming a black box experts route around.

**No `profile` and no `debug`.** They are interactive and session-shaped, and
a vocabulary of things that run unattended and exit with a status is the wrong
container for them.

This is the decision with the longest reach in the set. Reversing it later
means moving six commands.

---

## D-048 — `quarantine` copies; `promote` and `demote` move

[[2.7 Project generation model]]

Asymmetric on purpose. Promotion and demotion act on a tree the environment
already owns, so a rename is right and reversible. Quarantine acts on someone
else's tree — a clone the author may still want where it is — and moving it
would be this tool deciding its environment outranks the rest of the
filesystem.

The asymmetry is arguable and a `--move` flag would be easy to add.

---

## D-049 — The default workflow is `workflow.sqpg.default`

[[2.5 Lua control layer]]

Renamed from `workflow.generate.default`. It is no longer generation-only: it
carries the environment lifecycle and the workflow verbs as well.

A name that lies is the thing the corpus argument is against. The cost is
anyone with the old id in a script, which in alpha is nobody.

---

## D-050 — Demotion preserves history; promotion removes the sandbox boundary

[[2.7 Project generation model]]

Not symmetric, and the asymmetry is the point.

Promotion deletes the sandbox `AGENTS.md` because squared-pg wrote it and it
would now be false. Demotion refuses to delete `.git` because the author's
history lives in it, and removing the only copy to satisfy a tier rule would
be the worst thing this tool could do.

The rule: this tool may delete what it wrote and what has become untrue. It
may not delete what it did not write.

---

## D-051 — Ownership uses an explicit `default`, not glob specificity

[[2.7 Project generation model]] · [[2.8 Templates]]

At most one ownership list may match a path; an unmatched path takes
`ownership.default`, which is `seeded` when absent.

**This replaced working behaviour.** `classify_path` had resolved overlaps
with a specificity heuristic — +2 per literal character, −4 per `*`, −8 per
`**` — and it was correct: every shipped template classified identically under
it, and `user: ["**"]` behaved exactly as `default: "user"` does now. What
AUD-L-004 found was not a broken template but two implementations disagreeing,
`sqcart` enforcing exactly-one while the engine scored patterns.

It was replaced because nobody can predict a scoring table's outcome without
consulting it. Whether `mk/**` outranks `**/*.mk` is a question you compute —
it does, −2 against −4 — and ownership is a thing template authors meet on
their first day.

What was given up: wildcard carve-outs. `generated: ["mk/**"]` with
`user: ["mk/local.mk"]` worked under specificity; it is now an overlap
warning, and the author writes non-overlapping patterns instead.

The manifest's vocabulary is not the engine's: manifest `user` maps to
`OwnershipClass::seeded`, because a file the generator is materialising cannot
be `user`. `ownership_class_from_manifest` is the one place that translation
happens.

---

## D-052 — `workspace.verify` reports; it never repairs

[[2.16 Architectural invariants]]

The first reader of the provenance table, which every generation had written
and nothing had ever read.

It does not restore files. A verify that quietly repaired would be the
regeneration path §2.7.11 defers, arriving by a side door — and the value of
the report is that the author decides.

Findings exit non-zero so it composes into a `check` target.

---

## D-053 — Consumer schemas are normative in the consuming project's spec

[[2.8 Templates]] · [[2.9 Kits]] · [[2.14 Extensibility model]]

Cartridge format 2.0 moved every generator concept into
`consumers.squared_pg`, which `sqcart` carries as opaque text. That was right
and it left the schema homeless: the only definition of what a template or kit
could declare was the engine's parser.

§2.8.6 and §2.9.2 are now that definition — every member, its type, its
default when absent, and which code reads it.

Two rules that existed only in code and are now written down:

- **A malformed member is treated as absent** and takes its default. Not an
  error, because §5.6 means nothing validates the section, and a generator
  refusing an unrecognised value could not open a cartridge written for a
  newer version of itself.
- **`processor` is the exception** and fails with `capability.unsatisfied`.
  The other members describe *what* to generate; `processor` describes *how*
  to transform content, and the wrong transform produces a workspace that
  looks correct and is not.

Fields read by nothing are listed as reserved rather than omitted, because an
author copying a shipped manifest will copy them: `project_types`,
`requires.kits.optional`, `runtime.model`.

---

## D-054 — The environment is control-layer configuration, never engine discovery

[[2.7 Project generation model]]

An environment (`~/sqsysroot`) is resolved by the Lua control layer, from
`$SQSYSROOT` or `$HOME`, and passed to the engine as explicit absolute paths.

§2.7.2 forbids ambient inputs to generation, and a discovered root is ambient.
The prohibition survives because of *where* the lookup happens: the engine has
no concept of an environment and cannot be made to acquire one. `sqpg promote
demo` may resolve `demo` against a discovered root; `project.generate` receives
a path and nothing else.

The tier invariant is stated as **nothing in sandbox is irreplaceable**, and
every other rule derives from it. Stating a consequence — "sandbox has no
git" — invites relitigating each consequence separately.

**Divergence from the design proposal:** demotion moves a dirty tree and warns
rather than refusing. Refusing would make `git` a hard dependency of the tier
system and would refuse most sandbox trees, which have no repository to be
dirty, while protecting against a loss that cannot occur — the rename is
reversible and the working tree is carried intact.

---

## D-055 — `.sqpg` for the tool; `.squared` means a workspace record

[[2.7 Project generation model]] · [[2.1 Architectural model]]

The environment's tool directory is `~/sqsysroot/.sqpg/`. A workspace's
provenance record stays at `<workspace>/.squared/metadata.json`.

The two had the same name, and the collision produced a defect rather than
merely confusion. `env.find_workspace` tested for `.squared-pg` — a directory
that has never existed — because the name `.squared` was visibly taken by the
tool directory when that code was written. The check never once matched, and
every workspace found came through a fallback that tested for a Makefile
beside an `mk/` directory. That matches any C project laid out that way and
misses a real workspace without one; it worked by coincidence.

**A workspace is now recognised by its record and by nothing else.**

Three things ride along, all from the same root:

- **`sqpg initialize` installs `sqcart`.** It copied the binary, the
  workflows and the resources, and omitted the one tool an authoring
  workspace's `check` and `dist` targets shell out to — so the second entry in
  the generated Makefile's lookup path could never succeed and every author
  set `$SQCART` by hand. Absence is reported, not fatal: a squared-pg without
  sqcart is still a working generator.
- **The top-level build produces `sqcart`.** It compiled sqcart's sources into
  the engine library but never linked its CLI, so a fresh clone could not
  author a cartridge and nothing said why. A sub-make, not duplicated link
  rules: sqcart is self-contained and vendoring its link line would mean two
  places to change when it gains a source file.
- **Generation refuses a target inside an existing workspace.** The way in is
  `sqpg new x` from inside a workspace, where `-o` defaults to `./x` and
  nothing looked upward. The result could not be promoted and sat inside the
  outer workspace's provenance scope. Every ancestor is checked, not only the
  parent.

Migration is `rm -rf ~/sqsysroot/.squared && sqpg initialize`.

---

## D-056 — `selfdestruct` competes with `rm -rf`; it does not replace it

[[2.7 Project generation model]]

A command that deletes a workspace, a tier, or the whole environment, behind
two confirmations.

**It closes no hole.** `rm -rf ~/sqsysroot` remains one tab-completion away,
and that is how a previous project was lost. A safe command only helps if it
is the one reached for, so the design goal is not "make deletion hard" — it is
**make deletion worth doing here**.

The inventory is therefore the point, not the ceremony. Before anything is
removed it reports, for every repository in scope, whether it has uncommitted
changes, unpushed commits, or no remote at all — and says plainly that this is
the part `rm -rf` would not have told you. That is a reason to type the longer
command.

Two factors, neither producible by accident:

- **A derived token.** The first invocation prints the inventory and a token
  computed from it. The second must supply it. Derived rather than stored, so
  there is no state to forge or go stale, and it stops matching when the
  inventory changes — a command line copied from an earlier session will not
  run against a tree that has moved on.
- **A typed phrase on stdin.** No shell completion produces
  `destroy the sandbox`.

There is no `--yes`, no `--force`, no environment variable. A bypass would be
used, and then this is `rm` with extra steps.

Non-interactive use is allowed: a pipe may supply the phrase. The protection
is not that a human is present but that neither factor arrives by accident.

**Scope of the token, stated because the first draft overclaimed it.** The
token certifies that *the inventory you were shown is still true*. It changes
when a workspace appears or vanishes, a file count moves, or a repository
becomes dirty or gains a remote. It does not change when a file's contents
change without moving any of those — "65 files, uncommitted changes, no
remote" is still true after another line is added to one of them. The
user-facing text says exactly this, because the first version said "the moment
any of it changes", which the implementation could not keep.

Emptying a tier recreates it. An environment without its tiers is broken and
the next `sqpg new` would fail on a missing directory. `--environment` does
not recreate `.sqpg`: restoring it would mean copying back a binary the caller
just asked to delete, and it is one `sqpg initialize` away.
