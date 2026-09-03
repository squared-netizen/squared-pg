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

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

namespace {

namespace pg = squared::pg;

constexpr const char* kDefaultWorkflow = "workflow.generate.default";

struct HostConfig {
    std::string              workflow{kDefaultWorkflow};
    std::vector<std::string> resource_roots;
    std::vector<std::string> workflow_roots;
    std::vector<std::string> forwarded;
    bool                     fast_durability{false};
};

/// Where the installation lives.
///
/// Derived from the executable's own location rather than the current working
/// directory: §2.7.2 forbids generation depending on the cwd, and a host that
/// only works when launched from the repository root would violate that in
/// spirit even though the engine itself is clean.
[[nodiscard]] std::filesystem::path installation_root(const char* argv0) {
    std::error_code ec;
    std::filesystem::path self = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (ec || self.empty()) {
        self = std::filesystem::absolute(argv0 != nullptr ? argv0 : "sqpg", ec);
    }
    // <root>/build/sqpg and <root>/bin/sqpg both resolve to <root>.
    std::filesystem::path directory = self.parent_path();
    if (directory.filename() == "build" || directory.filename() == "bin") {
        return directory.parent_path();
    }
    return directory;
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
        config.resource_roots.push_back((root / "resources" / "templates").string());
        config.resource_roots.push_back((root / "resources" / "kits").string());
        config.resource_roots.push_back((root / "resources" / "packages").string());
        config.resource_roots.push_back((root / "resources" / "assets").string());
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

/// Read the workflow's `entry` from its declaration. The host reads only the
/// fields it needs to start the script; the workflow's own requirements are
/// checked by the workflow (§2.14.3).
[[nodiscard]] std::string workflow_entry(const std::filesystem::path& directory) {
    std::ifstream in(directory / "workflow.json", std::ios::binary);
    if (!in) return "init.lua";
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const std::size_t key = text.find("\"entry\"");
    if (key == std::string::npos) return "init.lua";
    const std::size_t open = text.find('"', text.find(':', key));
    if (open == std::string::npos) return "init.lua";
    const std::size_t close = text.find('"', open + 1);
    if (close == std::string::npos) return "init.lua";
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

    const std::filesystem::path entry = workflow_dir / workflow_entry(workflow_dir);

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
