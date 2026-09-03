# The `sqpg` CLI

The reference host. It contains no argument grammar of its own — everything
below is implemented by `workflow.generate.default`, and you can replace it.

## Commands

```sh
sqpg new <name> --template ID [options]     generate a workspace
sqpg plan <name> --template ID [options]    show what would be generated
sqpg list [template|kit|package|asset]      list indexed resources
sqpg show <id>                              resolve one resource and describe it
sqpg inspect <workspace>                    read a workspace metadata record
sqpg describe                               engine version, services, capabilities
sqpg operations                             enumerate the control surface
sqpg help
```

## Options for `new` and `plan`

| Option | Meaning |
|---|---|
| `-t, --template ID` | template identity, optionally `id@range` |
| `-k, --kit ID` | kit identity; repeat for several, applied in order |
| `--package ID` | package identity; repeat |
| `--asset ID` | asset bundle identity; repeat |
| `--platform NAME` | target platform; repeat (default `termux`) |
| `--framework VER` | Squared framework version (default `1.0.0`) |
| `-o, --out PATH` | workspace location (default `./<name>`) |
| `-p, --param key=value` | template parameter; repeat |
| `--config FILE` | Lua file returning a table of the above |
| `--json` | machine-readable output |
| `--verbose` | print informational diagnostics too |

**Precedence:** command line, then `--config` file, then defaults.

## Host options

Interpreted before the workflow runs, because something must choose which script
runs before a script is running.

| Option | Meaning |
|---|---|
| `--workflow ID` | run a different workflow |
| `--fast` | relax durability to journal transitions only |

| Environment | Meaning |
|---|---|
| `SQUARED_PG_RESOURCES` | colon-separated resource roots |
| `SQUARED_PG_WORKFLOWS` | colon-separated workflow search path |
| `NO_COLOR` | disable colour |

## Examples

```sh
# what is available
sqpg list

# what would happen
sqpg plan hello --template template.termux.cpp --kit kit.terminal

# do it
sqpg new hello --template template.termux.cpp --kit kit.terminal
cd hello && make && ./build/hello

# with a Lua script workspace as well
sqpg new hello --template template.termux.cpp --kit kit.terminal --kit kit.lua

# from a config file
cat > project.lua <<'EOF'
return {
  ["template"] = "template.termux.cpp",
  kits = { "kit.terminal", "kit.lua" },
  platforms = { "termux" },
  parameters = { description = "My terminal tool." },
}
EOF
sqpg new hello --config project.lua

# for a script
sqpg plan hello --template template.termux.cpp --kit kit.terminal --json
```

## Exit status

| Status | Meaning |
|---|---|
| 0 | success |
| 1 | the operation failed; the error was printed |
| 2 | usage error, or no such workflow |
