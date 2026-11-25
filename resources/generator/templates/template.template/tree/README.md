# template.{{project_name}}

{{description}}

Made by squared-pg {{generator_version}} from {{template_id}} {{template_version}}.

This workspace makes a **template**: the cartridge in `cartridge/`,
exploded, that sqpg makes workspaces from.

```
cartridge/                 the template: what ships
  SQ-INF/manifest.json     what it is: its parameters, layers, roles
  tree/                    the workspace it makes, before substitution
build.lua                  yours
sq/build/cartridge.lua     the template's: pack and install
```

```sh
sqmake            # pack it into dist/template.{{project_name}}.sq (sqdrive checks it)
sqmake install    # install it, exploded, where sqpg finds it
sqpg new hello -t template.{{project_name}}
```

## Writing it

- Files in `cartridge/tree/` are copied into each workspace, with
  `{{"{{"}}project_name}}` (and every other parameter the manifest declares)
  replaced, in their text and in their paths.
- To write a literal `{{"{{"}}`, write `{{"{{"}}"{{"{{"}}"}}`.
- The manifest's `parameters` are what New asks for; `square.roles` say
  which folders are what (`app`, `build`, `docs`, `layouts`): sqIDE's
  Home shows a button for each it knows.
- `ownership` says whose each file is: `generated` (the template's, replaced
  on update) or `user` (the workspace's, never touched).
