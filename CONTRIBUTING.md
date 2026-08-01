---
title: Contributing
tags:
  - development
  - testing
---

# Contributing

Squared Project Generator is offline-first. Changes must not make ordinary project
generation or local builds depend on a network connection.

## Development checkout

Keep experimental work beneath `~/sandbox`:

```bash
cd "$HOME/sandbox/squared-pg"
lua5.4 toolchain.lua
```

The complete toolchain command verifies bundled archives, builds the private
Lua runtime, runs CTest and Lua tests, and generates LDoc documentation.

## Documentation requirements

- Write Markdown that remains readable in Obsidian.
- Document public C++ headers with Doxygen.
- Document public Lua modules and functions with LDoc.
- Keep commands reproducible from Termux.

## Change policy

- Preserve the pinned Lua 5.4 runtime contract within a release series.
- Verify every bundled dependency with SHA-256.
- Keep generated projects independent from the generator installation.
- Keep GitHub Actions optional; local offline builds are the default.
- Add or update tests for every observable behavior change.
- Do not initialize, commit, push, or publish a generated project implicitly.

## Before committing

```bash
lua5.4 toolchain.lua
sha256sum -c SHA256SUMS.txt
git diff --check
git status --short
```

Regenerate `SHA256SUMS.txt` only after the intended source changes are final.
