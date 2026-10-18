# template.term.edu.cpp

Developer-facing counterpart: [../../developer/templates/template.term.edu.cpp.md](../../developer/templates/template.term.edu.cpp.md)

The simplest template. A terminal workspace for learning to program: one
program, one short Makefile that explains itself, and the C++ standard library.
It needs no kits. Two are optional: [`kit.edu_cpp`](../kits/kit.edu_cpp.md) adds lessons and
[`kit.terminal`](../kits/kit.terminal.md) adds input/output helpers.

Use `template.terminal.cpp` instead when you want the `App` class, the terminal
helpers, tests, sanitizers, or Lua.

## Creating a workspace

```sh
sqpg new hello -t template.term.edu.cpp                    # just the program
sqpg new hello -t template.term.edu.cpp -k kit.edu_cpp     # plus lessons
sqpg new hello -t template.term.edu.cpp -k kit.terminal    # plus print/input helpers
```

Kits can be combined: repeat `-k`.

Template id `template.term.edu.cpp`, version `0.1.0`. It requires no kits and
accepts `kit.edu_cpp` and `kit.terminal`.

| Parameter | Required | Meaning |
|---|---|---|
| `project_name` | yes | A C identifier. Becomes the binary name. |
| `description` | no | One line for the README. |

## What you get

```text
Makefile              seeded  - build file; a commented walkthrough of make
README.md             seeded  - how to run, how to read an error
AGENTS.md             seeded  - guidance for an AI assistant helping a learner
.gitignore            seeded
sq_app/README.md      seeded
sq_app/src/main.cpp   seeded  - hello world; the learner's program
```

Every file is `seeded`: written once and never overwritten. `sq_app/` is the
user working directory.

## Build interface

| Command | Effect |
|---|---|
| `make` | Build `build/<project_name>`. |
| `make run` | Build, then run it. |
| `make clean` | Delete `build/`. |
| `make -n` | Print the commands without running them. |

Overridable: `CXX` (default `c++`), e.g. `make CXX=clang++`.

Flags: `-std=c++20 -Wall -Wextra -g`. No optimisation, so debuggers and
warnings behave predictably.

## Source rules

- Every `.cpp` **directly inside** `sq_app/src/` is compiled into one program.
  Subdirectories are not searched. Exactly one file may define `main`.
- Headers go in `sq_app/include/` (already on the include path).
- Only the C++ standard library is available, plus `kit.terminal`'s
  `#include <terminal/terminal.hpp>` if you added that kit. A kit's include
  path is added to the build for you.

## Lua

Not supported.
