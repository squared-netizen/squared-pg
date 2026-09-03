# AGENTS.md — `app/`

`app/` is the reference CLI host. Supplements the repository `AGENTS.md`.

## The one rule

**No generation policy in native code** (§2.5.4, D-025).

This host does not know what a template is, does not define an argument
grammar, and does not decide anything about a project. It:

1. creates and initializes an engine,
2. starts a Lua state and installs the bindings,
3. hands argv to a workflow verbatim,
4. reports the exit status and shuts the engine down.

The single argument it interprets is `--workflow`, because something must
choose which script runs before a script is running. That is host
configuration, not generation policy.

If you find yourself adding a flag here that a workflow could have handled,
it belongs in `lua/workflows/` instead.

## Why it is a separate top-level directory

The host is a *consumer* of the engine, not part of it (§2.1.6). Putting it
under `engine/` would blur exactly the line §2.2.5 draws. Recorded as D-028.
