---
title: Getting started
tags: [usermanual]
---

# Getting started

## Install the tool

squared-pg installs into an environment directory, `~/sqsysroot` by default
(override with `$SQSYSROOT`). From a built checkout:

```sh
./build/sqpg initialize
```

That creates the environment and copies the tool and its resources into
`~/sqsysroot/.sqpg`. It then prints two `ln -sf` commands to put `sqpg` and
`sqcart` on your PATH. Run them; **nothing edits a shell profile**, and
removing the tool later is `rm` on two links.

## The environment

```
~/sqsysroot/
  project/    promoted work; version controlled
  sandbox/    experiments; nothing here is irreplaceable
  .sqpg/      the installed tool, its resources and workflows
```

Generate into `sandbox/` while you are trying things. `sqpg promote <name>`
moves a workspace to `project/` when it is worth keeping.

## Generate a workspace

```sh
cd ~/sqsysroot/sandbox
sqpg new hello -t template.terminal.cpp -k kit.terminal
cd hello && make && ./build/hello
```

`-t` selects the template — the project's shape. `-k` selects a kit, and may be
repeated. `sqpg list template` and `sqpg list kit` show what is available.

## See before you commit

```sh
sqpg plan hello -t template.terminal.cpp -k kit.terminal
```

`plan` prints every file that would be written, its origin and whether it is
yours or the generator's. It creates nothing.

## Your project does not need squared-pg

Once generated, a workspace builds with `make` and a compiler. The generator is
not a build dependency and does not need to be installed to build a project it
produced.

## Useful commands

| Command | What it does |
|---|---|
| `sqpg list [template\|kit\|package\|asset]` | what is available |
| `sqpg show <id>` | describe one resource |
| `sqpg plan <name> -t <id>` | what would be generated |
| `sqpg new <name> -t <id>` | generate it |
| `sqpg inspect <workspace>` | read a workspace's record |
| `sqpg verify [workspace]` | check it against that record |
| `sqpg doctor` | what is installed and what is stale |

Run `sqpg` with no arguments for the full list.

## If something looks stale

`sqpg` reads its resources from `~/sqsysroot/.sqpg/resources`, not from a
source checkout. If you have changed a template or kit in a checkout, re-run
`./build/sqpg initialize` **from that checkout** — running the installed `sqpg
initialize` will report success and do nothing. This is a known rough edge
(Q-31, Q-32).
