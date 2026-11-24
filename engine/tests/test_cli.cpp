// SPDX-License-Identifier: MIT
//
// The command surface: `app/`, `lua/` and the `engine/lua/` bindings.
//
// Driven as a subprocess rather than in-process. `main.cpp` owns the Lua state
// and the argument split, so linking it into a test would mean refactoring the
// host to be testable -- and the thing worth testing is precisely what a user
// gets when they type the command, including the parts of the host that only
// run under a real argv.
//
// sqpg keeps no environment of its own since 2026-11-23 (no ~/sqsysroot, no
// tiers): every workspace here is generated into a scratch folder by path.

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "check.hpp"

namespace fs = std::filesystem;
using namespace squared::pg;

namespace {

#ifndef SQUARED_PG_TEST_SOURCE_DIR
#error "SQUARED_PG_TEST_SOURCE_DIR must be defined"
#endif

struct Run {
    int         code{0};
    std::string output;   ///< stdout and stderr, merged

    [[nodiscard]] bool contains(std::string_view needle) const {
        return output.find(needle) != std::string::npos;
    }
};

/// A scratch folder, removed on destruction.
///
/// Under $TMPDIR, never /tmp: Termux has no /tmp, and this suite runs there.
class Scratch {
public:
    explicit Scratch(const char* name) {
        const char* base = std::getenv("TMPDIR");
        root_ = fs::path{base != nullptr ? base : "."} / (std::string{"sqpg-cli-"} + name);
        std::error_code ec;
        fs::remove_all(root_, ec);
        fs::create_directories(root_);
    }
    ~Scratch() {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }
    Scratch(const Scratch&) = delete;
    Scratch& operator=(const Scratch&) = delete;

    [[nodiscard]] const fs::path& path() const { return root_; }
    [[nodiscard]] fs::path child(const char* rel) const { return root_ / rel; }

private:
    fs::path root_;
};

/// Run sqpg from `where` (the scratch folder by default), capturing merged output.
Run sqpg(const Scratch& scratch, const std::string& args, const fs::path& where = {}) {
    const std::string binary = std::string{SQUARED_PG_TEST_SOURCE_DIR} + "/build/sqpg";
    const fs::path dir = where.empty() ? scratch.path() : where;
    const std::string command =
        "cd '" + dir.string() + "' && '" + binary + "' " + args + " </dev/null 2>&1";

    Run run;
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        run.code = -1;
        return run;
    }
    std::array<char, 4096> buffer{};
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        run.output += buffer.data();
    }
    const int status = pclose(pipe);
    run.code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return run;
}

fs::path seed_workspace(const Scratch& scratch, const char* name) {
    const fs::path target = scratch.child(name);
    const Run run = sqpg(scratch, std::string{"new "} + name
                                      + " -t template.terminal.cpp -k kit.terminal -o '" + target.string()
                                      + "'");
    CHECK(run.code == 0);
    return target;
}

/// The switch registry: a switch never consumes the argument after it.
void switches_do_not_swallow_arguments() {
    Scratch scratch("args");
    // Before the id and after it: the same answer, the id read as one.
    const Run before = sqpg(scratch, "show --json template.terminal.cpp");
    CHECK(before.code == 0);
    CHECK(before.contains("template.terminal.cpp"));
    const Run after = sqpg(scratch, "show template.terminal.cpp --json");
    CHECK(after.code == 0);
    CHECK(after.contains("template.terminal.cpp"));
}

/// inspect and verify went to square (2026-11-24): square info and square
/// verify read the workspace's SQ-INF; sqpg only makes workspaces.
void inspect_and_verify_are_squares() {
    Scratch scratch("gone");
    const fs::path made = seed_workspace(scratch, "g");
    CHECK(fs::is_regular_file(made / "SQ-INF" / "files.json"));
    CHECK(sqpg(scratch, "verify '" + made.string() + "'").code != 0);
    CHECK(sqpg(scratch, "inspect '" + made.string() + "'").code != 0);
    CHECK(!sqpg(scratch, "").contains("sqpg verify"));
}

