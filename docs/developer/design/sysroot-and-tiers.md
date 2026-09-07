# Design — sysroot, tiers, and conformance

Status: **proposal**. Nothing here is implemented. It exists to be argued with
before code is written, because two of the collisions below change what gets
built first.

Covers: the `~/sqsysroot` environment, sandbox/project tiers, promote and
demote, workspace conformance, and where git, linting and static analysis
belong.

---

## 1. The environment

```text
~/sqsysroot/
  squared/          squared-pg itself — the installed generator
  sandbox/          generated workspaces, by default
  projects/         promoted workspaces
```

### It must be configuration, not discovery

§2.7.2 forbids generation from depending on ambient state or the current
working directory. A sysroot is ambient by nature, so it has to arrive the same
way resource roots do: **explicitly, in `EngineConfig`**, resolved by the host
from `--sysroot` or `SQUARED_PG_SYSROOT`, and never guessed.

The engine may then *use* it. It must never *find* it.

### The default output path is a workflow decision

"Generated workspaces live in `sandbox/` by default" is policy, and §2.5.6 puts
policy in Lua. The workflow computes `output = $sysroot/sandbox/$name`, exactly
as it already computes `output = ./$name` today. The engine keeps supplying no
default for an identity-bearing input — consistent with D-042, where the same
reasoning removed the platform default.

This also keeps squared-pg usable without a sysroot at all, which matters: a
tool that requires a particular directory layout before it will run is a tool
people work around.

### Where sqpg lives

`~/sqsysroot/squared/` is just an installation prefix. `sqpg` already locates
its resources by walking up from its own executable and by checking
`<prefix>/share/squared-pg` (D-043), so this works today with no change. The
release bundle's `install.sh` takes a prefix argument already.

---

## 2. Tiers

| | `sandbox` | `project` |
|---|---|---|
| location | `$sysroot/sandbox/` | `$sysroot/projects/` |
| git | forbidden | required |
| `AGENTS.md` | restrictive | collaborative |
| agent boundary | may not leave the workspace, may not ask to | normal review rules |
| symlinks | must not escape the workspace | same |

The tier is a property of the workspace, so it belongs in
`.squared/metadata.json`:

```json
{ "record_version": 2, "tier": "sandbox", ... }
```

**That is a metadata schema change**, which makes Q-24 (metadata format
migration) live rather than hypothetical. A v1 record read by a v2 engine has
no tier; the honest reading is "unknown", not "sandbox", and `workspace.verify`
should say so rather than assume.

### What the engine can and cannot enforce

Most of the tier rules are **documentation**, and saying otherwise would be
dishonest:

- "agents may not leave the sandbox" is an `AGENTS.md` rule. No generator
  enforces what an agent does.
- "no symlinks escape" is a filesystem property the engine cannot prevent after
  generation.

What the engine *can* do is **detect** violations. That is not enforcement, and
it is worth more than it sounds: a rule nobody checks is a rule nobody follows.
Detection is §4's conformance report.

---

## 3. Promote and demote

### The collision

**The engine cannot mutate an existing workspace.** `project.generate` refuses
one, because regeneration needs the write-ahead journal of §2.15.9 and that is
deliberately unimplemented (D-033).

Promote and demote are, by definition, operations on an existing workspace. So
either they wait for the journal, or they need a narrower transactional path of
their own.

### The narrower path, and why it works

Promote and demote decompose into three kinds of change:

| Change | Safe today? |
|---|---|
| move the directory `sandbox/x` → `projects/x` | **yes** — same filesystem, one rename. Exactly D-024's argument, and the code exists. |
| add files that do not exist (`.gitignore`, `.git/`) | **yes** — no conflict is possible |
| replace a file the user may have edited (`AGENTS.md`) | **only with provenance** |

The third is the interesting one, and it is where this design earns its keep.

`.squared/metadata.json` already records a SHA-256 for every generated path.
Nothing reads it. Promote/demote would be its **first consumer**:

- hash matches the provenance record → the user never touched it → replace freely
- hash differs → the user edited it → refuse, or back it up and report

That is the same question regeneration has to answer, asked in a much smaller
setting. **So promote/demote is a good way to validate provenance before
building the journal on top of it** — which argues for doing it *before*
regeneration rather than after.

### Sketch

```text
project.promote   name | path
  1. verify the workspace (§4). Refuse if it does not conform.
  2. check provenance for every path the promotion would replace.
  3. stage additions in a sibling temp dir on the target filesystem.
  4. rename the workspace into projects/.
  5. apply additions; rewrite .squared/metadata.json with tier=project.
  6. git init is NOT run by the engine (see §5).

project.demote    name | path
  1. verify.
  2. refuse unless the working tree is clean — demotion discards history and
     the engine must not be the thing that loses someone's commits.
  3. archive .git/ to $sysroot/.cache/demoted/<name>-<date>.tar.gz, or refuse
     if that cannot be written. Never delete outright.
  4. rename into sandbox/, swap AGENTS.md, rewrite metadata with tier=sandbox.
```

