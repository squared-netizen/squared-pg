---
title: "`sqpg doctor` reports an installed tool as a broken checkout"
tags: [report, lua, doctor, install]
---

# `sqpg doctor` reports an installed tool as a broken checkout

```
Report run:  2026-10-18
Scope:       lua/squaredpg/doctor.lua, lua/squaredpg/lifecycle.lua,
             app/src/main.cpp, engine/tests/test_cli.cpp
Environment: Termux (aarch64 Android), sqsysroot at ~/sqsysroot
Status:      fixed, deployed, regression test added
```

## Summary

`sqpg doctor`, run from the installed environment at `~/sqsysroot`, reported

```text
components
  STOP  sqcart                 absent
  STOP  squared                absent
```

and exited **1**, while the section immediately below it reported `ok  sqcart
installed`, and `sqcart` was in fact present at `~/sqsysroot/.sqpg/bin/sqcart`
and on `PATH`. The report contradicted itself, named a fix that exists nowhere
in an installation (`./tools/bootstrap.sh`), and the non-zero status made the
whole command look like a failure.

Cause: the components check was written for a **source checkout** and was
handed an **installed tool**. The two layouts are different trees, and nothing
told the check which one it was looking at.

## Symptoms

From `~/sqsysroot`, before the fix:

```text
sqpg   doctor
installation /data/data/com.termux/files/home/sqsysroot/.sqpg

components
  STOP  sqcart                 absent
  STOP  squared                absent

resources
  ok    resources/templates    4 resource(s)
  ...

environment
  ok    environment            /data/data/com.termux/files/home/sqsysroot
  ok    sqpg                   installed
  ok    sqcart                 installed
  ok    PATH                   /data/data/com.termux/files/usr/bin/sqpg

suggested:
  ./tools/bootstrap.sh
```

Exit status: `1`.

Three things were wrong in that output:

1. `sqcart absent` — false; the binary is installed and linked on `PATH`.
2. `squared absent` — meaningless for an installation; `resources/.squared` is
   a checkout artifact that is deliberately not installed.
3. `./tools/bootstrap.sh` — no such file exists relative to `~/sqsysroot`, so
   the suggested fix could not be run.

## Root cause

`doctor.run` derives its subject from `sqpg.installation`:

```lua
local source = sqpg.installation
...
check_components(source, rep)
check_resources(source, rep)
```

(`lua/squaredpg/doctor.lua:329` and `doctor.lua:336-338`.)

For the installed binary, `sqpg.installation` is `~/sqsysroot/.sqpg`, because
the host walks up from the executable looking for `lua/workflows/` and
`resources/` together (`app/src/main.cpp:93-134`) — and `initialize` lays
`.sqpg` out exactly like a source tree on purpose, so that walk finds it with
no configuration (`lua/squaredpg/lifecycle.lua:70-82`).

`check_components` then probed that root for the things only a checkout has:

| Probe | Present in a checkout | Installed by `initialize` |
|---|---|---|
| `sqcart/` (git clone) | yes | no |
| `resources/.squared/` (git clone) | yes | no |
| `SQCART_VERSION`, `SQUARED_VERSION` | yes | no |
| `tools/bootstrap.sh` | yes | no |
| `resources/{templates,kits,packages,assets}` | symlinks | real dirs (`cp -aL`) |
| the tools themselves, at `<root>/bin/{sqpg,sqcart}` | not probed (checkout mode never looks) | yes (`lua/squaredpg/lifecycle.lua:88-170`) |

Every component probe missed, so both rows fell into the `absent → level 3`
branch, `worst` became 3, and `doctor.run` returned 1.

The file already contained the sentence that should have applied:

```lua
if not any then
  rep:line(1, "components", "none; this looks like an installed tool, not a checkout")
end
```

It was dead code. `any = true` was assigned in *every* branch of the loop —
absent, present-but-not-a-repo, and repo — so `not any` was reachable only if
the spec list were empty.

## Secondary defect

The resource counter ran

```lua
"find " .. path .. " -mindepth 1 -maxdepth 1 -type d | wc -l"
```

`find` in its default `-P` mode does not descend through a command-line
symlink. In a checkout `resources/templates` is a symlink into
`.squared/templates`, so every namespace in a checkout counted **0** and was
silently omitted from the report. The installed tree, whose namespaces are real
directories after `cp -aL`, counted correctly — so the same command printed
`4 resource(s)` from `~/sqsysroot` and nothing at all for those namespaces from
the checkout.

## Fix

All edits in this repository.

### `lua/squaredpg/doctor.lua`

