# Todo

Working list. Ordered by what unblocks what, not by size.

Status is honest: **done** means it has been run, on a device where that
matters. Nothing is listed done on the strength of having been written.

---

## Now

- [ ] **Commit the working tree.** 54+ uncommitted changes. `release.sh tag`
      refuses a dirty tree, so nothing else can proceed.
- [ ] **Tag `v0.1.0-alpha.1`** — `tools/release.sh tag`, then `publish`.
- [ ] **Watch the first CI run.** macOS has never been tested; the Darwin
      executable-path branch is written and unexecuted. Runner labels
      (`ubuntu-24.04-arm`, `macos-13`) may also need adjusting.

## Spec debt — `spec-audit-02.md`

Sixteen decisions (D-029…D-044) name a section to amend; none is applied.
This is transcription, not thinking, and it gets more expensive every week.

- [ ] **AUD-K-001** §2.8.1–2.8.3: resources are cartridges *(blocking)*
- [ ] **AUD-K-002** §2.7.11 / §2.15.9: regeneration is not implemented *(blocking)*
- [ ] **AUD-K-003** §2.14.4: sandboxed tier is refused, not enforced *(blocking)*
- [ ] **AUD-M-002** `Spec status`: show the pending amendments
- [ ] **AUD-M-003** open questions: Q-21 and Q-23 have moved since they were written
- [ ] **AUD-K-004 … K-016** the remaining thirteen
- [ ] Audit-01 findings still marked *awaiting decision* — check whether any
      remain untriaged

## Conformance

Agreed, and worth doing regardless of what happens to the sysroot design.

- [ ] **`test_templates`** — iterate every indexed template, assert the shared
      conventions. ~150 lines, no engine change. Catches the drift a third
      template would otherwise introduce silently.
- [ ] **`workspace.verify`** — the same invariants applied to a workspace, and
      the first real consumer of the provenance table. Separate operation from
      `workspace.inspect`, which stays a pure read; `sqpg inspect` presents both.

## Sysroot and tiers

Design proposal: `docs/developer/design/sysroot-and-tiers.md`. **Spec first**
(AUD-L-001…L-005), then code — or the after-the-fact-decision pattern repeats.

- [ ] Decide: does promote/demote **move** the workspace, or operate in place?
      Moving makes the tier legible from the path and moves the directory out
      from under anyone who has `cd`-ed into it.
- [ ] Sysroot as `EngineConfig`, resolved by the host from `--sysroot` or
      `SQUARED_PG_SYSROOT`. Configuration, never discovery (§2.7.2).
- [ ] Workflow default: `output = $sysroot/sandbox/$name`. Policy, so Lua.
- [ ] Metadata `record_version` 2 with `tier`. Makes Q-24 live.
- [ ] `project.promote` — rename, additive writes, provenance-checked replacement.
- [ ] `project.demote` — refuse on a dirty tree, **archive** `.git` rather than
      delete it. A generator that can silently destroy history is one people
      stop trusting.

## Framework — Q-23

Held until `kit.sfml` works, which is correct: a GUI that runs on SFML *or*
SDL3 needs a backend to be designed against.

- [ ] **`kit.sfml`** against the installed SFML 3.1.0 (Termux/X11). Also the
      first real test of `render.backend` — the single-arity detection and the
      `__has_include` guard have one implementation each and are therefore
      hypotheses.
- [ ] Decide first: A (add `render.backend` to the terminal template — cheap,
      but the naming mistake D-041 was about), B (a third
      `template.desktop.cpp`), or C (cross-build SFML for Android — weeks).
- [ ] `termux-x11-nightly` is available but **not installed** and `$DISPLAY` is
      empty. SFML will build; a window test needs X running. Decide whether the
      smoke test opens a window or stops at link.
- [ ] Then Q-23 proper: how a project acquires the framework offline, what
      `sq_framework/` looks like, and whether Q-22 (build emission) changes.
- [ ] **`squared-holodisk` is a second sqcart consumer.** The standing decision
      was in-tree until one appears, `git subtree split` as the path. Decide
      before holodisk starts, not after.

## Engine

- [ ] **Regeneration and the journal** (§2.15.9). The biggest missing
      capability — today a project can be created and never touched again.
      Deliberately after promote/demote, which validates provenance in a
      smaller setting first.
- [ ] Workspace locking (§2.15.12) — only meaningful once regeneration exists.
- [ ] Package and asset materialisation — blocked behind Q-23 in practice.
- [ ] Q-30: express "requires *a* kit providing X" rather than a named kit.
      `__has_include` covers it in generated source; whether resolution should
      also express it is open.

## Tooling and hygiene

- [ ] **`kit.lint`** — a kit contributing `.clang-format` and a `make lint`
      target. Answers the linting question without a plugin system, because a
      lint setup is files and files are what kits contribute.
- [ ] `sqpg doctor` — host capability for the *generator*. The probe scripts
      cover the Android toolchain; nothing covers this.
- [ ] Delete `tools/*.fish` once the bash ports have been used in anger
      (D-044). Separate, deliberate commit.
- [ ] Release tarball is 26 MB, mostly `third-party/` documentation and test
      data the build never touches. `export-ignore` would slim it, but the
      existing `.gitattributes` protects byte-exact fixtures — be careful.
- [ ] macOS: unverified until CI runs.

## Not doing, and why

- **Template composition (Q-21).** Measured: one file of twenty is duplicated
  between the two templates, and it is `.clang-format`. What they share is
  shape, not content, so a base template would hold almost nothing. The real
  risk is convention drift, and that is `test_templates`, not a feature.
  Revisit if two templates ever duplicate real content.
- **Git as an engine service.** The engine would gain its first external
  runtime dependency, for facts it can already detect (`.git` exists) and
  policy that belongs to Lua (init on promote).
- **A plugin system for linting.** `kit.lint` does the job with a mechanism
  that already exists.
