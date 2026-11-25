# package.{{project_name}}

{{description}}

Made by squared-pg {{generator_version}} from {{template_id}} {{template_version}}.

This workspace makes a **package**: reusable squared code that a workspace
takes in as source, in the cartridge in `cartridge/`, exploded.

```
cartridge/                 the package: what ships
  SQ-INF/manifest.json     what it is, provides and needs
  tree/                    what it adds to a workspace that uses it
    sq_pkg/{{project_name}}/{{project_name}}.cppm   its C++ module
    sq/build/package_{{project_name}}.lua           its part of the build
build.lua                  yours
sq/build/cartridge.lua     the template's: pack and install
```

```sh
sqmake            # pack it into dist/package.{{project_name}}.sq (sqdrive checks it)
sqmake install    # install it, exploded, where sqpg finds it
sqpg new probe -t template.terminal.cpp --package package.{{project_name}}
```

In the new workspace, `import {{project_name}};`.

A kit bridges something outside squared; a package is squared code itself.
