---
title: Squared Project Generator Safety Model
tags:
  - filesystem
  - safety
  - testing
---

# Squared Project Generator Safety Model

`squared-pg` treats filesystem changes as explicit operations:

1. Project names cannot contain paths or `..`.
2. Creation is restricted to the configured sandbox or projects root.
3. Existing destinations are never overwritten.
4. Promotion and demotion require `.squared-pg.lua`; the legacy
   `.sdl-pg.lua` marker remains accepted during migration.
5. Promotion rejects `.git`; demotion deliberately omits it.
6. Symlinks and special files are rejected while copying project trees.
7. Copies are assembled in a temporary sibling directory.
8. The final destination appears through one rename.
9. A failed copy removes only the temporary directory it created.
10. Source directories are preserved.

The initial generator intentionally has no delete command.

## Android broad storage access

Unrestricted shared-storage access is disabled by default. The Android-only
`--manage-all-files` generation option must be selected explicitly. When it is
selected, the generated project declares `MANAGE_EXTERNAL_STORAGE`, opens the
app-specific special-access settings once on first startup if access is
missing, and records the security implications in a prominent README warning.
The setting remains a user-controlled Android grant and can be revoked.

## Git policy

Git operations are outside the generator command set. The generated GitHub
Actions workflow is manual and never commits, pushes, tags, or releases.

## Generated plug-ins

Generated plug-ins do not receive the raw APK-asset reader. Each plug-in gets
a read-only host proxy containing only declared capabilities, and its local
module loader cannot escape the plug-in asset root. This isolates cooperative
application extensions and ordinary programming mistakes; it is not a
hardened boundary for hostile scripts.
