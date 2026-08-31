# AGENTS.md

## Project Context
- Languages: C++17 or C++20 (per-project, check `CMakeLists.txt`/`compile_commands.json` for the actual standard), Lua 5.4 as scripting layer.
- Prefer modern C++ standard library facilities and idiomatic Lua 5.4 over hand-rolled equivalents.
- Use established design patterns where they genuinely fit the problem, and document *why* a pattern was chosen, not just that it was.

## Tooling Preferences
- Discovery/search: `fd`, `rg`, `git grep`, `ast-grep` — never `find`/`grep` for code search.
- Structural/semantic edits: prefer LSP-aware tools (`clangd` for C++, a Lua language server, e.g. `lua-language-server`) over `sed`/text-substitution for anything beyond trivial renames. `ast-grep` is fine for simple syntactic patterns; anything depending on overload resolution, template instantiation, or implicit conversions needs LSP/semantic tooling, not pattern matching.
- Structured data: `jq` (JSON), `yq` (YAML) — never ad hoc text parsing.
- C++ quality tools: `clang-format`, `clang-tidy`, `clangd`, `clang-query`/`clang-check`. Never hand-fix something a tool can auto-apply — run the tool, then review its diff.
- Lua quality tools: `luacheck` (lint) and `stylua` (format), or project-configured equivalents if present.

## Workflow

**Step 1 — Inspect the workspace**
Identify build system, language standard, existing style/lint configs (`.clang-format`, `.clang-tidy`, `.luacheckrc`, `stylua.toml`), directory layout, and test harness before touching anything. Do not assume conventions — confirm from the repo.

**Step 2 — Generate/edit code via LSP**
Make the smallest reviewable change that accomplishes the current milestone step. Use `clangd`/Lua LSP for navigation, refactors, and correctness-sensitive edits. Avoid unrelated reformatting or refactors in the same change.

**Step 3 — Lint / format / static analysis loop**
Run, in order: formatter (`clang-format` / `stylua`) → linter/static analysis (`clang-tidy` / `luacheck`). Apply auto-fixes directly from tool output. For anything the tools flag but can't auto-fix, edit the code, then re-run Step 3 tools to confirm clean.

**Step 4 — Repeat**
Loop Steps 2–3 until the current milestone (not just the current file) is complete. Each loop iteration should still produce a minimal, reviewable diff.

**Step 5 — Test**
Run unit tests if present. If no unit tests cover the change, run a smoke test or a live/manual run appropriate to the project (e.g. launching the engine with a minimal Lua script). Do not proceed to documentation on a failing or unverified run.

**Step 6 — Documentation**
Only after a successful Step 5 run:
- Update **developer docs** (engine/framework maintainers): implementation details, data structures, algorithms, and the reasoning behind design-pattern choices.
- Update **programmer docs** (API consumers): public API surface, usage examples, behavior contracts — not internals.

## Completion Report
Every completed milestone/change should report:
- What changed and why (one paragraph).
- Which tools were run in Step 3 and their final (clean) status.
- Test/smoke-test result from Step 5.
- Which docs (developer/programmer, or both) were updated in Step 6.
