# Contributing to squared-pg

Thank you for looking. squared-pg is part of the
[squared SDK](https://squared-netizen.github.io/squared-netizen/),
maintained by one person, so a short note first saves everyone time.

- **Something large** (a new feature, a change to a format or an API):
  open an issue describing it before writing the code.
- **A fix or a small change**: a pull request from a branch is welcome.
  Say what it changes and why, and how you checked it.
- **A security issue**: never in public. See [SECURITY.md](SECURITY.md).

## Before a pull request

Build with CMake as [BUILDING.md](BUILDING.md) describes (`cmake -S . -B build && cmake --build build && ctest --test-dir build`), and run the tests.

The pull request's CI must be green before it is merged. Builds are
offline: nothing in a build may download anything.

## Licence

squared-pg is under the MIT licence ([LICENSE](LICENSE)). By contributing,
you agree that your contribution is licensed under it too.
