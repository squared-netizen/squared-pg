---
title: GitHub Actions
tags:
  - ci
  - github
---

# GitHub Actions

The workflow at `.github/workflows/android-debug.yml` is manual by default.
It downloads the pinned SDL2 offline kit from its v1.0.0 GitHub release,
verifies the published SHA-256 file, builds the Android ARM64 application, and
uploads the debug APK.

The workflow at `.github/workflows/docs.yml` is also manual. It verifies the
pinned SDL Project Generator release, reconstructs its private documentation
toolchain, runs `sdl-pg docs`, and uploads the Lua, C++, and Java API
references.

Local development does not depend on GitHub Actions.
