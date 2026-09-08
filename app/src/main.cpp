// SPDX-License-Identifier: MIT
//
// sqpg — the reference command-line host.
//
// Specification: §2.1.6 and §2.5.4. The CLI is a thin host: it starts a Lua
// runtime, creates and initializes an engine, hands control to a workflow,
// forwards I/O, and reports the final result.
//
// **The CLI MUST NOT implement generation policy in native code.** So there
// is no argument grammar here. This file does not know what `--template`
// means, does not know that templates exist, and does not know what a kit is.
// It forwards argv verbatim to the workflow and reports what comes back.
//
// The only argument it does interpret is `--workflow`, and only because
// something has to choose which script runs before any script is running. It
// is host configuration, not generation policy.

#include "squared/pg/engine.hpp"
#include "squared/pg/lua/bindings.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// Locating our own executable has no portable spelling.
#if defined(__APPLE__)
#  include <mach-o/dyld.h>
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#  include <sys/sysctl.h>
#  include <sys/types.h>
#endif

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

namespace {

namespace pg = squared::pg;

constexpr const char* kDefaultWorkflow = "workflow.sqpg.default";

struct HostConfig {
    std::string              workflow{kDefaultWorkflow};
    std::vector<std::string> resource_roots;
    std::vector<std::string> workflow_roots;
    std::vector<std::string> forwarded;
    bool                     fast_durability{false};
};

/// This executable's own path.
///
/// Every platform spells it differently and none of the spellings is in the
/// standard library. argv[0] is the last resort rather than the first choice:
/// it is whatever the caller passed, which for a binary found on PATH is a
/// bare name that resolves to nothing useful.
[[nodiscard]] std::filesystem::path executable_path(const char* argv0) {
    std::error_code ec;

#if defined(__linux__) || defined(__ANDROID__)
    std::filesystem::path self = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec && !self.empty()) return self;
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);  // asks for the required length
    std::string buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) == 0) {
        std::filesystem::path self = std::filesystem::canonical(buffer.c_str(), ec);
        if (!ec) return self;
    }
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
    char buffer[4096];
    std::size_t size = sizeof buffer;
    if (::sysctl(mib, 4, buffer, &size, nullptr, 0) == 0 && size > 0) {
        return std::filesystem::path{buffer};
    }
#endif

    return std::filesystem::absolute(argv0 != nullptr ? argv0 : "sqpg", ec);
}

/// Whether `candidate` looks like a squared-pg installation.
///
/// Both markers, not either: `lua/` alone matches a Lua project that happens
/// to be nearby, and `resources/` alone is a common enough directory name to
/// match by accident.
[[nodiscard]] bool is_installation_root(const std::filesystem::path& candidate) {
    std::error_code ec;
    return std::filesystem::is_directory(candidate / "lua" / "workflows", ec) &&
           std::filesystem::is_directory(candidate / "resources", ec);
}

/// Where the installation lives.
///
/// Derived from the executable's own location, never the current working
/// directory: §2.7.2 forbids generation depending on the cwd, and a host that
/// only worked when launched from the repository root would violate that in
/// spirit even though the engine itself is clean.
///
/// Found by walking upward and testing for the markers, rather than by
/// recognising directory names. An earlier version special-cased "build" and
/// "bin", which meant `cmake -B build-cmake` -- or CLion's default
/// `cmake-build-debug`, or any other name -- produced a binary that could not
/// find its own workflows. Recognising the thing being looked for survives a
/// build directory called anything.
[[nodiscard]] std::filesystem::path installation_root(const char* argv0) {
    std::error_code ec;
    const std::filesystem::path self = executable_path(argv0);

    // An installed layout puts the binary in <prefix>/bin and its data in
    // <prefix>/share/squared-pg, which no amount of walking upward would find.
    const std::filesystem::path shared = self.parent_path().parent_path() / "share" / "squared-pg";
    if (is_installation_root(shared)) return shared;

    // Six levels covers a build tree nested well beyond anything reasonable
    // and still terminates on a binary sitting at the filesystem root.
    std::filesystem::path directory = self.parent_path();
    for (int depth = 0; depth < 6; ++depth) {
        if (is_installation_root(directory)) return directory;
        const std::filesystem::path parent = directory.parent_path();
        if (parent == directory) break;
        directory = parent;
    }

    // Nothing found. Return the executable's own directory so the error names
    // a real path, and let workflow lookup report what it searched.
    return self.parent_path();
}

/// Environment overrides exist so a packaged install can point elsewhere
/// without a rebuild. They configure the *host*, never generation inputs.
[[nodiscard]] std::vector<std::string> split_list(const char* text) {
    std::vector<std::string> out;
    if (text == nullptr) return out;
    std::string current;
    for (const char* c = text; *c != '\0'; ++c) {
        if (*c == ':') {
            if (!current.empty()) out.push_back(current);
            current.clear();
        } else {
            current.push_back(*c);
        }
    }
    if (!current.empty()) out.push_back(current);
    return out;
}