1. **`is_checkout(root)`** (`doctor.lua:94-109`) — tests for the markers only
   a source tree carries: `tools/bootstrap.sh`, either pin file, `.git`,
   `sqcart/`, `resources/.squared`.

2. **`check_components` branches on it** (`doctor.lua:139-166`). For an
   installed layout it now reports what is actually true there:

   ```text
   components
     note  components             installed tool; pins and checkouts live in the source tree
     ok    sqpg                   installed in .sqpg/bin
     ok    sqcart                 installed in .sqpg/bin
     ok    git                    /data/data/com.termux/files/usr/bin/git
     ok    make                   /data/data/com.termux/files/usr/bin/make
   ```

   A missing tool is a `warn` with the existing fix (`make -C sqcart && sqpg
   initialize`); a missing host dependency is a `warn` with `install git` /
   `install make`. Those two are the only external commands the Lua layer
   invokes as features: `git` for `promote`/`demote` repository initialisation
   and for the component state reads, `make` for the workflow verbs. The
   checkout path is unchanged: absent/pin-mismatch/behind still warn at the
   same levels with the same bootstrap suggestions.

3. **Dead branch removed** — the unreachable `if not any` clause and its three
   `any = true` assignments.

4. **`find -H`** (`doctor.lua:224`) — symlinked resource roots now count.

### `engine/tests/test_cli.cpp`

`doctor_reports_without_repairing()` gained a regression case
(`engine/tests/test_cli.cpp:531-547`): after `initialize`, it runs the
*installed* `.sqpg/bin/sqpg doctor` and asserts exit `0`, no `"absent"` anywhere
in the output, and `sqcart` present.

## Verification

| Check | Command | Result |
|---|---|---|
| Installed environment | `cd ~/sqsysroot && sqpg doctor` | exit **0**, `sqcart installed in .sqpg/bin`, no `STOP`, `nothing to report.` |
| Checkout unchanged | `./build/sqpg doctor` (in the repo) | unchanged output; exit 1 only for the genuine `squared` pin warning (`main 52858b4; pinned to 43bc63c`) |
| Resource counting | `./build/sqpg doctor` in the repo | `4 / 7 / 1 resource(s)` with `-> .squared/*`, previously omitted |
| Unit tests | `./build/test_cli` | `ok test_cli` |
| Smoke | `make smoke` | `all checks passed` |
| Full suite | `make check` | only pre-existing failure, see below |

Negative probes, run against a scratch `SQSYSROOT`:

- `sqcart` deleted from `.sqpg/bin` → `warn sqcart not installed in .sqpg/bin`,
  `warn sqcart not installed in the environment`, fix suggested, exit `1`.
- `PATH` stripped of `git` and `make` → `warn git not found on PATH`,
  `warn make not found on PATH`, `install git` / `install make` suggested,
  exit `1`.

### Pre-existing failure, unrelated

`make check` fails at `engine/tests/test_generate.cpp:511
(FAIL header != nullptr)` followed by a segfault. It reproduces on a pristine
tree with these changes stashed, and is not touched by this work.

## Deployment

The Lua layer is read from disk at run time — the host builds `package.path`
from the installation's `lua/workflows` directory
(`app/src/main.cpp:321-328`). Editing `lua/squaredpg/doctor.lua` in the
checkout therefore changes nothing for the installed tool until the tree is
re-copied:

```sh
cd ~/projects/squared-pg
make -j4 && ./build/test_cli
./build/sqpg initialize        # from the checkout; the installed copy skips itself
cmp lua/squaredpg/doctor.lua ~/sqsysroot/.sqpg/lua/squaredpg/doctor.lua
cd ~/sqsysroot && sqpg doctor
```

Running `sqpg initialize` from the *installed* tool is a no-op by design
(`lua/squaredpg/lifecycle.lua:104-107`), which is why the command above is run
from the checkout. This is the rough edge already recorded as Q-31/Q-32 in
`docs/usermanual/getting-started.md`.

## Not changed

- **Exit-status policy.** Unchanged: 1 on any warning or stop, 0 otherwise. The
  installed environment now earns a 0 because it is healthy, not because the
  rule was relaxed.
- **The `environment` section's tool checks.** They are keyed on `$SQSYSROOT`,
  not on the installation, and still carry the staleness comparison against a
  freshly built copy — so `sqpg`/`sqcart` are named twice in an installed
  report: once as inventory (components) and once as installed-plus-staleness
  (environment). Deliberate: the two sections answer different questions.
- **`./tools/bootstrap.sh` suggestions in a checkout.** Still correct there;
  they were only ever wrong when aimed at an installation.
