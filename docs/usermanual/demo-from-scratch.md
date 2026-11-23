---
title: From sqterm to an app on the phone
tags: [usermanual]
---

# From sqterm to an app on the phone

Every step from sqterm to an app of your own running on the phone: install
the tools, generate an Android workspace, build it and install it. Nothing
here needs a computer, adb or Wi-Fi; the network is needed only for `pkg`.

| Step | What you end up with |
|---|---|
| 1. Install | `sqpg`, `sqmake` and squared, from sqterm's package repository |
| 2. Generate | an Android workspace, `~/projects/demo` |
| 3. Build and run | the template's starter app, installed and started |
| 4. The demo app | the demo's own code from `squared-examples` |

(Working on squared-pg itself, from a clone: BUILDING.md.)

## 1. Install

```bash
pkg install squared-pg sqmake squared
```

sqpg and its templates, kits and packages; sqmake, which builds; and the
squared framework, which every workspace builds against
(`$PREFIX/share/squared/<version>`). sqpg keeps no environment of its own:
there is nothing to initialize.

## 2. Generate

```bash
mkdir -p ~/projects && cd ~/projects
sqpg plan demo -t template.android.cpp -p package_name=com.example.demo
sqpg new  demo -t template.android.cpp -p package_name=com.example.demo
```

`plan` prints every file that would be written and who owns it, and changes
nothing. `new` writes the workspace to `./demo`.

| Part | Meaning |
|---|---|
| `demo` | the project name: library name (`libdemo.so`), C++ namespace and log tag; a C identifier |
| `-t template.android.cpp` | the shape: NativeActivity, C++23, no Java, no Gradle |
| `-p package_name=com.example.demo` | the Android application id; must be unique on the device |
| `--package package.squared-core` | optional: vendors the framework into `demo/squared/`, for a workspace that changes squared itself, or that builds with make |

Optional: `-p app_label="Squared Demo"` sets the launcher label, which
otherwise defaults to the project name.

The workspace is a cartridge folder: `SQ-INF/manifest.json` says what it was
made from and where its parts are (square reads it), and `SQ-INF/files.json`
lists every generated file with its class and checksum.

## 3. Build and run

```bash
cd demo
sqmake                  # build/libdemo.so, packaged and signed into build/demo.apk
sqmake install          # hand the APK to Android's installer (sqterm-install)
sqmake run              # install, then start it (sqterm-launch)
sqmake --debug run      # the build you debug with, in build/debug/
```

Android asks once per install; the first time, it sends you to Settings,
Install unknown apps, to allow sqterm.

`app.squared` in `build.lua` picks the squared it builds against: a version
(`"1.0.0"`) for that installed copy, or a folder for a vendored one; unset,
`./squared` when there is one, else the newest installed. Its compiles are
shared with every other workspace through sqmake's cache.

What runs is the template's starter app. The screen is dark green if the
bundled skin was found and dark red if not, and the colour changes when you
touch it. That proves the whole chain: window, EGL, events, update, render,
assets.

**A name is used once.** `sqpg new` refuses a folder that already exists.
To generate again, remove the old one or pick a new name.

## 4. The demo app itself

The demo is kept in `squared-examples`, with its framework copy committed
beside it, so a clone builds with nothing else:

```bash
cd ~/projects
git clone https://github.com/squared-netizen/squared-examples
cd squared-examples/demo
make apk && make install
```

A development build sends its log, a crash backtrace and frame-allocation
reports to a port on this device: start `nc -klu 5555` in a second session,
then `make DEV_LOG=1 install` (`DEV_LOG=1` on the line that installs).

## When something goes wrong

| Symptom | Cause and fix |
|---|---|
| `sqpg: command not found` | `pkg install squared-pg` |
| `error: the output location already exists` | the workspace name is taken; remove it or use another name |
| `sqmake: ... squared` not found | `pkg install squared` |
| `nc` shows nothing | the installed APK is not a `DEV_LOG` build, or `nc` was started after the app |
| the app opens and closes at once | build with `sqmake --debug run`; its log reaches the Console and `nc` |

## See also

- [[usermanual/getting-started]]: the same tool, in short
- [[usermanual/workspace]]: which files in a workspace are yours