[[nodiscard]] HostConfig parse_host_arguments(int argc, char** argv) {
    HostConfig config;
    const std::filesystem::path root = installation_root(argc > 0 ? argv[0] : nullptr);

    config.resource_roots = split_list(std::getenv("SQUARED_PG_RESOURCES"));
    config.workflow_roots = split_list(std::getenv("SQUARED_PG_WORKFLOWS"));
    if (config.resource_roots.empty()) {
        // Two trees, and the split between them is not arbitrary.
        //
        // `resources/` holds the **framework's** resources -- templates that
        // produce Squared applications, and the kits, packages and assets
        // those applications link. It is owned by the `squared` repository and
        // assembled into place by tools/bootstrap.sh.
        //
        // `resources/generator/` holds resources for authoring the
        // **generator's own inputs**: template.kit produces a cartridge, not a
        // program, and makes sense to someone who has never heard of the
        // Squared framework. It stays with squared-pg because it describes
        // squared-pg's formats.
        //
        // The test that separates them: would this resource mean anything to
        // someone who does not use the framework? If yes, it is a generator
        // resource.
        //
        // Both are scanned, and a missing one is not an error -- a squared-pg
        // checkout without `squared/` assembled still authors cartridges, and
        // saying so with an empty index is better than refusing to start.
        // Three trees, and a missing one is never an error: a squared-pg
        // checkout with no `squared/` assembled still authors cartridges, and
        // an empty index says so more usefully than a refusal to start.
        //
        //   resources/squared/resources/
        //                         the framework, in a source checkout. The
        //                         `squared` repository, cloned under
        //                         resources/ by tools/bootstrap.sh -- it is
        //                         data the engine indexes, not code this
        //                         project compiles, so it belongs where
        //                         resources live rather than beside engine/.
        //                         The repeated `resources` is the clone's own
        //                         directory, not a mistake.
        //   resources/            the framework, in an installed environment,
        //                         where `sqpg initialize` has merged the two
        //                         trees under one root.
        //   resources/generator/  squared-pg's own: template.kit produces a
        //                         cartridge rather than a program, and means
        //                         something to someone who has never heard of
        //                         the Squared framework. That is the test
        //                         separating the two, and the reason this
        //                         tree stays with the tool.
        for (const char* kind : {"templates", "kits", "packages", "assets"}) {
            config.resource_roots.push_back(
                (root / "resources" / "squared" / "resources" / kind).string());
            config.resource_roots.push_back((root / "resources" / kind).string());
            config.resource_roots.push_back((root / "resources" / "generator" / kind).string());
        }
    }
    if (config.workflow_roots.empty()) {
        // §2.14.3 search order: repository, then user configuration. The
        // project-local and explicit tiers are the workflow's business, not
        // the host's, and are not scanned here.
        config.workflow_roots.push_back((root / "lua" / "workflows").string());
        if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg != nullptr) {
            config.workflow_roots.push_back(std::string{xdg} + "/squared-pg/workflows");
        } else if (const char* home = std::getenv("HOME"); home != nullptr) {
            config.workflow_roots.push_back(std::string{home} + "/.config/squared-pg/workflows");
        }
    }

    for (int i = 1; i < argc; ++i) {
        const std::string argument{argv[i]};
        if (argument == "--workflow" && i + 1 < argc) {
            config.workflow = argv[++i];
            continue;
        }
        if (argument.rfind("--workflow=", 0) == 0) {
            config.workflow = argument.substr(std::string{"--workflow="}.size());
            continue;
        }
        if (argument == "--fast") {
            config.fast_durability = true;
            continue;
        }
        config.forwarded.push_back(argument);
    }
    return config;
}

[[nodiscard]] std::filesystem::path find_workflow(const HostConfig& config, std::string& error) {
    for (const std::string& root : config.workflow_roots) {
        const std::filesystem::path directory = std::filesystem::path{root} / config.workflow;
        std::error_code ec;
        if (std::filesystem::exists(directory / "workflow.json", ec)) return directory;
    }
    error = "no workflow '" + config.workflow + "' on the workflow search path";
    return {};
}

/// Read one string field from a workflow declaration.
///
/// The host reads only the fields it needs to start the script -- `entry` and
/// `trust`. Everything else in the declaration is the workflow's own business
/// (§2.14.3), and a full JSON parse here would put the engine's yyjson on the
/// host's critical path for two string lookups.
[[nodiscard]] std::string workflow_field(const std::filesystem::path& directory,
                                         std::string_view field, std::string fallback) {
    std::ifstream in(directory / "workflow.json", std::ios::binary);
    if (!in) return fallback;
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    const std::string quoted = '"' + std::string{field} + '"';
    const std::size_t key = text.find(quoted);
    if (key == std::string::npos) return fallback;
    const std::size_t colon = text.find(':', key + quoted.size());
    if (colon == std::string::npos) return fallback;
    const std::size_t open = text.find('"', colon);
    if (open == std::string::npos) return fallback;
    const std::size_t close = text.find('"', open + 1);
    if (close == std::string::npos) return fallback;
    return text.substr(open + 1, close - open - 1);
}

