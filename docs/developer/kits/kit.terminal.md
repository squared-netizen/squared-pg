# kit.terminal - implementation notes

Consumer-facing counterpart: [../../programmer/kits/kit.terminal.md](../../programmer/kits/kit.terminal.md)

## Version 0.2.0

Simplification of 0.1.0. The header code is unchanged; what changed is the
metadata and the build fragment, so the kit declares only what something reads
or what the project's own decisions ask authors to state.

| Changed | From | To | Why |
|---|---|---|---|
| `version` | 0.1.0 | 0.2.0 | Manifest and fragment changed. |
| `compatible_templates` | `template.terminal.cpp` | adds `template.term.edu.cpp` | The teaching template gained the kit-fragment hook (see its developer page), so the same kit serves both. |
| `description` | "...bridge..." | single header, standard library only | It wraps no external system, so "bridge" misled. |
| `external` block | `posix-terminal`, `vendored` | removed | The kit wraps nothing. Spec 2.9.2: absent means "wraps nothing"; the block was read only by diagnostics. |
| `runtime` block | `model: none` | removed | Read by nothing (spec 2.9.2). |
| `platforms` | termux, linux, macos, desktop | removed | Standard C++ only; absent admits every platform. |
| `requires.framework` | `>=1.0.0 <2.0.0` | removed | The kit does not use the framework; absent is unconstrained. |
| `requires.kits` | `[]` | removed | Empty; absent means none. |
| `requires.capabilities` | `substitute@^1.0` | removed | The payload contains no `{{`, so nothing needs substituting. |
| `mk/kit_terminal.mk` | variable `SQ_KIT_TERMINAL_INCLUDE` plus two appends | two appends | The variable had no other user. |
| Header comment | long design preamble | tidied | Typos and list layout from the hand edit; wording kept. |

Kept deliberately:

- `provides` and `conflicts_with`. D-070 puts the burden on kit authors to
  declare these accurately, even though overlap detection for `provides` is
  deferred. `conflicts_with` is already honoured by validation.
- `integration_areas` (`app.include`, `build.make.fragments`), which both
  templates declare, `requires.engine`, `requires.packages`.
- Layout `sq_kit/include/terminal/` (D-061: kit-named segment inside the include
  root keeps the app and kit roots disjoint) and the ownership entries
  `sq_kit/**` plus the literal `mk/kit_terminal.mk` (D-064).

Existing workspaces recorded kit 0.1.0 and are unaffected. Kit updates are not
implemented, so nothing migrates them.

## Layout

```text
resources/kits/kit.terminal/
  SQ-INF/manifest.json
  tree/sq_kit/include/terminal/terminal.hpp
  tree/mk/kit_terminal.mk
```

## Design patterns

| Pattern | Where | Why / deviation |
|---|---|---|
| Facade | the regex functions over `<regex>` | Four-argument `std::regex` calls and exceptions become one-line, non-throwing calls. Deviation: it is a set of free functions rather than a class, because there is no state to hold. `pattern()` is the escape hatch to the real type. |
| Value object | `FileHandle` | A cheap, copyable wrapper around `std::filesystem::path` with no identity. Modelled on libGDX, so it keeps libGDX's names (`readString`, `child`) rather than the standard library's. |

Not a pattern but a convention: `std::optional` is the error channel
everywhere. A terminal program's reaction to "no such file" is to print and
carry on, so exceptions would only be caught at every call site.

## Data structures and complexity

- Regex results are `std::vector<std::string>`; `find` reserves `match.size()`.
- `list()` sorts with `std::sort`: O(n log n), because directory order is not
  stable.
- `readLines()` and `readString()` are O(file size). The whole file is held in
  memory; this kit is not for large files.
- Each regex call compiles its pattern again. `std::regex` construction is
  slow, so a loop over many lines should call `pattern()` once and use
  `<regex>` directly.

## Standard library versus custom

Nothing third-party. Custom code where the standard library has no equivalent
or a worse fit:

- `trim`: there is no `std::trim`; six lines.
- `parse<T>`: wraps `std::from_chars` instead of `std::stoi`, which throws and
  accepts trailing garbage. Integer `T` works everywhere; floating-point `T`
  depends on the standard library's `from_chars` support, which older libc++
  releases lack. Verify on-device before documenting `parse<double>` as
  supported.
- `readLines`: a character loop instead of `std::getline` over a stream, so
  CRLF handling and the "no final empty line" rule live in one place.
- `FileHandle` has one constructor, not three, because overloads for string
  types make `FileHandle{some_string}` ambiguous.

## Invariants

- Header-only, every function `inline` or a template, include-guarded.
- No function throws. Anything that can fail returns a value that says so.
- Compiles warning-free under `-std=c++20 -Wall -Wextra -Wpedantic`.
- No terminal control (design note in the header).

## Verification

Each release: build a `template.terminal.cpp` workspace with the kit, run it,
and compile a smoke test calling every function under the flags above.
