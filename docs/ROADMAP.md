# Roadmap

Where squared-pg stands after the 0.1.0-alpha.1 work, and what the next moves
are in the order they make sense.

Status is honest rather than optimistic: everything listed as done has been run
on a device, and everything listed as missing is missing rather than partial.

---

## Where it stands

**Working, tested on Termux/aarch64 and Linux/x86_64.** Engine lifecycle,
service layer, operation registry, resource index over sqcart, plan production,
transactional new-workspace generation with provenance. The Lua control layer
and its generated binding. Two templates, four kits. Both build systems, seven
engine suites, an end-to-end smoke test, and now CI.

**The Android path is proven end to end**: generate → build → package → sign →
install → run, on a phone, with no Gradle and no Java.

---

## 0. Done since this was written

- **Tooling ported to bash** (D-044). `release.sh`, `clang-check.sh`,
  `lua-check.sh`. The fish versions had two bugs that only appeared when run,
  and could not be run where they were written. The bash form is testable by
  whoever writes it, which is the whole point.

## 1. Close the books on the alpha

*Small, and everything else is easier afterwards.*

- **Commit and tag.** `tools/release.fish tag`, then `publish`. CI drafts the
  release; nothing is public until the draft is published.
- **Watch the first CI run.** macOS has never been tested — the executable-path
  lookup has a Darwin branch nobody has executed. CI will say.

## 2. Pay down the specification debt

*This is the item most likely to be skipped and most expensive to skip.*

Sixteen decisions — **D-029 through D-044** — are recorded in
`docs/spec/_meta/Spec decisions.md`, and most of them end with "amend §X".
None of the amendments has been made. The specification and the implementation
now disagree in fifteen documented places, which means the spec has stopped
being the thing you can read to understand the system.

The largest is **D-030**: §2.8.1–2.8.3 describe a manifest envelope the engine
does not implement, because the engine reads cartridges instead. §2.8 needs
rewriting to say that generator resources *are* cartridges.

`docs/spec/_meta/spec-audit-01.md` was prepared for exactly this and has never
been run. It is a two-phase plan — read-only audit, then edit mode applying
only approved findings — and it predates all fifteen decisions.

**Open questions still unanswered:** Q-21 (template composition), Q-22 (build
emission model), Q-23 (framework acquisition), Q-24 (metadata migration),
Q-25 (invariant testability), Q-26 (sandboxed trust), Q-27 (identifier
grammar), Q-28 (framework version default), Q-29 (requiring a kit by
capability).

## 3. `kit.sfml`, then Q-23

*Every template declares `requires.framework: ">=1.0.0 <2.0.0"`. There is no
Squared framework, and nothing acquires one — but the order in which to fix
that is now clear.*

### What the framework is

From the project author, and worth recording because it settles the layering:

Squared is a cross-platform framework of **high-level abstractions**. Not a
thin portability shim over a backend — a stack:

```text
application code                     sq_app/
  └─ components                      text editor pane, debugger frontend,
     │                               spreadsheet, markdown presentation pane,
     │                               3D graphing pane
     └─ primitives                   windows, screens, toolbars
        └─ kit                       kit.sfml, kit.sdl3, kit.ncurses
           └─ platform               Android, Linux, Termux
```

Components compose primitives the way Java Beans compose widgets.
`squared-holodisk` — a fully virtual sqcart reader/writer that models a disk
drive behind a high-level file abstraction — is a framework module rather than
a generator concern.

Two consequences follow immediately.

**The framework sits above kits, not beside them.** A Squared GUI that can run
on SFML *or* SDL3 needs a backend to sit on, which is why `kit.sfml` has to
work before the framework can be designed against it. The author's instinct to
hold Q-23 until then is right, and this is the reason.

**So the framework is a package, not a vendored blob.** It is versioned,
shared across projects, and independent of any one template — which is exactly
what §2.7.6's package resource is for, and what makes
`requires.framework` mean something. Vendoring it per template would fork it
per project.