int report_engine_failure(const pg::OperationResult& result) {
    for (const pg::EngineError& error : result.errors()) {
        std::fprintf(stderr, "sqpg: %s: %s\n", error.code.c_str(), error.message.c_str());
    }
    return 1;
}

}  // namespace

int main(int argc, char** argv) {
    const HostConfig config = parse_host_arguments(argc, argv);

    std::string       lookup_error;
    const std::filesystem::path workflow_dir = find_workflow(config, lookup_error);
    if (workflow_dir.empty()) {
        std::fprintf(stderr, "sqpg: %s\n", lookup_error.c_str());
        for (const std::string& root : config.workflow_roots) {
            std::fprintf(stderr, "  searched: %s\n", root.c_str());
        }
        return 2;
    }

    pg::EngineConfig engine_config;
    for (const std::string& root : config.resource_roots) engine_config.resource_roots.emplace_back(root);
    engine_config.fast_durability = config.fast_durability;

    std::unique_ptr<pg::Engine> engine = pg::Engine::create(std::move(engine_config));
    const pg::OperationResult started = engine->initialize();
    if (!started.succeeded()) return report_engine_failure(started);

    lua_State* state = luaL_newstate();
    if (state == nullptr) {
        std::fprintf(stderr, "sqpg: could not create a Lua state\n");
        (void)engine->shutdown();
        return 1;
    }
    luaL_openlibs(state);

    pg::lua::open_engine(state, *engine);

    pg::lua::HostInfo info;
    info.args          = config.forwarded;
    info.program       = argc > 0 ? argv[0] : "sqpg";
    info.workflow_id   = config.workflow;
    info.workflow_path = config.workflow_roots;
    std::error_code ec;
    info.cwd = std::filesystem::current_path(ec).string();
    info.executable = executable_path(argc > 0 ? argv[0] : nullptr).string();
    info.installation = installation_root(argc > 0 ? argv[0] : nullptr).string();
    pg::lua::open_host(state, info);

    // Let the workflow require its own modules, and the repository's shared
    // Lua library, without either knowing where it was installed.
    {
        const std::string package_path = (workflow_dir / "?.lua").string() + ";" +
                                         (workflow_dir / "?" / "init.lua").string() + ";" +
                                         (workflow_dir.parent_path().parent_path() / "?.lua").string() + ";" +
                                         (workflow_dir.parent_path().parent_path() / "?" / "init.lua").string();
        lua_getglobal(state, "package");
        lua_pushlstring(state, package_path.data(), package_path.size());
        lua_setfield(state, -2, "path");
        lua_pop(state, 1);
    }

    // §2.14.4: a workflow *requests* a trust tier and the host assigns one.
    //
    // Enforcing `sandboxed` means constructing a restricted _ENV -- no `io`,
    // no `os.execute`, no `loadfile`, no `package` -- and giving the workflow
    // a host-provided I/O channel in place of the standard library's. None of
    // that exists yet, so a workflow declaring the tier is refused rather than
    // run with the full standard library.
    //
    // A tier that is declared but silently unenforced is worse than no tier at
    // all: it invites exactly the assumption it fails to justify. Refusing
    // converts a false guarantee into an honest failure, and costs nothing to
    // remove once enforcement lands. Tracked as Q-26.
    const std::string trust = workflow_field(workflow_dir, "trust", "trusted");
    if (trust == "sandboxed") {
        std::fprintf(stderr,
                     "sqpg: workflow '%s' requests the 'sandboxed' trust tier, which this build\n"
                     "      does not enforce. Refusing to run it with full standard-library\n"
                     "      access, because that would grant more than it asked for.\n"
                     "\n"
                     "      To run it anyway, review it and change its trust tier to 'trusted'\n"
                     "      in %s\n",
                     config.workflow.c_str(), (workflow_dir / "workflow.json").string().c_str());
        lua_close(state);
        (void)engine->shutdown();
        return 3;
    }

    const std::filesystem::path entry = workflow_dir / workflow_field(workflow_dir, "entry", "init.lua");

    int status = 0;
    if (luaL_dofile(state, entry.string().c_str()) != LUA_OK) {
        // A Lua error is the workflow's failure, not the engine's. The engine
        // is still in a valid state and is shut down normally below.
        const char* message = lua_tostring(state, -1);
        std::fprintf(stderr, "sqpg: %s\n", message != nullptr ? message : "workflow failed");
        status = 1;
    } else if (lua_isinteger(state, -1)) {
        status = static_cast<int>(lua_tointeger(state, -1));
    } else if (lua_isboolean(state, -1)) {
        status = lua_toboolean(state, -1) != 0 ? 0 : 1;
    }

    lua_close(state);

    // §2.4.9: the host created the engine, so the host shuts it down, and the
    // process is expected to keep running afterwards.
    const pg::OperationResult stopped = engine->shutdown();
    if (!stopped.succeeded() && status == 0) status = report_engine_failure(stopped);

    return status;
}
