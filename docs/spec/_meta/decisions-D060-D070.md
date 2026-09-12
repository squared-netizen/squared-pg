---
title: Spec decisions — D-060 to D-070 (pending merge)
tags: [spec, meta, pending]
---

# Pending decision entries

**These are not yet in [[Spec decisions]].** Append them there, matching the
existing format, then delete this file.

## Numbering

The log runs D-001 to D-059 with **D-057 absent** — an unexplained gap, left
alone rather than filled, since a reused number is worse than a missing one.

**D-060 is written to match an existing citation.** `tools/bootstrap.sh` cites
D-060 for the pin circularity, but no such entry was ever recorded. The entry
below is reconstructed from the script's own comments and the failure it
describes; check it against your memory of that session before merging.

Earlier drafts this session proposed D-057–D-063 for these decisions. Those
numbers collide with the real D-058 and D-059 and should be discarded.
`docs/decision-D061.md` in the docs root is one such draft; its content is
D-064 below.

---

## D-060 — `--latest` exists because a pin cannot advance itself
**[[1.7 Tools]]**

`bootstrap.sh` checks out the pin; `--update` records whatever is checked out.
Running them in sequence writes the old pin straight back.

**The failure was silent and every individual step reported success.** A pushed
change to `squared` was reverted by a bootstrap doing exactly what it was told.
Nothing in the output said a version had moved backwards.

`git pull` was not the alternative: before D-065 bootstrap left each component
on a detached HEAD, where pull refuses.

`--latest` fetches, moves each component to *the remote's own default branch*
rather than an assumed `main` — a component may call it something else, and
guessing would fail in a way that looks like a network problem — and records
that as the new pin.

Rejected: making `--update` fetch first, which would silently change what "the
current checkout" means — the one thing `--update` is for.

---

## D-061 — Kit headers live at `sq_kit/include/<kit-name>/`
**[[2.9 Kits]]**

Supersedes the `sq_kit/include/squared/kit/<header>` layout shipped in the
first four kits.

A generated project compiles with two include roots, `-Isq_app/include` and
`-Isq_kit/include`. Header resolution takes the first match in `-I` order and
**issues no diagnostic when a second file of the same name exists further down
the list.** With both roots flat, any shared filename resolves silently to the
app's copy — including from inside kit sources, which surfaces as errors in a
translation unit the user never touched.

The names at risk are exactly the ones a bridge wants: `window.hpp`,
`graphics.hpp`, `event.hpp`. One prefix segment makes the roots disjoint by
construction, so no `-I` ordering discipline and no naming convention has to be
maintained by kit authors.

**The segment sits inside the include root, not above it.** A layout of
`sq_kit/<kit-name>/include/` looks equivalent when browsing the tree but is
not: the `-I` would point at `sq_kit/<kit-name>/include`, the include string
would be bare `<gl.hpp>`, and the flat-search-space problem returns in full.
Recorded because the distinction is invisible in a directory listing.

The segment is the kit name rather than a generic `squared/` because it is the
only variant that survives multiple kits in one workspace: `squared/` collides
with itself the moment a second kit appears.

Rejected: a shared installed include root under `sqsysroot`, which would have
justified a fuller namespace. Kits are owned by the workspace that contains
them; there is no shared kit and no mechanism to produce one.

---

## D-062 — Kit identifiers are exactly two dot-separated segments
**[[2.9 Kits]]**

`kit.opengl`, `kit.sfml`. Template identifiers stay free-form
(`template.android.cpp`).

D-061 derives a filesystem path segment from the kit identifier, and that
derivation must be total. `kit.sfml.x11` could map to `sfml/x11/`, `sfml_x11/`,
or be an error — each with different collision properties. Constraining the
identifier removes the question rather than answering it.

Templates are unconstrained because template identifiers never produce include
paths. **The asymmetry follows from what each identifier is used for, not from
a general naming rule.**

Already enforced in practice: `project_name` is typed `identifier`, and
`-p project_name=a.b` is refused with `template.parameter.invalid`. No new
validation was required.

---

## D-063 — The dot remains the single namespace separator
**[[2.6 Identity]]**

Confirms existing practice; recorded because it was reopened.

Underscore and hyphen alternatives were rejected. The dot reads as hierarchy —
kind, then identity, then optional qualification — and `template.android.cpp`
expresses that `android.cpp` is a subdivision of android templates, which
`template_android_cpp` flattens into one opaque token.

**The separator is already load-bearing elsewhere.** `manifest_extension()`
resolves dotted paths and consumer sections nest under dotted keys. Changing
identifier separators would leave the dot as the manifest-path separator and
something else as the identifier separator — two conventions where there is
one.

CLI cost is nil: dots are not shell metacharacters, so no quoting, no escaping,
no glob interaction.

