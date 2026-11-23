---
title: Getting started
tags: [usermanual]
---

# Getting started

## Install the tool

In sqterm, from its package repository:

```sh
pkg install squared-pg
```

That installs `sqpg` and `sqcart`, with the templates, kits and packages sqpg
generates from (`$PREFIX/share/squared-pg`). To build what it generates, also
`pkg install sqmake squared`. sqpg keeps no environment of its own: there is
nothing to initialize, and a workspace lives wherever you make it.

## Generate a workspace

```sh
mkdir -p ~/projects && cd ~/projects
sqpg new hello -t template.android.cpp -p package_name=dev.me.hello
cd hello && sqmake run
```

`-t` selects the template — the project's shape. `-k` selects a kit, and may be
repeated; `-p key=value` sets a template parameter. `sqpg list template` and
`sqpg list kit` show what is available, and `sqpg show <id>` one of them with
its parameters. `-o PATH` puts the workspace somewhere other than `./<name>`.

A workspace is a cartridge folder: `SQ-INF/manifest.json` (kind `workspace`)
says what it was made from and where its parts are, and `SQ-INF/files.json`
lists each generated file with its class and checksum. square reads them;
`sqcart pack hello hello.sq` packs the workspace into one file.

## See before you commit

```sh
sqpg plan hello -t template.android.cpp -p package_name=dev.me.hello
```

`plan` prints every file that would be written, its origin and whether it is
yours or the generator's. It creates nothing.

## Your project does not need squared-pg

Once generated, a workspace builds with sqmake (or, with the framework
vendored by `--package package.squared-core`, with make). The generator is not
a build dependency.

## Useful commands

| Command | What it does |
|---|---|
| `sqpg list [template\|kit\|package\|asset]` | what is available |
| `sqpg show <id>` | describe one resource |
| `sqpg plan <name> -t <id>` | what would be generated |
| `sqpg new <name> -t <id>` | generate it |
| `sqpg inspect <workspace>` | read a workspace's record |
| `sqpg verify [workspace]` | check it against that record |

Run `sqpg` with no arguments for the full list. Questions about a workspace
and changes to it (`info`, `where`, `owner`, `verify`, `add`, `update`) move
to square as it is built; `inspect` and `verify` stay here until then.