Step 2 of demote matters more than the rest. "Stripped of git dir and files" is
destructive, and a generator that can silently destroy history is a generator
people stop trusting. Archive, refuse on dirty, never `rm -rf .git`.

### Open question

Does promote/demote move the directory, or operate in place? Moving makes the
tier legible from the path, which is the point of the layout. But a workspace
that has been `cd`-ed into, opened in an editor, or referenced by an absolute
path in someone's notes moves out from under all of them. Worth deciding
explicitly rather than by implementation accident.

---

## 4. Conformance

Two audiences, one invariant list.

### `test_templates` — development time

Iterates every indexed template, plans a workspace from it, and asserts the
shared conventions hold. Roughly 150 lines, no engine changes. Catches the
drift that a third template would otherwise introduce silently:

- declares a working directory, and it exists in the plan
- no `generated`-class path inside the working directory
- `Makefile` is `seeded`; every `mk/*` fragment is `generated`
- `sq_app/include` and `sq_app/src` are present
- no unsubstituted `{{` survives in any step's content
- every declared `integration_area` has an arity entry
- required kits resolve

This is the conformance test agreed in the previous discussion, and it is worth
doing whatever else happens.

### `workspace.verify` — user time

The reader's counterpart, and the thing `sqpg inspect` should surface:

- every provenance entry: present, and hash matches or is reported as
  user-modified
- no `generated` path inside the working directory
- files present that provenance does not know about, and are not under a
  user-owned path — reported as unknown, not as errors
- symlinks that resolve outside the workspace
- tier invariants: `sandbox` has no `.git`; `project` has one
- the recorded template and kits still resolve in the current index

### Two operations, one command

`workspace.inspect` stays a **pure read** — it returns the record and cannot
fail for reasons about the workspace's state. `workspace.verify` is a separate
operation returning a report with findings.

Keeping them apart matters because they have different failure modes: a
malformed record is an `inspect` failure, while a modified file is a `verify`
*finding*, not an error. Collapsing them would make "this workspace has three
findings" indistinguishable from "I could not read this workspace".

The CLI can present them as one thing:

```sh
sqpg inspect ~/sqsysroot/sandbox/myapp          # record + findings
sqpg inspect ~/sqsysroot/sandbox/myapp --json   # machine-readable
```

Which is exactly the requested use case, without conflating the operations.

---

## 5. Git, linting, static analysis

Tested against §2.2.5 — the engine provides capability, never policy — and
against a property worth defending: **the engine has no external runtime
dependency today.** Everything it needs is vendored and compiled in.

### Git: not an engine service

The engine should never execute `git`.

- It is an external binary whose behaviour varies by version and user config.
- Everything the generator needs *from* it is either policy ("init a repo on
  promote") or detectable without it (`.git` exists, `.gitignore` exists).
- Running it would give the engine its first external runtime dependency, and
  that property is worth more than the convenience.

So: the engine **reports facts** — `.git` present, tier mismatch — as
conformance findings. Lua runs git, in the `trusted` tier, as workflow policy.
`project.promote` returns "this workspace now expects a repository"; the
workflow runs `git init`.

### Linting and formatting: kit or plugin, not engine

`clang-format`, `clang-tidy`, `stylua` are external tools with per-project
configuration. The same argument applies, more strongly.

But there is a cheaper answer than a plugin system: **this is already a kit's
job.** A kit contributes files to a workspace, and a lint setup is files — a
`.clang-format` and a `mk/kit_lint.mk` contributing a `make lint` target. That
needs no engine feature at all, works today, and puts the tool configuration
next to the code it applies to.

`kit.lint` is a better first move than implementing §2.14.2's extension model.

### Static analysis: same, later

`scan-build` and friends are heavy, slow and toolchain-specific. Same
conclusion: a kit contributing a `make analyze` target. Worth doing after
`kit.lint` proves the shape.

### What *should* be an engine service

Conformance verification, because it needs the provenance table, the ownership
classes and the resource index — all engine-internal, none of it reachable from
outside.

---

## 6. Ordering

1. **`test_templates`** — no engine change, catches drift now.
2. **`workspace.verify`** — the invariant list from step 1, applied to a
   workspace. First real consumer of provenance.
3. **Sysroot as configuration** — `EngineConfig`, host resolution, workflow
   default. Small, and unblocks the rest.
4. **`project.promote` / `project.demote`** — validates provenance in a small
   setting before the journal is built on top of it.
5. **Regeneration and the journal** — by which point provenance is proven and
   the tier model is settled.
6. **`kit.lint`** — cheap, and answers the linting question without a plugin
   system.

Steps 1 and 2 are worth doing regardless of whether the sysroot design
survives review. Steps 3 and 4 need the spec written first — see
`spec-audit-02.md`, class L.