See Q-27: §2.8.1's grammar and cartridge format §5.2's still disagree on
segment counts, and this entry does not resolve that.

---

## D-064 — Ownership tables glob the kit's own tree, enumerate outside it
**[[2.9 Kits]]**

`ownership.generated` uses a glob for the kit's own subtree (`sq_kit/**`) and
literal paths for files dropped outside it (`mk/kit_<name>.mk`).

Both forms were in use. kit.terminal globbed; kit.lua, kit.opengl and
kit.termux enumerated their single header. **The D-061 flatten showed the
difference is not cosmetic:** the glob was immune to the header move, while all
three literal entries needed rewriting, and a stale entry would have left a
file `workspace.verify` no longer tracks.

The split follows ownership itself. Everything under `sq_kit/` belongs to the
kit whatever it is named, so the pattern states a fact that stays true across
refactors. Files outside that subtree share a directory with other producers,
so the claim has to say which file is whose.

kit.lua, kit.opengl and kit.termux should be converted to the glob form; their
literal entries are correct post-flatten but remain fragile.

---

## D-065 — Vendored components sit on a branch, never detached
**[[1.7 Tools]]**

`bootstrap.sh` places each vendored component on `pin/<ref>` at the pinned
commit rather than checking out the bare ref. A 40-character SHA is abbreviated
in the branch name.

`git checkout <sha>` leaves a detached HEAD. A commit made there belongs to no
branch and is reachable only by its SHA, **with no warning at commit time, at
status time, or at any point before it is lost.**

Not hypothetical: the D-061 flatten was committed onto a detached HEAD in
`resources/.squared` and survived only because the SHA was still in scrollback.
`git push` refused it; `git status` in the parent repo showed a clean tree;
every individual step reported success.

The script's own header already made this argument for choosing a script over
submodules — that a contributor should get "an ordinary checkout on an ordinary
branch, not a detached HEAD they have to remember to fix before committing."
The implementation did not do what the rationale claimed.

**If that branch exists and has moved, the script reports and leaves it.** A
component branch that has moved means somebody worked there; discarding it
would recreate the failure this change exists to prevent, only louder.

`tools/sq-sync.sh` handles components bootstrapped before this change, which
stay detached until named.

---

## D-066 — Pin files may carry a semantic version, resolved to an exact tag
**[[1.7 Tools]]**

`SQUARED_VERSION` and `SQCART_VERSION` may contain `0.3.0`, resolved to tag
`v0.3.0` or `0.3.0`. A SHA, tag or branch name continues to work.

**A pin names one exact tag. It is not a range.** Ranges would make bootstrap
enumerate tags and select the highest match — a resolver in the one script that
touches the network, and a return to "give me whatever satisfies this", which
is the failure the pins exist to prevent. Ranges stay in cartridge manifests,
where they describe compatibility rather than select a checkout.

A SHA pin is correct and unreadable. It cannot be compared by eye against a
manifest's `requires.framework`, gives no signal about the size of a change,
and in practice went un-bumped because updating it meant copying forty
characters.

`--update` records a tag when HEAD carries one, and **refuses** when the
existing pin is a version and HEAD has no tag — silently changing a pin's kind
leaves the next reader unable to tell whether it was deliberate.

**New invariant: a published tag in `sqcart` or `squared` is never repointed.**
A semver pin is reproducible only so long as this holds. Re-tagging silently
changes what every past commit of squared-pg builds against, and nothing in the
build would report it.

Neither repository carries tags yet; until they do, semver pins cannot resolve
and bootstrap says so explicitly.

---

## D-067 — SFML on Android is a template, not a kit
**[[2.8 Templates]]** · **[[2.9 Kits]]**

`template.android.sfml` exists because SFML cannot be applied to
`template.android.cpp` as a rendering kit. Three independent blockers, each
verified in SFML 3.1.0's source:

- `src/SFML/Main/MainAndroid.cpp:491` defines `ANativeActivity_onCreate` — the
  same process entry point `android_native_app_glue` occupies. `sfml-main`
  links with `-u,ANativeActivity_onCreate` to force it in. Two definitions, one
  slot.
- `WindowImplAndroid(WindowHandle)` is an **empty stub**: the parameter is
  commented out. Adopting an existing `ANativeWindow*` compiles and does
  nothing.
- `WindowImplAndroid` reaches global `ActivityStates` sixteen times via
  `getActivity()`, which asserts on a pointer set only by SFML's own
  `ANativeActivity_onCreate`.

**SFML is the platform layer on Android; it does not plug into someone else's.**
`template.android.cpp`'s `sq_android/entry.cpp` is ownership class `seeded` —
the user's file — so a kit could not replace it even if the symbol collision
did not exist. And the kit doctrine holds: a kit adds to a workspace's build,
it does not reorganise it.

The two templates are alternatives, and `sq_app/` compiles unchanged under
both. That portability is what makes the choice reversible, and it is worth
defending.

