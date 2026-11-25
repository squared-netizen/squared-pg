---
title: Cartridges and sqcart
tags: [usermanual, sqcart]
---

# Cartridges and `sqcart`

A cartridge is a `.sq` file, or the same thing as a folder (exploded): a
manifest, `SQ-INF/manifest.json`, and a payload tree. Templates, kits,
packages, assets and workspaces are all cartridges, in the format of the
[Squared Cartridge Specification 3.0][spec]. `sqcart` is the tool for
reading and building them; it is sqcart 3's (`sqcart3`, installed with sqpg
as `sqcart`). sqcart 2 and its format are deprecated, and no squared tool
reads them.

You do not need `sqcart` to use squared-pg. You need it if you want to see
inside a `.sq`, or to change one in place. A workspace is packed and
exploded by square (`square pack`, `square explode`), and the cartridge an
authoring workspace makes by `sqmake` (below); both through sqdrive.

[spec]: https://github.com/squared-netizen/sqcart/blob/main/docs/developer/specs/sq-cartridge-3.0.md

## Looking inside

```sh
sqcart info <file.sq>           # id, version, kind; commit, digest, signatures
sqcart ls <file.sq>             # entries: flags, size, codec, path
sqcart cat <file.sq> <entry>    # one entry's bytes to stdout (verified)
sqcart check <file.sq>          # every entry read and verified (§12)
```

`check` exits 1 for a cartridge that does not conform, and says why.

## Building one

```sh
sqcart pack <folder> <file.sq>  # a new cartridge from an exploded one
sqcart unpack <file.sq> <folder> [--merge] [--executable]
```

`pack` makes a new file: it refuses one that is there already. Its options:

| Option | Effect |
|---|---|
| `--keep .NAME` | pack a dot folder it would leave out (repeatable) |
| `--leave-out PATH` | leave a file or folder out (repeatable) |
| `--drop-orphans` | drop records in `SQ-INF/entries.json` for files that are gone, rather than refusing |
| `--sign KEYFILE --signer NAME` | sign it (`sqcart keygen KEYFILE` makes a key) |

A `.sq` can also be changed in place, each change a new commit:
`sqcart new`, `put`, `rm`, `mv`, `truncate`, and `compact` to drop what no
commit needs any more. `sqcart` with no arguments lists them all.

## Exit codes

| Code | Meaning |
|---|---|
| 0 | success |
| 1 | operation failed, or the cartridge does not conform |
| 2 | usage error |

## Authoring a kit, a package or a template

`sqpg` has a template for each, which gets the layout and the manifest right:

| Template            | Makes                                                    |
|---------------------|----------------------------------------------------------|
| `template.kit`      | `kit.NAME`: bridges something outside squared; a module  |
| `template.package`  | `package.NAME`: reusable squared code; a module          |
| `template.template` | `template.NAME`: a template of your own                  |

```sh
sqpg new mykit -t template.kit -p description="What it does"
cd mykit
sqmake            # pack cartridge/ into dist/kit.mykit.sq (sqdrive checks it)
sqmake install    # install it, exploded, into your own resources
sqpg new probe -t template.terminal.cpp -k kit.mykit
```

The workspace is a cartridge itself (`SQ-INF/manifest.json`, kind
`workspace`); what it makes is the exploded cartridge in `cartridge/`. Edit
`cartridge/SQ-INF/manifest.json` and the payload under `cartridge/tree/`.
The name must be a C identifier; the identity comes from `project_name`, so
`mykit` makes `kit.mykit`.

Your own resources are in `$XDG_DATA_HOME/squared-pg/resources` (else
`~/.local/share/squared-pg/resources`), in `templates/`, `kits/`,
`packages/` and `assets/`; sqpg and square search it, so a template you
install there is listed by `sqpg list template` (and by New in sqIDE).

## Not documented here

The manifest schema, conformance levels, and the container format itself.
Those are specification material rather than user documentation, and a reader
authoring a cartridge seriously will need them.
