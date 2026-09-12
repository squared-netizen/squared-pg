---
title: Cartridges and sqcart
tags: [usermanual, sqcart]
---

# Cartridges and `sqcart`

A cartridge is a `.sq` file: a container holding a manifest and a payload tree.
Templates, kits, packages and assets all ship as cartridges. `sqcart` is the
tool for reading and building them.

You do not need `sqcart` to use squared-pg. You need it if you are **authoring**
a cartridge, or want to see inside one.

## Looking inside

```sh
sqcart info <cartridge>       # human summary of the manifest
sqcart show <cartridge>       # the manifest JSON
sqcart list <cartridge>       # entries: path, size, compressed, method
sqcart cat <cartridge> <entry>  # one entry's bytes to stdout
sqcart verify <cartridge>     # full validation
sqcart digest <cartridge>     # content digest
```

`stdout` carries machine-readable results only; diagnostics go to `stderr`, so
piping is safe.

## Building one

```sh
sqcart pack <directory> -o <out.sq>
sqcart unpack <cartridge> -d <dir>
```

`-o` takes a **file path, not a directory.** Passing a directory currently
fails with `cartridge.io_failed: failed to open output file for writing`, which
names the syscall rather than the mistake.

To start a new cartridge from a directory of files:

```sh
sqcart create <dir> --kind=kit -o <out.sq>
```

`create` seeds `SQ-INF/manifest.json` and then packs. It needs `--kind`; any
token matching `[a-z][a-z0-9_]*` is accepted, because what a kind *means* is
the consuming tool's question, not sqcart's. `--in-place` writes the manifest
without packing. `--force` reseeds an existing manifest.

The directory must contain at least one file — `create` on an empty directory
reports it has nothing to package.

## Options worth knowing

| Option | Effect |
|---|---|
| `-c, --compress=0-9` | deflate level, default 6. `0` stores. |
| `--store` | same as `--compress=0` |
| `--no-hashes` | omit `SQ-INF/hashes.json` |
| `--verify-integrity` | check declared digests while opening |
| `--lenient` | open at lenient conformance rather than strict |
| `--overwrite` | allow unpack to overwrite existing files |
| `--symlinks=skip\|follow\|reject` | default `skip`. `follow` resolves links whose target stays inside the cartridge; links escaping it are never packed whatever the mode. |

## Exit codes

| Code | Meaning |
|---|---|
| 0 | success |
| 1 | operation failed |
| 2 | usage error |
| 3 | cartridge non-conforming |

## Authoring a kit

`sqpg` has a template for it, which gets the directory layout and a manifest
skeleton right:

```sh
sqpg new my_kit -t template.kit \
  -p project_name=mykit -p description="What it does"
```

The workspace name must be a C identifier; the kit's identity comes from
`project_name`, so that command produces `kit.mykit`. Edit
`cartridge/SQ-INF/manifest.json` and the payload under `cartridge/tree/`, then
pack.

## Not documented here

The manifest schema, conformance levels, and the container format itself.
Those are specification material rather than user documentation, and a reader
authoring a cartridge seriously will need them.