Rejected: a renderer-contract header both kits satisfy. It would solve the
`__has_include(<opengl/gl.hpp>)` hardcoding in `template.android.cpp` — still
worth solving, see Q-29 — but it cannot solve an entry-point collision.

---

## D-068 — Vendored third-party source lives in `third-party/`; artifacts ship in the cartridge
**[[1.6 Third-party dependencies]]** · **[[2.13 Dependency graph]]**

SFML's source is vendored at `third-party/SFML-3.1.0/`, its dependencies at
`third-party/sfml-deps/`. `tools/build-sfml.sh` produces static archives into
`build/sfml-stage/`; `--install` copies them into kit.sfml's payload for local
iteration, gitignored. At release time CI builds them and they reach users
inside the cartridge.

**Source cannot live under `resources/`.** §2.13.2 makes resources inert data
with no outgoing dependencies, and §2.13.6 lists a file-type check over
resource roots as an enforcement mechanism. Eight hundred `.cpp` files and a
CMake build are what that check exists to catch; built archives are worse.

That is also why `third-party/` is at the repository root — the same place
miniz and yyjson already live.

**Generation stays a file copy.** The engine's capabilities are copy,
substitute, plan, generate, index, archive, rename. Nothing builds, and nothing
needed to: the compile happens in tooling, the same place `bootstrap.sh`
already lives and for the same reason.

The workspace stays self-contained, so the smoke suite's "the generated project
does not depend on the generator" check remains honest.

Rejected: a `package.vendor.sfml`. §2.11.1's taxonomy assigns external
frameworks to kits and reserves packages for Squared's own functionality;
putting SFML in `resources/packages/` would have contradicted it *and* put
source in a resource root.

Rejected: referencing a shared build under `sqsysroot`, which breaks the
generated-project independence invariant outright.

---

## D-069 — kit.sfml ships SFML's include tree verbatim, and archives under `sq_kit/lib/`
**[[2.9 Kits]]**

Two conventions this kit is the first to need.

**Headers at `sq_kit/include/SFML/`, capital, not a D-061 kit-name directory.**
The payload is a third party's include tree reached as `<SFML/Window.hpp>`, not
a Squared-side bridge. D-061 exists to keep *bridge* headers from colliding;
kit.sfml ships no bridge header, so there is nothing to collide with. The
scaffolded `include/sfml/sfml.hpp` was removed for the same reason — on a
case-insensitive filesystem it and `include/SFML/` are the same path, and CI
runs on macOS.

**Archives at `sq_kit/lib/`,** declared in the manifest's `binary.archives` so
the location is stated rather than implied.

The manifest also carries a `binary` block recording ABI, API level and
linkage. Nothing reads it yet. It is the field a resolve-time ABI check would
use, and without it the archives are anonymous: nothing in
`libsfml-window-s.a` says arm64-v8a at API 24.

---

## D-070 — Kits are an ordered list; ordering is the author's responsibility
**[[2.9 Kits]]**

`--kit` is accepted repeatedly and carried as an ordered list end to end. This
was found to be **already implemented**: `squaredpg.args` lists `kit` as
repeatable, `build_config` passes `kits` through, and a two-kit generation
against `template.terminal.cpp` applies both in order.

When kit-on-kit dependency is enforced, the user supplies the order and a kit
that depends on another must be listed after it. **Validation fires at
generation time.** A wrong order surfacing as a missing symbol inside generated
code is expensive to diagnose on-device and points the author at the wrong
artifact.

### The schema is already complete

Every field needed lives in `consumers.squared_pg` and none of it touches
sqcart:

- `requires.kits` — present and empty in all four original kits
- `provides` — kit.terminal declares `console.io`, `text.regex`, `files.handle`
- `conflicts_with` — **populated**: kit.terminal declares a conflict with
  kit.ncurses

Note the shape difference: a template's `requires.kits` is
`{required: [], optional: []}`; a kit's is a flat array. A template declares a
selection policy, a kit declares peers. Both intentional.

### What overlap detection can and cannot do

**Declared overlap is detectable.** Two kits claiming the same `provides`
string is a comparison of two string lists; `integration_arity` already refuses
two renderers on a single-arity area, and the smoke suite exercises it.

**Undeclared overlap is not.** Two kits that each quietly own an EGL context
without saying so are indistinguishable from two that cooperate.

The kit author's responsibility is therefore narrower than "be mindful":
declare accurately what a kit provides and conflicts with. The generator
enforces what is declared. Enumerating incompatible pairs is explicitly not
attempted — it would require sqpg to track every framework's resource model and
would still be incomplete.

### Deferred

Reading `requires.kits` for ordering, `provides` for overlap, and
`conflicts_with` for the data already waiting in it. Adding kits to an existing
workspace after generation.
