# template.kit probe

- sqpg:      /data/data/com.termux/files/usr/bin/sqpg
- repo tpl:  resources/generator/templates/template.kit
- installed: /data/data/com.termux/files/home/sqsysroot/.sqpg/resources/generator/templates/template.kit
- tmp:       /data/data/com.termux/files/usr/tmp/tk-probe.19947

## 1. template tree, exact names

```
SQ-INF
SQ-INF/manifest.json
tree
tree/.gitignore
tree/AGENTS.md
tree/Makefile
tree/README.md
tree/cartridge
tree/cartridge/README.md
tree/cartridge/SQ-INF
tree/cartridge/SQ-INF/manifest.json
tree/cartridge/tree
tree/cartridge/tree/mk
tree/cartridge/tree/mk/kit_{{project_name}}.mk
tree/mk
tree/mk/squared_generated.mk
```

## 1b. every README.md in the template, with size and first line

```
tree/README.md
    bytes: 2016
    first: # kit.{{project_name}}
tree/cartridge/README.md
    bytes: 1391
    first: # kit.{{project_name}}
```

## 1c. does the installed copy match the repo copy?

```
identical
```

## 2. plan output (this is the important one)

```

$ /data/data/com.termux/files/usr/bin/sqpg plan probekit -t template.kit -p project_name=probe -p description=Probe --verbose
plan for probekit
  template          template.kit@0.1.0
  working directory cartridge
  workspace         ./probekit

  [34mdir [0m cartridge/
  [34mdir [0m cartridge/SQ-INF/
  [34mdir [0m cartridge/tree/
  [34mdir [0m cartridge/tree/mk/
  [34mdir [0m mk/
  [32mfile[0m .gitignore                      [35mgenerated[0m  template.kit
  [32mfile[0m AGENTS.md                       [32mseeded[0m  template.kit
  [32mfile[0m Makefile                        [32mseeded[0m  template.kit
  [32mfile[0m README.md                       [32mseeded[0m  template.kit
  [32mfile[0m cartridge/README.md             [32mseeded[0m  template.kit
  [32mfile[0m cartridge/SQ-INF/manifest.json  [32mseeded[0m  template.kit
  [32mfile[0m cartridge/tree/mk/kit_probe.mk  [32mseeded[0m  template.kit
  [32mfile[0m mk/squared_generated.mk         [35mgenerated[0m  template.kit

  13 steps
  (exit 0)
```

## 2b. plan as JSON — shows origin and disposition per step

```

$ /data/data/com.termux/files/usr/bin/sqpg plan probekit -t template.kit -p project_name=probe -p description=Probe --json
{
  "assets": {},
  "diagnostics": {},
  "framework_version": "1.0.0",
  "kits": {},
  "packages": {},
  "platforms": {},
  "project_name": "probekit",
  "step_count": 13,
  "steps": [
    {
      "action": "create_directory",
      "ownership": "seeded",
      "path": "cartridge",
      "phase": "template.instantiate"
    },
    {
      "action": "create_directory",
      "ownership": "seeded",
      "path": "cartridge/SQ-INF",
      "phase": "template.instantiate"
    },
    {
      "action": "create_directory",
      "ownership": "seeded",
      "path": "cartridge/tree",
      "phase": "template.instantiate"
    },
    {
      "action": "create_directory",
      "ownership": "seeded",
      "path": "cartridge/tree/mk",
      "phase": "template.instantiate"
    },
    {
      "action": "create_directory",
      "ownership": "seeded",
      "path": "mk",
      "phase": "template.instantiate"
    },
    {
      "action": "write_file",
      "bytes": 11,
      "origin": "template.kit",
      "origin_version": "0.1.0",
      "ownership": "generated",
      "path": ".gitignore",
      "phase": "template.instantiate"
    },
    {
      "action": "write_file",
      "bytes": 490,
      "origin": "template.kit",
      "origin_version": "0.1.0",
      "ownership": "seeded",
      "path": "AGENTS.md",
      "phase": "template.instantiate"
    },
    {
      "action": "write_file",
      "bytes": 3687,
      "origin": "template.kit",
      "origin_version": "0.1.0",
      "ownership": "seeded",
      "path": "Makefile",
      "phase": "template.instantiate"
    },
    {
      "action": "write_file",
      "bytes": 1958,
      "origin": "template.kit",
      "origin_version": "0.1.0",
      "ownership": "seeded",
      "path": "README.md",
      "phase": "template.instantiate"
    },
    {
      "action": "write_file",
      "bytes": 1336,
      "origin": "template.kit",
      "origin_version": "0.1.0",
      "ownership": "seeded",
      "path": "cartridge/README.md",
      "phase": "template.instantiate"
    },
    {
      "action": "write_file",
      "bytes": 833,
      "origin": "template.kit",
      "origin_version": "0.1.0",
      "ownership": "seeded",
      "path": "cartridge/SQ-INF/manifest.json",
      "phase": "template.instantiate"
    },
    {
      "action": "write_file",
      "bytes": 972,
      "origin": "template.kit",
      "origin_version": "0.1.0",
      "ownership": "seeded",
      "path": "cartridge/tree/mk/kit_probe.mk",
      "phase": "template.instantiate"
    },
    {
      "action": "write_file",
      "bytes": 2008,
      "origin": "template.kit",
      "origin_version": "0.1.0",
      "ownership": "generated",
      "path": "mk/squared_generated.mk",
      "phase": "template.instantiate"
    }
  ],
  "template": "template.kit",
  "template_version": "0.1.0",
  "working_directory": "cartridge",
  "workspace": "./probekit"
}
  (exit 0)
```

