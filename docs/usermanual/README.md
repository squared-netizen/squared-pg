---
title: User manual
tags: [usermanual]
---

# squared-pg user manual

For people **building something** with a workspace squared-pg generated.

Nothing here requires knowing how the generator works. No engine API, no
manifest schema, no Lua. If a page starts explaining how squared-pg decides
something, it is in the wrong tree — that belongs in
[[programmer/README]] (using the generator's API and authoring resources) or
[[developer/README]] (maintaining the generator itself).

## Pages

- [[usermanual/getting-started]] — generate a workspace and run it
- [[usermanual/workspace]] — what the directories are, which files are yours
- [[usermanual/android-sfml]] — Android apps with SFML
- [[usermanual/cartridges]] — `sqcart`, for opening and building `.sq` files

## What you need installed

Covered in [[usermanual/getting-started]]. The short version for Android work
is Termux with clang, make, the Android SDK build tools and an NDK.

## Not yet written

- A page for `template.terminal.cpp` and the terminal kits
- `promote`, `demote` and `quarantine` — the sandbox-to-project lifecycle
- The Squared framework itself, which is a separate body of documentation
