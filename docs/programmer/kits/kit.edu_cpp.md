# kit.edu_cpp

Developer-facing counterpart: [../../developer/kits/kit.edu_cpp.md](../../developer/kits/kit.edu_cpp.md)

Four short C++ lessons for a teaching workspace. Text only: it adds no code, no
include path and no build changes.

## Using it

```sh
sqpg new hello -t template.term.edu.cpp -k kit.edu_cpp
```

Compatible with `template.term.edu.cpp` only. Version `0.1.0`.

## What it adds

```text
lessons/cpp/README.md                    how to use a lesson, reading order
lessons/cpp/01-output.md
lessons/cpp/02-variables-and-input.md
lessons/cpp/03-decisions-and-loops.md
lessons/cpp/04-functions.md
```

Each lesson has a complete program that compiles warning-free under the
template's flags (`-std=c++20 -Wall -Wextra`) and ends with a **Try** exercise.
The learner pastes the code into `sq_app/src/main.cpp` and runs `make run`.

## Ownership

Every file is `seeded`: written once and never overwritten, so a learner can
write notes in the lessons without losing them.

## Removing it

Delete `lessons/`. Nothing else in the workspace refers to it.
