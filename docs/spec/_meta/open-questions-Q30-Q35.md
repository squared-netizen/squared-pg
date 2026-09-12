---
title: Spec open questions — Q-30 to Q-35 (pending merge)
tags: [spec, meta, pending]
---

# Pending open questions

**Not yet in [[Spec open questions]].** The register currently runs to Q-29;
these continue it. Append and delete this file.

Q-27–Q-29 are still marked untriaged in the 2026-09-07 triage note. Per
[[Spec conventions]], a question added during implementation is triaged when
the audit that would have caught it closes — these five are added during
implementation and inherit that obligation.

---

- **Q-30** — the installed resource tree at `~/sqsysroot/.sqpg/resources` can
  be stale relative to the repository, and nothing reports it. Cost three
  separate diagnosis sessions in one day: a flattened kit header layout, a
  deleted backup file, and a corrected makefile comment each appeared fixed in
  the repository while the CLI continued to read the old copy. `sqpg doctor`
  already knows both paths and its stated purpose is "what is assembled,
  installed and stale"; comparing resource identities and mtimes would close
  this. Open: whether `plan` and `new` should also refuse, or only warn, when
  the installed tree is older than the repository.

- **Q-31** — `sqpg initialize`'s resource refresh is **additive**. It
  overwrites what is present and never removes what is gone, so a deleted or
  renamed resource file persists in the installed tree — and, being absent from
  every ownership table, is reclassified as `seeded` and copied into every new
  workspace. Observed twice: the pre-flatten `squared/kit/*.hpp` headers, and
  two `.bak` files, all of which reached generated projects as seed content.
  Silently stale is bad; silently promoted to seed content is worse. Open:
  whether the refresh clears the resource tree first, or reconciles against the
  manifests.

- **Q-32** — `initialize` is two operations under one verb. Run from a source
  tree it installs and refreshes; run from an installed root it no-ops with a
  quiet message. The guard is correct — an installed tool should not copy over
  itself — but it means `sqpg initialize`, the obvious thing to type after
  editing resources, is the invocation that does nothing, while `build/sqpg
  initialize` is the one that works. Open: whether to split the verb, or to
  make the no-op message name the working alternative.

- **Q-33** — a syntax error in a generated makefile fragment takes out the
  diagnostics along with the build. Make parses `include`d files before running
  any target, so one malformed line in `mk/squared_generated.mk` made
  `android-status`, `android-help` and `squared-info` unreachable — the
  commands you would reach for to investigate. The error was a comment line
  that lost its `#`. Open: whether a template's own checks should parse its
  fragments (this class of error is invisible until a project is generated),
  and whether the workspace Makefile should degrade to a diagnostic-only mode
  when a fragment fails to parse.

- **Q-34** — SFML's Graphics and Audio modules are not yet built. The
  dependencies are vendored (freetype, harfbuzz, SheenBidi for Graphics; ogg
  and vorbis for Audio) but none has been cross-compiled for Android on
  Termux's clang, and SFML's own CMake patches FreeType's config to break a
  FreeType/HarfBuzz cycle (`src/SFML/Graphics/CMakeLists.txt` around lines
  119–135). Whether that patching survives outside SFML's FetchContent flow is
  unknown. Until this closes, kit.sfml provides window, GLES context and input
  only.

- **Q-35** — the archives kit.sfml ships are built for one ABI at one API
  level, recorded in `binary.abi` and `binary.api_level` (D-069) but read by
  nothing. A workspace generated for a different target links a wrong-ABI
  archive and fails at link time, or worse at load time. Open: whether
  resolution should refuse on mismatch, which requires the engine to know the
  requesting target — a fact it does not currently carry, since ABI is a build
  setting rather than a generation parameter (see `mk/squared_generated.mk`'s
  note on SDK levels).