## 3. what  actually writes

```
created ./probekit
  template          template.kit
  working directory cartridge/
  files             9 in 5 directories

write your code in ./probekit/cartridge/ and run `make` in ./probekit

resulting tree:
.gitignore
.squared
.squared/metadata.json
AGENTS.md
Makefile
README.md
cartridge
cartridge/README.md
cartridge/SQ-INF
cartridge/SQ-INF/manifest.json
cartridge/tree
cartridge/tree/mk
cartridge/tree/mk/kit_probe.mk
mk
mk/squared_generated.mk
```

## 3b. filename substitution: did kit_{{project_name}}.mk become kit_probe.mk?

```
cartridge/tree/mk/kit_probe.mk
mk/squared_generated.mk

literal-brace names still present (a bug if any appear):
```

## 3c. README.md files in the generated workspace

```
cartridge/README.md  (1336 bytes)
    first: # kit.probe
README.md  (1958 bytes)
    first: # kit.probe
```

Compare 1b with 3c. If the template has two README.md and the workspace
has one, or one has the other's content, that is the collision.

## 3d. contents of the generated mk fragment

```
--- cartridge/tree/mk/kit_probe.mk
# kit.probe — build fragment.
#
# Included automatically: the workspace Makefile picks up mk/kit_*.mk by
# wildcard, so this fragment is live the moment the kit is installed. The name
# carries the kit's own name so two kits never collide.
#
# A kit ADDS to a workspace's build; it does not reorganise it
# (Generator Architecture 2.9). Two rules that came from real build failures:
#
#   - additional sysroots go in with -idirafter, never -I. -I prepends and
#     will displace the host's own C headers, at which point libc++ stops
#     finding them and the error appears far from its cause.
#   - name libraries by path rather than adding -L and using -l. An added -L
#     changes resolution for every -l in the link, including ones this kit
#     knows nothing about.
#
# The test: if your fragment only works when it comes first, it is
# reorganising the toolchain rather than adding to it.

# CXXFLAGS += -Isq_kit/probe/include
# LDLIBS   += /usr/lib/libprobe.a
```

## 3e. the generated cartridge manifest, substituted

```
{
  "format": "squared-cartridge",
  "format_version": 2,

  "kind": "kit",
  "id": "kit.probe",
  "version": "0.1.0",
  "tree": "tree/",

  "title": "probe kit",
  "description": "Probe",
  "license": "MIT",

  "consumers": {
    "squared_pg": {
      "external": {
        "id": "",
        "version": ">=0.0.0",
        "acquisition": "system"
      },

      "runtime": { "model": "none" },

      "platforms": ["all"],
      "compatible_templates": [],
      "integration_areas": ["build.make.fragments"],
      "conflicts_with": [],
      "provides": [],

      "requires": {
        "engine": ">=0.1.0 <0.2.0",
        "framework": ">=1.0.0 <2.0.0",
        "packages": []
      },

      "ownership": {
        "generated": ["**"],
        "user": [],
        "shared": [],
        "default": "generated"
      }
    }
  }
}
```

## 3f. does the authored cartridge pack as-is?

```
bb1b298c43efc7d5e60c337ae48c8eea91d862bb6fee9931442634fd78e679dc  /data/data/com.termux/files/usr/tmp/tk-probe.19947/probe.sq
  (exit 0)
  verify exit 0
```

## 4. directory-name substitution (invasive test)

```
added to installed copy: tree/cartridge/tree/sq_kit/include/{{project_name}}/probe.hpp

  [34mdir [0m cartridge/tree/sq_kit/
  [34mdir [0m cartridge/tree/sq_kit/include/
  [34mdir [0m cartridge/tree/sq_kit/include/probe/
  [32mfile[0m cartridge/tree/sq_kit/include/probe/probe.hpp  [32mseeded[0m  template.kit

If the path above reads sq_kit/include/probe/probe.hpp the processor
substitutes directory names. If it reads {{project_name}} literally,
it does not, and the layout cannot be templated by name.

removed probe directory from installed copy
```

## 5. context

```

$ /data/data/com.termux/files/usr/bin/sqpg show template.kit
{
  "description": "A workspace whose deliverable is a kit cartridge: edit a manifest and a payload tree, then check and pack them.",
  "entry_count": 9,
  "id": "template.kit",
  "integration_areas": [
    "build.make.fragments"
  ],
  "kind": "template",
  "parameters": [
    {
      "description": "The kit's short name; `format` becomes `kit.format`.",
      "name": "project_name",
      "required": true,
      "type": "identifier"
    },
    {
      "description": "The external dependency this kit wraps, if any. Empty for a kit that contributes only files.",
      "name": "external_id",
      "required": false,
      "type": "string"
    },
    {
      "description": "none, polled or callback. See Generator Architecture 2.9.2.",
      "name": "runtime_model",
      "required": false,
      "type": "enum"
    },
    {
      "description": "One-line description, used in the manifest and the README.",
      "name": "description",
      "required": false,
      "type": "string"
    }
  ],
  "title": "Squared kit authoring workspace",
  "version": "0.1.0",
  "working_directory": "cartridge"
}
  (exit 0)
```

---

Report written to /data/data/com.termux/files/home/projects/squared-pg/template-kit-probe.md
