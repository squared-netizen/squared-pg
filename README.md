squared-pg is an offline-first project generator for applications built on the Squared framework. Its first frontend is an Android ARM64 C++20 host using SDL2 and a private Lua 5.4.8 scripting runtime. SDL2 is a selected frontend, not part of the generator's permanent identity; later templates may target SFML, PDCurses, or other libraries without shipping unused frontends in a generated application.



   - transactional creation beneath ~/sandbox or ~/projects;
   - non-destructive promote and demote commands;
   - offline kit and Gradle-Wrapper registration;
   - generator-only template-provider selection with no generated runtime dispatcher;
   - a Lua-driven local Android build command;
   - an optional, manually dispatched GitHub Actions build;
   - deterministic APK-asset Lua module loading;
   - versioned, capability-limited application plug-ins;
   - Obsidian-compatible Markdown documentation;
   - Doxygen comments for public C++ APIs and LDoc comments for Lua APIs.
   - a local squared-pg docs command and optional manual documentation workflow.
   - a transactionally loaded, libGDX-compatible multi-page TextureAtlas;
