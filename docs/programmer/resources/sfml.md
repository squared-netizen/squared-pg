---
title: template.android.sfml and kit.sfml
tags: [programmer, resources, sfml, android]
---

# template.android.sfml and kit.sfml

Counterpart: [[developer/resources/sfml-android]] · End users: [[usermanual/android-sfml]]

For people selecting, composing or authoring against these resources. For
writing application code inside a generated workspace, see the user manual.

## Selecting them

```sh
sqpg new <name> -t template.android.sfml -k kit.sfml \
  -p project_name=<identifier> -p package_name=<java.package.name>
```

`kit.sfml` is **required**, not optional. The template declares it in
`requires.kits.required`, and generation without it is refused before anything
is written:

```
error: template template.android.sfml requires kit kit.sfml, which the
       workflow did not select
  code:      template.kit.required
```

## Parameters

| Name | Type | Required | Notes |
|---|---|---|---|
| `project_name` | `identifier` | yes | Library name, C++ namespace and log tag. A C identifier — dots are refused. |
| `package_name` | `java_package` | yes | Android application id. Must be unique on the device. |
| `app_label` | `string` | no | Launcher label; defaults from `project_name`. |
| `description` | `string` | no | One line, used in the README. |

## What the template declares

- `platforms: ["android"]`, `working_directory: sq_app`, `processor: substitute`
- `integration_areas`: `app.include`, `build.make.fragments`,
  `android.manifest.features`
- **No `render.backend` area.** `template.android.cpp` declares it single-arity
  so two renderers are refused. Here SFML is not a swappable backend — it is
  the platform — so the area would arbitrate nothing.
- Ownership: `mk/squared_generated.mk` and `mk/squared_android_package.mk` are
  `generated`; everything else defaults to `user`.

## What the kit declares

- `external`: `sfml` 3.1.0, `acquisition: vendored`
- `runtime.model: callback`
- `compatible_templates: ["template.android.sfml"]`
- `provides`: `platform.entrypoint`, `window.surface`, `gl.context`,
  `input.touch`
- `binary`: ABI, API level, linkage and the three archive paths (D-069). **Read
  by nothing today** — see Q-35.
- Ownership: `sq_kit/**` plus the literal `mk/kit_sfml.mk`, per D-064

## Why kit.sfml cannot be used with template.android.cpp

`compatible_templates` names only the SFML template, and the reason is not
preference. SFML defines `ANativeActivity_onCreate`; `template.android.cpp`
uses `android_native_app_glue`, which defines the same symbol. One entry point
per process. See D-067.

The reverse also holds: `kit.opengl` is not compatible with
`template.android.sfml`. SFML brings its own EGL context.

## Build variables the kit contributes

Through `mk/kit_sfml.mk`, accumulated into the template's variables:

```
SQ_KIT_CPPFLAGS += -Isq_kit/include
SQ_KIT_CPPFLAGS += -idirafter $(SQ_NDK_INC)
SQ_KIT_LDLIBS   += <three archives> <three NDK stubs>
SQ_KITS_PRESENT += kit.sfml
```

`SQ_NDK_INC` and `SQ_NDK_LIB` are located by the **template**, not the kit —
the NDK is what makes the output an Android project at all, and would be needed
whatever library drew the pixels.

## Diagnostics

`make sfml-status` in a generated workspace reports the kit's `BUILD-INFO`,
archive sizes, whether `ANativeActivity_onCreate` is defined in the payload,
and whether each NDK stub is present. It is the first thing to run when a
generated project does not link.

## Limitations

- **Window, GLES context and input only.** Graphics and Audio are not built;
  the headers ship but the archives do not exist (Q-34).
- **One ABI, one API level** per kit build (Q-35).
- The kit's payload is not in git. A fresh clone has the manifest and the make
  fragment; the archives arrive from a release cartridge or from
  `tools/build-sfml.sh --install`.
