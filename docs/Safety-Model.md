---
title: SDL Project Generator Safety Model
tags:
  - filesystem
  - safety
  - testing
---

# SDL Project Generator Safety Model

`sdl-pg` treats filesystem changes as explicit operations:

1. Project names cannot contain paths or `..`.
2. Creation is restricted to the configured sandbox or projects root.
3. Existing destinations are never overwritten.
4. Promotion and demotion require `.sdl-pg.lua`.
5. Promotion rejects `.git`; demotion deliberately omits it.
6. Symlinks and special files are rejected while copying project trees.
7. Copies are assembled in a temporary sibling directory.
8. The final destination appears through one rename.
9. A failed copy removes only the temporary directory it created.
10. Source directories are preserved.

The initial generator intentionally has no delete command.

## Git policy

Git operations are outside the generator command set. The generated GitHub
Actions workflow is manual and never commits, pushes, tags, or releases.
