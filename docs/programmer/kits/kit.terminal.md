# kit.terminal

Developer-facing counterpart: [../../developer/kits/kit.terminal.md](../../developer/kits/kit.terminal.md)

One header of simple terminal utilities. It uses only the C++20 standard
library, so a workspace needs a compiler and `make` and nothing else.

```sh
sqpg new hello -t template.terminal.cpp -k kit.terminal
sqpg new hello -t template.term.edu.cpp -k kit.terminal   # the teaching template
```

Version `0.2.0`. Compatible with `template.terminal.cpp` and
`template.term.edu.cpp`. Conflicts with
`kit.ncurses`. Provides `console.io`, `text.regex` and `files.handle`.

## Using it

```cpp
#include <terminal/terminal.hpp>

int main() {
    using namespace sq::term;
    const auto name = prompt("Your name?", "world");
    println("Hello, ", name, "!");
}
```

The header is `sq_kit/include/terminal/terminal.hpp` and everything is in
namespace `sq::term`. The kit adds `-Isq_kit/include` to the build; nothing is
linked. Files under `sq_kit/` and `mk/kit_terminal.mk` are generated: write your
own code in `sq_app/`.

**Nothing here throws.** Failure comes back as an empty `std::optional`, an
empty container, `false`, or the fallback you supplied.

## Output

| Function | Does |
|---|---|
| `print(args...)` | Write every argument to stdout. No separator, no newline. |
| `println(args...)` | Like `print`, then a newline. |
| `eprintln(args...)` | Like `println`, to stderr. Use it for diagnostics so real output stays pipeable. |
| `flush()` | Flush stdout. |

Any type with `operator<<` works as an argument, including `FileHandle`.

## Input

| Function | Returns | Notes |
|---|---|---|
| `input()` | `optional<string>` | One line, newline removed (a trailing `\r` too). Empty optional at end of input, which is not the same as an empty line. |
| `input(prompt)` | `optional<string>` | Prints `prompt`, flushes, then reads. |
| `prompt(message, fallback = "")` | `string` | Shows `[fallback]` when one is given. Returns `fallback` on an empty line or end of input. |
| `confirm(message, fallback = false)` | `bool` | Shows `[y/N]` or `[Y/n]`. True if the answer starts with `y` or `Y`. End of input or an empty line gives `fallback`. |
| `read_all_input()` | `string` | The whole of stdin. For programs used in a pipe. |
| `parse<T>(text)` | `optional<T>` | Parses the whole string as a number. Empty if it is not entirely a valid `T`. |

Always check the optional from `input`. A loop that treats "no line" as "empty
line" never ends when input is redirected from `/dev/null`.

```cpp
const auto line = input("Number? ");
if (!line) return 0;                 // input ended
const auto n = parse<int>(*line);
if (!n) { println("not a number"); return 1; }
println(*n * 2);
```

## Text

A facade over `<regex>` (ECMAScript syntax). A pattern that does not compile
never throws: the functions below return `false`, an empty container, or the
input unchanged.

| Function | Returns |
|---|---|
| `pattern(expr, ignore_case = false)` | `optional<std::regex>`; empty if `expr` is invalid. |
| `matches(text, expr, ic)` | Whether the whole string matches. |
| `contains(text, expr, ic)` | Whether the pattern occurs anywhere. |
| `find(text, expr, ic)` | First match as `vector<string>`: index 0 is the whole match, 1..n the groups. Empty if no match. |
| `find_all(text, expr, ic)` | Every whole match. |
| `replace(text, expr, with, ic)` | Every match replaced. `$1`, `$2` refer to groups. |
| `split(text, expr, ic)` | Fields between matches. Empty fields are kept. |
| `trim(text)` | `text` without leading and trailing ASCII whitespace. |

## Files

`FileHandle` wraps a path, modelled on libGDX's class of the same name.

```cpp
const FileHandle notes{"notes/today.txt"};
if (notes.writeString("buy milk\n", /*append=*/true)) println("saved to ", notes);
for (const auto& line : notes.readLines()) println("- ", line);
```

| Member | Does |
|---|---|
| `FileHandle(path)` | Explicit; accepts anything `std::filesystem::path` accepts. |
| `readString(path)` / `readString()` | Whole file as `optional<string>`; empty if unreadable. Static and instance forms. |
| `writeString(path, text, append = false)` / `writeString(text, append)` | Writes the file, creating parent directories. Returns success. |
| `path()`, `name()`, `extension()`, `nameWithoutExtension()` | Path parts. |
| `parent()`, `child(name)` | A new `FileHandle`. |
| `exists()`, `isDirectory()`, `length()` | Queries. `length()` is 0 on error. |
| `readLines()` | Lines without newlines; no final empty line for a trailing newline. |
| `mkdirs()`, `remove()` | Return success. |
| `list()` | Immediate children, sorted. |

## Not included

No raw mode, cursor control or alternate screen. That is a TUI's job
(`kit.ncurses`).