/// The authoring loop: generate a kit workspace, install it, use it.
///
/// This is the first template whose deliverable is a cartridge rather than a
/// binary. It is also where two defects in template.kit were found,
/// both invisible until the template was actually used:
///
///   - a README under the payload tree landed as <workspace>/README.md and
///     collided with the project template's own;
///   - the make fragment was called kit_placeholder.mk and told the author to
///     rename it, so two kits authored from the template collided the moment
///     a project used both.
///
/// Neither would have been caught by inspecting the template. Both are
/// asserted below.
void kit_authoring_round_trip() {
    Scratch scratch("authoring");

    const std::string workspace = scratch.child("fmt").string();
    const Run made = sqpg(scratch, "new fmt -t template.kit -o '" + workspace + "'");
    CHECK(made.code == 0);

    const fs::path cart = fs::path{workspace} / "cartridge";
    CHECK(fs::is_regular_file(cart / "SQ-INF" / "manifest.json"));

    // The payload is namespaced after the kit, so two kits from this template
    // never contribute the same path.
    CHECK(fs::is_regular_file(cart / "tree" / "mk" / "kit_fmt.mk"));

    // And the cartridge's own README sits beside tree/, not inside it. A file
    // under tree/ is installed into every workspace that uses the kit.
    CHECK(fs::is_regular_file(cart / "README.md"));
    CHECK(!fs::exists(cart / "tree" / "README.md"));

    // Identity is composed textually rather than through a parameter.
    // `default_from` copies a value verbatim and cannot derive one, so a
    // parameter that is always "kit." plus another parameter is not a
    // parameter -- it is text.
    std::ifstream manifest(cart / "SQ-INF" / "manifest.json");
    const std::string text((std::istreambuf_iterator<char>(manifest)),
                           std::istreambuf_iterator<char>());
    CHECK(text.find("\"kit.fmt\"") != std::string::npos);
    CHECK(text.find("{{") == std::string::npos);   // no placeholder survived
}

/// A workspace inside a workspace is refused.
///
/// The way in is `sqpg new x` run from inside a workspace, where -o defaults
/// to ./x and nothing looked upward. The result sat inside the outer
/// workspace's provenance scope, and its cartridge.
void nested_workspaces_are_refused() {
    Scratch scratch("nested");
    const fs::path outer = seed_workspace(scratch, "outer");

    const Run nested = sqpg(scratch, "new inner -t template.terminal.cpp -k kit.terminal -o '"
                                         + (outer / "inner").string() + "'");
    CHECK(nested.code != 0);
    CHECK(nested.contains("inside an existing workspace"));
    CHECK(!fs::exists(outer / "inner"));

    // Deeper nesting is caught too: the search walks every ancestor, not just
    // the parent.
    const Run deep = sqpg(scratch, "new deep -t template.terminal.cpp -k kit.terminal -o '"
                                       + (outer / "sq_app" / "deep").string() + "'");
    CHECK(deep.code != 0);
    CHECK(deep.contains("inside an existing workspace"));

    // A sibling is fine. The check must refuse nesting, not refuse to
    // generate near an existing workspace.
    const std::string sibling = scratch.child("sibling").string();
    CHECK(sqpg(scratch, "new sibling -t template.terminal.cpp -k kit.terminal -o '"
                            + sibling + "'").code == 0);
}

/// The workspace marker is SQ-INF/manifest.json of kind workspace, and
/// nothing else: a cartridge of another kind is not one (D-055).
void workspace_detection_uses_the_manifest() {
    Scratch scratch("detect");

    // A cartridge of another kind above the output (a kit being authored)
    // is not a workspace: generating under it is allowed.
    const fs::path kitdir = scratch.child("notws");
    fs::create_directories(kitdir / "SQ-INF");
    std::ofstream(kitdir / "SQ-INF" / "manifest.json")
        << R"({"format": "squared-cartridge", "format_version": 3, "kind": "kit", "id": "kit.x", "version": "0.1.0"})";
    CHECK(sqpg(scratch, "new inkit -t template.terminal.cpp -k kit.terminal -o '"
                            + (kitdir / "inkit").string() + "'").code == 0);
}

void unknown_commands_and_help() {
    Scratch scratch("help");

    const Run bare = sqpg(scratch, "");
    CHECK(bare.contains("sqpg new"));
    // The environment and its commands are gone (2026-11-23).
    CHECK(!bare.contains("initialize"));
    CHECK(!bare.contains("selfdestruct"));
    CHECK(sqpg(scratch, "initialize").code != 0);

    // --version reports the version, not the usage text. It has to be
    // handled before the help branch, which absorbs anything with no command.
    const Run version = sqpg(scratch, "--version");
    CHECK(version.code == 0);
    CHECK(version.contains("sqpg 0."));
    CHECK(!version.contains("usage:"));

    const Run nonsense = sqpg(scratch, "frobnicate");
    CHECK(nonsense.code != 0);
    CHECK(nonsense.contains("unknown command"));
}

}  // namespace

int main() {
    switches_do_not_swallow_arguments();
    inspect_and_verify_are_squares();
    kit_authoring_round_trip();
    nested_workspaces_are_refused();
    workspace_detection_uses_the_manifest();
    unknown_commands_and_help();
    return test::report("test_cli");
}