**And `squared-holodisk` is a second sqcart consumer.** The standing decision
was that sqcart stays in-tree until one appears, with `git subtree split` as
the path. One is now on the horizon.

### The order

1. **`kit.sfml`.** Also the first real test of `render.backend` — the
   single-arity conflict detection and the `__has_include` guard have one
   implementation behind them, and a design with one implementation is a
   hypothesis.
2. **Q-23 proper**, against a working backend: how a project acquires the
   framework offline, what `sq_framework/` looks like in a workspace, and
   whether the build emission model (Q-22) needs to change to link it.
3. **Package materialisation** (§2.7.6), which is currently unimplemented and
   has had nothing to materialise. The framework is its first real consumer.

## 4. Regeneration — the biggest missing engine capability

*A generator's second day is more important than its first.*

`project.generate` refuses an existing workspace. So today squared-pg can
create a project and can never touch it again: no adding a kit, no updating a
template, no fixing a generated build fragment. That is the capability people
actually want from a generator once they have used it once.

Nearly everything for it exists:

| Have | Need |
|---|---|
| provenance hashes on every generated path | the write-ahead journal (§2.15.9) |
| a reified plan — an update plan is the same type | per-file staging under `.squared/staging/` |
| ownership classes, enforced | crash recovery across the four journal states |
| metadata that reads back | the `damaged` terminal state (§2.15.13) |
| | workspace locking (§2.15.12) |

The reason it was deferred is still valid: a half-implemented journal corrupts
user work in exactly the cases the journal exists to protect. But it is the
right next *engine* project.

## 5. A third template, and a second rendering kit

*Both validate designs that currently have one implementation each.*

The vision names four project types: headless, terminal, ncurses TUI, and
Android C++. Two exist.

**`template.ncurses.cpp`** would force Q-21. Two templates already share
`sq_app/`, the Makefile-plus-fragment split, `.gitignore` and `.clang-format`,
and a fix applied to one does not reach the other. At three templates that
duplication stops being tolerable, which is exactly when composition becomes
worth designing rather than speculating about.

**`kit.sfml` or `kit.ncurses`** would be the first real test of
`render.backend`. The single-arity conflict detection, the `__has_include`
guard and the swappable-renderer design all have exactly one implementation
behind them, and a design with one implementation is a hypothesis.

## 6. Smaller, worth doing when convenient

- **Q-26, sandboxed workflows.** Currently *refused* rather than enforced,
  which is honest but not useful. Needs a restricted `_ENV`, a whitelisted
  `require`, and a host-provided I/O channel — which ripples, because
  `squaredpg.report` writes to `io.stderr` directly.
- **`sqpg doctor`.** The probe scripts report host capability for the *Android*
  toolchain; nothing reports it for the generator itself. A new user who hits a
  problem currently reads `BUILDING.md` and guesses.
- **Package and asset materialisation.** Blocked behind Q-23 in practice —
  there is nothing to materialise until the framework question is answered.
- **macOS.** Unverified until CI runs.
- **Release tarball size.** 26 MB, mostly `third-party/` documentation and test
  data that the build never touches. `export-ignore` in `.gitattributes` would
  slim it, but the existing `.gitattributes` protects byte-exact fixtures and
  is worth being careful around.

---

## Suggested order

1. Tag the alpha, watch CI. *(hours)*
2. **`kit.sfml`** — validates `render.backend`, and gives the framework a
   backend to be designed against.
3. Run the spec audit and apply D-029…D-044. *(a focused pass; cheap in code,
   expensive to postpone)*
4. **Q-23** against a working SFML backend: how a project acquires the
   framework offline, and what that makes of package materialisation.
5. Regeneration and the journal. *(the big engine project — and by then it
   knows what content model it is updating)*

Step 3 is worth slotting in wherever there is a gap; it does not depend on the
others and the debt compounds. Step 5 is what makes squared-pg a tool people
keep using rather than one they run once, and it is deliberately last because
building an update path before knowing what a framework looks like in a
workspace means building it twice.
