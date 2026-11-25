# kit.{{project_name}}

{{description}}

Made by squared-pg {{generator_version}} from {{template_id}} {{template_version}}.

This workspace makes a **kit**: the cartridge in `cartridge/`, exploded.

```
cartridge/                 the kit: what ships
  SQ-INF/manifest.json     what it is and declares
  tree/                    what it adds to a workspace that uses it
    sq_kit/{{project_name}}/{{project_name}}.cppm   its C++ module
    sq/build/kit_{{project_name}}.lua               its part of the build
build.lua                  yours
sq/build/cartridge.lua     the template's: pack and install
```

```sh
sqmake            # pack it into dist/kit.{{project_name}}.sq (sqdrive checks it)
sqmake install    # install it, exploded, where sqpg finds it
sqpg new probe -t template.terminal.cpp -k kit.{{project_name}}
```

In the new workspace, `import {{project_name}};`.
