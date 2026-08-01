---
title: {{PROJECT_TITLE}} API Reference
tags:
  - api
  - cpp
  - java
  - lua
---

# API Reference

Generate all API references from the project root or any subdirectory:

```sh
squared-pg docs
```

## Generated references

- [Lua runtime and plug-in API](../build/docs/lua/index.html)
- [C++ host API](../build/docs/cpp/html/index.html)
- [SDL2 Android Java wrapper](../build/docs/java/html/index.html)

The generated HTML and XML live beneath `build/docs/` and are intentionally
ignored by Git. This Markdown page remains a stable Obsidian-compatible
entry point.

## Ownership

The C++ reference describes the generated application interface, its
platform-neutral event boundary, the application host, and the public
`squared::` graphics2d, data, time, and messaging frameworks. The
Lua reference covers both the trusted generated runtime and application
plug-ins. The Java reference describes vendored SDL2 Android glue copied from
the registered offline kit. Its original SDL license and upstream ownership
remain unchanged.

## Adding source directories

Lua, C++, and Java documentation inputs are recursive. New package or module
directories beneath the configured roots are included automatically. Edit
`docs/ldoc-config.ld`, `docs/Doxyfile.cpp`, or `docs/Doxyfile.java` when a
new source tree lives outside those roots.
