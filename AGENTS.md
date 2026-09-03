# AGENTS.md

## Project Context

`squared-pg` is the offline-first project generator for the Squared framework:
a native C++ engine providing generation capabilities, and a Lua 5.4 control
layer providing workflow orchestration.

- **C++20 exclusively.** Not C++17. Use concepts, ranges, `std::span`,
  designated initializers, `constexpr`/`consteval` where they improve clarity or
  safety.
- **Lua 5.4** for the scripting layer.
- **Offline is a hard constraint.** A clean checkout plus `third_party/` must be
  sufficient to inspect, build, test, and develop. Never add a dependency that
  requires network access at build or run time.
- **Termux is a first-class target.** Prefer solutions that work without
  elevated privileges, without cross-device renames, and without heavy toolchains.

## Specification is a guideline (temporary stance)

> [!note] Temporary — initial implementation only
> During the initial implementation, the **repository is the ground truth** and
> the specification is a **strong guideline that is weakly enforced**. Where the
> spec and the repo disagree about the repo's present state, the repo wins and
> the spec is amended to match. This relaxes the specification's authority on
> purpose so implementation can move faster than the spec. This is a temporary
> stance for the initial implementation; revisit and tighten enforcement once the
> engine reaches a stable shape.

The functional specification lives at `docs/spec/` and is an Obsidian vault.
Start at `docs/spec/Specification Index.md`.

Before making an architectural decision, check whether the specification already
made it. Decisions are logged in `docs/spec/_meta/Spec decisions.md`; unresolved
items are in `docs/spec/_meta/Spec open questions.md`.

Normative keywords (MUST/SHOULD/MAY) describe the intended design and are binding
on implementations that aim to conform, but they are aspirational for the current
implementation and never override the repository as ground truth.

If an implementation cannot satisfy the specification, say so and propose a
specification change. Do not silently diverge — when you diverge, record it in
the decision log and amend the spec to match the implementation.

## Language and library policy

- Prefer the C++ standard library over hand-rolled utilities or third-party
  dependencies: containers, `<algorithm>`, `<ranges>`, smart pointers,
  `<chrono>`, `<filesystem>`, `<optional>`, `<variant>`, `<expected>`.
- Introduce an external dependency only when the standard library is genuinely
  insufficient, and justify it in the developer docs.
- RAII for all resource management. No manual `new`/`delete` in engine code.
- Prefer `std::unique_ptr`/`std::shared_ptr` over raw owning pointers.
- Prefer value and move semantics over unnecessary copying.
- Mark functions `const`, `noexcept`, `[[nodiscard]]` where correct.
- Avoid macros except for build configuration and platform shims.
- No raw owning arrays. Use `std::array`/`std::vector`/`std::span`.
- Prefer compile-time checks (concepts, `static_assert`) over runtime checks.
- Keep the C++/Lua boundary explicit and narrow. Bind through the defined API
  surface; no ad hoc `lua_State` manipulation scattered through engine code.
- No coroutines in v1 (see D-019).

### Type aliasing

Alias a type when **any** of these hold:

1. The fully-qualified name exceeds 40 characters, or nesting depth is 3+.
2. The exact type string appears 3+ times in one file.
3. It represents a named domain concept — something that would appear as a noun
   in the specification.
4. It crosses a public API boundary.
5. It is a template pattern reused across 2+ instantiation sites.

Do **not** alias a one-off local scratch type, and do not alias in a way that
erases information a caller needs. `using Meters = int;` adds meaning;
`using Data = std::vector<Widget>;` removes it.

## Design patterns

Apply established patterns — GoF and engine patterns such as Strategy, Command,
Factory, Observer, Service Locator, State, Object Pool — where they genuinely fit.
Do not force a pattern where a simpler solution is clearer.

Every pattern used MUST be documented in the developer docs: name it, explain why
it was chosen over the alternatives, and note deviations from the textbook form.

Patterns already fixed by the specification:

| Pattern | Where | Why |
|---|---|---|
| Command | generation plan | plan and apply consume the same reified list, so they cannot diverge |
| Strategy | template processors, asset transformers | selected by manifest declaration; new ones need no core change |
| Service Locator | engine services | registered at init, resolved by operations |
| State | engine lifecycle | explicit states with a legality matrix |

## Tooling Preferences

- Discovery/search: `fd`, `rg`, `git grep`, `ast-grep`. Never `find`/`grep` for
  code search.
- Structural/semantic edits: prefer LSP-aware tools (`clangd`,
  `lua-language-server`) over `sed`. `ast-grep` is fine for simple syntactic
  patterns; anything depending on overload resolution, template instantiation, or
  implicit conversions needs semantic tooling.
- Structured data: `jq` for JSON, `yq` for YAML. Never ad hoc text parsing.
  Manifests are JSON — read them with `jq`.
- C++ quality: `clang-format`, `clang-tidy`, `clangd`, `clang-query`/`clang-check`.
- Lua quality: `luacheck`, `stylua`, or project-configured equivalents.
- Never hand-fix what a tool can auto-apply. Run the tool, then review its diff.

## Workflow

**Step 1 — Inspect the workspace.**
Identify the build system, language standard, style and lint configs
(`.clang-format`, `.clang-tidy`, `.luarc.json`, `.luacheckrc`, `stylua.toml`),
directory layout, and test harness before touching anything. Confirm conventions
from the repository; do not assume them. Check the applicable `AGENTS.md` files —
a nested one overrides or supplements this file for its directory.

**Step 2 — Check the specification.**
Find the section governing what you are about to build. If it is silent, say so
before implementing, and propose the specification text alongside the code.

**Step 3 — Generate/edit code.**
Make the smallest reviewable change that accomplishes the current milestone step.
Use LSP tooling for navigation, refactors, and correctness-sensitive edits. No
unrelated reformatting or refactors in the same change.

**Step 4 — Lint / format / static analysis loop.**
Run in order: formatter (`clang-format` / `stylua`), then linter and static
analysis (`clang-tidy` / `luacheck`). Apply auto-fixes from tool output. For
anything flagged but not auto-fixable, edit and re-run until clean.

**Step 5 — Repeat.**
Loop Steps 3–4 until the milestone is complete, not merely the current file. Each
iteration should still produce a minimal, reviewable diff.

**Step 6 — Test.**
Run unit tests if present. If none cover the change, run a smoke test appropriate
to the project — for example, launching the engine with a minimal Lua workflow.
Do not proceed to documentation on a failing or unverified run.

**Step 7 — Document.**
Only after a successful Step 6:

- **Developer docs** (`docs/developer/`) — implementation, data structures and
  their complexity characteristics, algorithms, design-pattern reasoning,
  invariants, ownership and threading model, known limitations.
- **Programmer docs** (`docs/programmer/`) — public API only: purpose,
  parameters, returns, pre/postconditions, errors, a minimal example, and the
  Lua-side equivalent where bound. No internals.

The two trees mirror each other's module structure and cross-link. Documentation
is part of the change, not a follow-up task.

## Deliverables

When producing a downloadable artifact:

- Package all files into a single `.zip`.
- Include a fish script (`install.fish` or `apply_<feature>.fish`) that installs
  the deliverable into the right locations. It MUST be idempotent and safe to
  re-run.
- Reference the script in a short note alongside the archive.

## Completion Report

Every completed milestone reports:

- What changed and why, in one paragraph.
- Which specification sections govern the change.
- Which design patterns were applied, and why.
- Any place a standard library solution was passed over, with justification.
- Which Step 4 tools were run and their final clean status.
- The Step 6 test or smoke-test result.
- Which documentation trees were updated.
