---
title: From a clone to the demo app
tags: [usermanual, android, demo]
---

# From a clone to the demo app

Every step from an empty Termux to the demo app running on the phone: clone,
assemble, build, install the tool, generate an Android workspace, build it,
and get the demo app's own code. Run each command in bash; nothing here needs
a computer, adb or Wi-Fi.

The network is needed for installing packages (step 1), cloning (steps 2 and
7) and `tools/bootstrap.sh` (step 3). Nothing after that uses it.

| Step | What you end up with |
|---|---|
| 1. Termux packages | a compiler, `make`, git, the Android SDK and NDK, and the packaging tools |
| 2. Clone | `~/projects/squared-pg` |
| 3. Bootstrap | the framework (`squared`) and `sqcart` cloned inside it |
| 4. Build | `build/sqpg`, tested |
| 5. Initialize | `~/sqsysroot`, with `sqpg` and `sqcart` on your PATH |
| 6. Generate | an Android workspace in the sandbox, built and installed |
| 7. The demo app | the demo's own code from `squared-examples`, built and installed |

## 1. Termux packages

Termux's own repository has most of the Android tooling. The SDK, with the
NDK and the platform `android.jar`, comes from the
[Termux Void](https://termuxvoid.github.io) repository, which is added once:

```bash
curl -sL https://github.com/termuxvoid/repo/raw/main/install.sh | bash
```

That pipes a script from the network into bash. Download it and read it first
if you prefer: `curl -sLO https://github.com/termuxvoid/repo/raw/main/install.sh`.

Then:

```bash
pkg install git clang make
pkg install android-sdk ndk-sysroot aapt2 apksigner openjdk-17 netcat-openbsd
```

| Package | From | For |
|---|---|---|
| `git`, `clang`, `make` | Termux | building squared-pg and every workspace |
| `android-sdk` | Termux Void | the NDK and the platforms, under `$PREFIX/opt/android-sdk/` |
| `ndk-sysroot` | Termux | NDK headers and libraries for Termux's own clang |
| `aapt2`, `apksigner` | Termux | packaging and signing the APK |
| `openjdk-17` | Termux | `keytool`, which creates the debug signing key |
| `netcat-openbsd` | Termux | `nc`, to read a development build's log |

A workspace looks for the NDK and `android.jar` here, and the first match
wins:

| What | Where it looks |
|---|---|
| NDK | `$ANDROID_NDK_HOME`, then `$PREFIX/opt/android-sdk/ndk/<newest version>`, then `$ANDROID_HOME/ndk/*`, `$ANDROID_SDK_ROOT/ndk/*` |
| `android.jar` | `$PREFIX/opt/android-sdk/platforms/android-*/`, then the same under `$ANDROID_HOME` and `$ANDROID_SDK_ROOT` |

With `android-sdk` installed, the second NDK location is the one found
(`ndk/29.0.14206865` on the phone this page was written on). Step 6 runs
`make android-status`, which reports what the build found and what it is
missing. That is the check for this step.

## 2. Clone squared-pg

```bash
mkdir -p ~/projects
git clone https://github.com/squared-netizen/squared-pg ~/projects/squared-pg
cd ~/projects/squared-pg
```

`gh repo clone squared-netizen/squared-pg` does the same if you use the GitHub
CLI.

## 3. Bootstrap: fetch the framework and sqcart

squared-pg is three repositories. `tools/bootstrap.sh` clones the other two
into place:

```bash
tools/bootstrap.sh
```

| Repository | Lands in | What it is |
|---|---|---|
| sqcart | `sqcart/` | the cartridge library and tool, compiled into the build |
| squared | `resources/.squared/` | the framework, templates, kits and packages |

`resources/templates`, `resources/kits`, `resources/packages` and
`resources/assets` are links into `resources/.squared`.

It ends with:

```text
Assembled. Build with:

  make -j4 && make check
```

**Which framework you get.** Each component is checked out at the revision
pinned in `SQUARED_VERSION` and `SQCART_VERSION`, on a branch named
`pin/<revision>`. If the pin is older than the framework you want (check with
`tools/bootstrap.sh --check`), move both components to their latest commits:

```bash
tools/bootstrap.sh --latest
```

That also records the new revisions in the two version files. Commit those
files: they are what a fresh clone gets.

## 4. Build

```bash
make -j4 && make check
```

This builds the engine, the `sqpg` CLI and the tests, then runs them. Progress
prints as it goes. `make smoke` additionally generates a terminal project,
builds it and runs it.

## 5. Initialize: install the tool

Run this from the checkout:

```bash
./build/sqpg initialize
```

```text
environment /data/data/com.termux/files/home/sqsysroot
  project    promoted work; version controlled
  sandbox    experiments; nothing here is irreplaceable
  .sqpg      tools, resources and workflows
...
put the tool on your PATH:
  ln -sf /data/data/com.termux/files/home/sqsysroot/.sqpg/bin/sqpg /data/data/com.termux/files/usr/bin/sqpg
  ln -sf /data/data/com.termux/files/home/sqsysroot/.sqpg/bin/sqcart /data/data/com.termux/files/usr/bin/sqcart
```

Run the two `ln -sf` lines it prints. They are the only change made outside
`~/sqsysroot`, and no shell profile is edited.

```bash
ln -sf "$HOME/sqsysroot/.sqpg/bin/sqpg" "$PREFIX/bin/sqpg"
ln -sf "$HOME/sqsysroot/.sqpg/bin/sqcart" "$PREFIX/bin/sqcart"
sqpg doctor        # what is installed, and whether it is stale
```

The environment:

```text
~/sqsysroot/
  sandbox/    try things here; nothing in it is irreplaceable
  project/    work worth keeping; version controlled
  .sqpg/      the installed sqpg, sqcart, resources and workflows
```

`sqpg` generates from `~/sqsysroot/.sqpg/resources`, which is a copy, not from
the checkout. Step 8 covers keeping that copy current.

## 6. Generate the workspace the demo was made from

The demo was generated with this command. The workspace's own record,
`.squared/metadata.json`, lists the template, the package and the
parameters:

```bash
cd ~/sqsysroot/sandbox
sqpg plan demo -t template.android.cpp --package package.squared-core -p package_name=com.example.demo
sqpg new  demo -t template.android.cpp --package package.squared-core -p package_name=com.example.demo
```

`plan` prints every file that would be written and who owns it, and changes
nothing. `new` writes the workspace to `./demo`.

| Part | Meaning |
|---|---|
| `demo` | the project name: library name (`libdemo.so`), C++ namespace and log tag; a C identifier |
| `-t template.android.cpp` | the shape: NativeActivity, C++20, no Java, no Gradle |
| `--package package.squared-core` | vendors the framework into `demo/squared/`. **Not optional in practice**: without it you get a warning that the template is written against squared-core, and a workspace with no framework |
| `-p package_name=com.example.demo` | the Android application id; must be unique on the device |

Optional: `-p app_label="Squared Demo"` sets the launcher label, which
otherwise defaults to the project name. Leave out `--platform`. It only
filters resources, and the demo was generated without it.

Build and install:

```bash
cd demo
make android-status     # every tool and path the build found
make apk                # build/libdemo.so, packaged and signed into build/demo.apk
make install            # hand the APK to the package installer
```

The first `make apk` creates a debug keystore in `~/.android/` if there is
none. `make install` opens Android's installer; confirm there.

What runs is the template's starter app. The screen is dark green if the
bundled skin was found and dark red if not, and the colour changes when you
touch it. That proves the whole chain: window, EGL, events, update, render,
assets. The demo's screens are not in the template. They live in the
`squared-examples` repository (step 7).

**A name is used once.** `sqpg new` refuses a directory that already exists.
To generate again, remove the old one or pick a new name:

```bash
cd ~/sqsysroot/sandbox && sqpg new test1 -t template.android.cpp --package package.squared-core -p package_name=com.example.test1
```

## 7. The demo app itself

The demo is kept in `squared-examples`, with its framework copy committed
beside it, so a clone builds with nothing else:

```bash
cd ~/sqsysroot/project
git clone https://github.com/squared-netizen/squared-examples
cd squared-examples/demo
make apk && make install
```

A development build sends its log, a crash backtrace and frame-allocation
reports to a port on this device. Start the listener first, in a second Termux
session:

```bash
nc -klu 5555
```

Then build and install in one step:

```bash
make DEV_LOG=1 install
```

> [!warning] `make DEV_LOG=1 apk && make install` installs the wrong build
> `install` rebuilds with the flags it is given. Without `DEV_LOG=1` on the
> `install` line it quietly rebuilds a plain APK and installs that, and `nc`
> receives nothing. Put `DEV_LOG=1` on the command that installs.

Updating the demo's framework copy from the package, and committing it, is a
maintainer task: `squared-examples/MAINTAINING.md`.

## 8. After changing a template, kit or the framework

The installed copy does not follow the checkout. Re-install from the checkout:

```bash
cd ~/projects/squared-pg && ./build/sqpg initialize
```

Running the *installed* `sqpg initialize` reports success and copies nothing.

A re-install copies over the old files but does not delete files that were
removed upstream. For a clean install, remove the installed tool first:

```bash
rm -rf ~/sqsysroot/.sqpg
cd ~/projects/squared-pg && ./build/sqpg initialize
```

> [!danger] `sqpg selfdestruct --environment` deletes `project/` too
> It removes everything in `sandbox/`, **everything in `project/`**, and
> `.sqpg`. That includes `~/sqsysroot/project/squared-examples`. To start
> clean, use the two commands above, and `sqpg selfdestruct --sandbox` if you
> also want the sandbox emptied.

Existing workspaces are not updated. squared-pg generates new workspaces only,
so generate a fresh one to see a template change.

## 9. Keeping a workspace

```bash
sqpg promote demo      # sandbox/demo -> project/demo, and git init
```

## When something goes wrong

| Symptom | Cause and fix |
|---|---|
| `no workflow '…' on the workflow search path` | `sqpg` cannot find its installation: re-run step 5, or run it from the checkout as `./build/sqpg` |
| `error: the output location already exists` | the workspace name is taken; remove it or use another name |
| `No Android NDK found` | see step 1; `make android-status` lists the paths searched |
| a template change does not show up in a new workspace | the installed copy is stale: step 8 |
| `nc` shows nothing | the installed APK is not a `DEV_LOG` build (step 7), or `nc` was started after the app |
| the app opens and closes at once | build with `DEV_LOG=1`; the crash backtrace arrives at `nc`. If nothing arrives, take an Android bug report |

## See also

- [[usermanual/getting-started]]: the same tool from the terminal-template
  side
- [[usermanual/workspace]]: which files in a workspace are yours
- `squared/docs/programmer/dev.md`, inside any workspace: the log sink, the
  crash reporter and the frame-allocation counter
