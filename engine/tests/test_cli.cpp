// SPDX-License-Identifier: MIT
//
// The command surface: `app/`, `lua/` and the `engine/lua/` bindings.
//
// These three had no tests at all. That was tolerable while the CLI only
// generated workspaces -- a mistake showed up as a workspace that did not
// build. It stopped being tolerable when `lifecycle.lua` gained three commands
// that rename trees, copy trees and write files into them. `demote` moves a
// directory the author cares about; `quarantine` runs `cp -a`. A defect in
// either costs real work, and nothing was watching.
//
// Driven as a subprocess rather than in-process. `main.cpp` owns the Lua state
// and the argument split, so linking it into a test would mean refactoring the
// host to be testable -- and the thing worth testing is precisely what a user
// gets when they type the command, including the parts of the host that only
// run under a real argv.
//
// SQSYSROOT is set on every invocation without exception. A test that fell
// back to $HOME would operate on the author's own environment, and `demote`
// would move their work.

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

/// A scratch environment root, removed on destruction.
///
/// Under $TMPDIR, never /tmp: Termux has no /tmp, and this suite runs there.
class Sysroot {
public:
    explicit Sysroot(const char* name) {
        const char* base = std::getenv("TMPDIR");
        root_ = fs::path{base != nullptr ? base : "."} / (std::string{"sqpg-cli-"} + name);
        std::error_code ec;
        fs::remove_all(root_, ec);
    }
    ~Sysroot() {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }
    Sysroot(const Sysroot&) = delete;
    Sysroot& operator=(const Sysroot&) = delete;

    [[nodiscard]] const fs::path& path() const { return root_; }
    [[nodiscard]] fs::path child(const char* rel) const { return root_ / rel; }

private:
    fs::path root_;
};

/// Run sqpg with SQSYSROOT pointed at `sysroot`, capturing merged output.
Run sqpg(const Sysroot& sysroot, const std::string& args, const std::string& input = {}) {
    const std::string binary = std::string{SQUARED_PG_TEST_SOURCE_DIR} + "/build/sqpg";
    // `printf | cmd` rather than a here-string: popen runs /bin/sh, which has
    // no <<<. Piping is also the form the command documents, so the tests
    // exercise the published invocation.
    const std::string prefix =
        input.empty() ? std::string{"</dev/null "}
                      : "printf '%s\\n' '" + input + "' | ";
    const std::string command = prefix + "SQSYSROOT='" + sysroot.path().string() + "' '" + binary
                              + "' " + args + " 2>&1";

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

// ---------------------------------------------------------------------------

void initialize_creates_and_installs() {
    Sysroot sysroot("init");

    const Run first = sqpg(sysroot, "initialize");
    CHECK(first.code == 0);

    // The four directories, and .squared last -- it is the marker an
    // initialised environment is recognised by, so a failure part-way through
    // must not leave something that looks complete.
    CHECK(fs::is_directory(sysroot.child("project")));
    CHECK(fs::is_directory(sysroot.child("sandbox")));
    CHECK(fs::is_directory(sysroot.child(".sqpg")));

    // It installs. A `.squared` with nothing in it was the whole defect the
    // install change existed to fix.
    CHECK(fs::is_regular_file(sysroot.child(".sqpg/bin/sqpg")));
    CHECK(fs::is_directory(sysroot.child(".sqpg/lua/workflows")));
    CHECK(fs::is_directory(sysroot.child(".sqpg/resources")));

    // The layout is the mechanism: a binary at .sqpg/bin/sqpg walks up one
    // level and finds both markers. If this pair ever stops holding, the
    // installed copy silently falls back to some other root.
    CHECK(fs::is_directory(sysroot.child(".sqpg/resources/templates")));
    CHECK(fs::is_directory(sysroot.child(".sqpg/lua/squaredpg")));

    CHECK(first.contains("export PATH="));

    // Idempotent. The natural response to "did that work?" is to run it again.
    const Run second = sqpg(sysroot, "initialize");
    CHECK(second.code == 0);
    CHECK(second.contains("already present"));
    CHECK(fs::is_regular_file(sysroot.child(".sqpg/bin/sqpg")));
}

/// Generate a workspace into the sandbox. Returns its path.
fs::path seed_workspace(const Sysroot& sysroot, const char* name) {
    const std::string target = sysroot.child("sandbox").string() + "/" + name;
    const Run run = sqpg(sysroot, std::string{"new "} + name
                                      + " -t template.terminal.cpp -k kit.terminal -o '" + target
                                      + "'");
    CHECK(run.code == 0);
    return fs::path{target};
}

void promote_moves_and_is_reversible() {
    Sysroot sysroot("promote");
    CHECK(sqpg(sysroot, "initialize").code == 0);
    const fs::path made = seed_workspace(sysroot, "demo");
    CHECK(fs::is_directory(made));

    // --explain performs nothing. A dry run that moved something would be
    // worse than no dry run, because the user would have trusted it.
    const Run explained = sqpg(sysroot, "promote demo --explain");
    CHECK(explained.code == 0);
    CHECK(explained.contains("mv "));
    CHECK(fs::is_directory(made));                             // still in sandbox
    CHECK(!fs::exists(sysroot.child("project/demo")));

    const Run promoted = sqpg(sysroot, "promote demo");
    CHECK(promoted.code == 0);
    CHECK(!fs::exists(made));                                  // gone from sandbox
    CHECK(fs::is_directory(sysroot.child("project/demo")));
    // The tier is the location (D-046); the move is the whole operation.
    CHECK(fs::is_regular_file(sysroot.child("project/demo/Makefile")));

    // The sandbox boundary described a tier this tree has left, so it would
    // now be a lie.
    CHECK(!fs::exists(sysroot.child("project/demo/AGENTS.md")));

    // A second promote finds nothing, and says which tier it looked in.
    const Run again = sqpg(sysroot, "promote demo");
    CHECK(again.code != 0);
    CHECK(again.contains("sandbox"));

    // Reversible, and demotion preserves what it did not write (D-050).
    const Run demoted = sqpg(sysroot, "demote demo");
    CHECK(demoted.code == 0);
    CHECK(fs::is_directory(sysroot.child("sandbox/demo")));
    CHECK(!fs::exists(sysroot.child("project/demo")));
    CHECK(fs::is_regular_file(sysroot.child("sandbox/demo/AGENTS.md")));
    CHECK(fs::is_regular_file(sysroot.child("sandbox/demo/sq_app/src/main.cpp")));
}

void demote_never_deletes_history() {
    Sysroot sysroot("demote");
    CHECK(sqpg(sysroot, "initialize").code == 0);
    seed_workspace(sysroot, "keep");
    CHECK(sqpg(sysroot, "promote keep").code == 0);

    const fs::path promoted = sysroot.child("project/keep");
    // Stand in for a repository without requiring git to be installed: the
    // rule under test is that demotion does not delete this directory, and
    // that rule does not care what created it.
    fs::create_directories(promoted / ".git");
    std::ofstream(promoted / ".git" / "HEAD") << "ref: refs/heads/main\n";

    // A dirty tree is moved, not refused (D-054). The design proposal said
    // refuse; the implementation is normative, and this is the assertion that
    // makes the divergence deliberate rather than drift.
    std::ofstream(promoted / "sq_app" / "src" / "main.cpp", std::ios::app) << "\n// uncommitted\n";

    const Run demoted = sqpg(sysroot, "demote keep");
    CHECK(demoted.code == 0);
    CHECK(fs::is_regular_file(sysroot.child("sandbox/keep/.git/HEAD")));
    // And it says so, rather than moving a tree into a disposable tier in
    // silence.
    CHECK(demoted.contains("git"));
}

void quarantine_copies_and_refuses_insiders() {
    Sysroot sysroot("quarantine");
    CHECK(sqpg(sysroot, "initialize").code == 0);

    const fs::path foreign = sysroot.child("outside");
    fs::create_directories(foreign / "src");
    std::ofstream(foreign / "src" / "a.c") << "int main(void){return 0;}\n";

    const Run adopted = sqpg(sysroot, "quarantine '" + foreign.string() + "'");
    CHECK(adopted.code == 0);
    CHECK(fs::is_regular_file(sysroot.child("sandbox/outside/src/a.c")));

    // Copies, never moves (D-048): the source belongs to someone else.
    CHECK(fs::is_regular_file(foreign / "src" / "a.c"));

    // Marked unreviewed. A tree with no provenance record cannot be verified,
    // and the boundary file is the only thing that says so.
    CHECK(fs::is_regular_file(sysroot.child("sandbox/outside/AGENTS.md")));

    // A tree already inside the environment is the wrong tool, and the error
    // must say which tool is right. Diagnosing the name clash first would be
    // true and useless.
    const Run inside = sqpg(sysroot, "quarantine '"
                                         + sysroot.child("sandbox/outside").string() + "'");
    CHECK(inside.code != 0);
    CHECK(inside.contains("already inside"));
    CHECK(inside.contains("demote"));
}

void workflow_verbs_drive_make() {
    Sysroot sysroot("verbs");
    CHECK(sqpg(sysroot, "initialize").code == 0);
    const fs::path made = seed_workspace(sysroot, "app");

    for (const char* verb : {"format", "lint", "check", "test", "docs", "dist"}) {
        const Run explained =
            sqpg(sysroot, std::string{verb} + " --workspace '" + made.string() + "' --explain");
        CHECK(explained.code == 0);
        // The driver's whole contract: find the workspace, run `make <verb>`.
        CHECK(explained.contains("make -C"));
        CHECK(explained.contains(verb));
    }

    // A verb no kit provides fails with an explanation, not a make error.
    const Run missing = sqpg(sysroot, "docs --workspace '" + made.string() + "'");
    CHECK(missing.code != 0);
    CHECK(missing.contains("contributed by a kit"));

    // Outside a workspace, the search fails cleanly rather than running make
    // somewhere unexpected.
    const Run nowhere = sqpg(sysroot, "lint --workspace '" + sysroot.path().string() + "'");
    CHECK(nowhere.code != 0);
}

void switches_do_not_swallow_arguments() {
    Sysroot sysroot("args");
    CHECK(sqpg(sysroot, "initialize").code == 0);
    seed_workspace(sysroot, "sw");

    // The bug this guards: the parser had no switch registry, so any long
    // option not in a hardcoded list consumed the next argument. `promote
    // --explain sw` ate the workspace name and then reported that promote
    // needed one.
    const Run before = sqpg(sysroot, "promote --explain sw");
    CHECK(before.code == 0);
    CHECK(before.contains("mv "));
    CHECK(!before.contains("needs a workspace name"));
    CHECK(fs::is_directory(sysroot.child("sandbox/sw")));   // explain performed nothing

    // And the same switch after the positional, which is the ordering that
    // always worked and must keep working.
    const Run after = sqpg(sysroot, "promote sw --explain");
    CHECK(after.code == 0);
    CHECK(after.contains("mv "));
}

void verify_reads_the_provenance_table() {
    Sysroot sysroot("verify");
    CHECK(sqpg(sysroot, "initialize").code == 0);
    const fs::path made = seed_workspace(sysroot, "v");

    const Run clean = sqpg(sysroot, "verify '" + made.string() + "'");
    CHECK(clean.code == 0);
    CHECK(clean.contains("no conflicts"));

    std::ofstream(made / "mk" / "squared_generated.mk", std::ios::app) << "\n# edit\n";
    const Run dirty = sqpg(sysroot, "verify '" + made.string() + "'");
    // Non-zero so the command composes into a check target or a CI step.
    CHECK(dirty.code != 0);
    CHECK(dirty.contains("squared_generated.mk"));
}

/// The authoring loop: generate a kit workspace, install it, use it.
///
/// This is the first template whose deliverable is a cartridge rather than a
/// binary, and the first use of the workflow verbs' `dist` and `check` for
/// something real. It is also where two defects in template.kit were found,
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
    Sysroot sysroot("authoring");
    CHECK(sqpg(sysroot, "initialize").code == 0);

    const std::string workspace = sysroot.child("sandbox").string() + "/fmt";
    const Run made = sqpg(sysroot, "new fmt -t template.kit -o '" + workspace + "'");
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
/// to ./x and nothing looked upward. The result was a tree the tier commands
/// could not reach -- `promote` takes a name in sandbox/, not a path -- and
/// which sat inside the outer workspace's provenance scope.
void nested_workspaces_are_refused() {
    Sysroot sysroot("nested");
    CHECK(sqpg(sysroot, "initialize").code == 0);
    const fs::path outer = seed_workspace(sysroot, "outer");

    const Run nested = sqpg(sysroot, "new inner -t template.terminal.cpp -k kit.terminal -o '"
                                         + (outer / "inner").string() + "'");
    CHECK(nested.code != 0);
    CHECK(nested.contains("inside an existing workspace"));
    CHECK(!fs::exists(outer / "inner"));

    // Deeper nesting is caught too: the search walks every ancestor, not just
    // the parent.
    const Run deep = sqpg(sysroot, "new deep -t template.terminal.cpp -k kit.terminal -o '"
                                       + (outer / "sq_app" / "deep").string() + "'");
    CHECK(deep.code != 0);
    CHECK(deep.contains("inside an existing workspace"));

    // A sibling is fine. The check must refuse nesting, not refuse to
    // generate near an existing workspace.
    const std::string sibling = sysroot.child("sandbox").string() + "/sibling";
    CHECK(sqpg(sysroot, "new sibling -t template.terminal.cpp -k kit.terminal -o '"
                            + sibling + "'").code == 0);
}

/// The workspace marker is the metadata record.
///
/// find_workspace tested for a `.squared-pg` directory that has never
/// existed; the engine writes `.squared/metadata.json`. Every workspace found
/// so far came through a fallback matching a Makefile beside an mk/ directory
/// -- which matches any C project laid out that way, and misses a real
/// workspace without one.
void workspace_detection_uses_the_record() {
    Sysroot sysroot("detect");
    CHECK(sqpg(sysroot, "initialize").code == 0);
    const fs::path made = seed_workspace(sysroot, "det");
    CHECK(fs::is_regular_file(made / ".squared" / "metadata.json"));

    // Found from a subdirectory, which is the whole point of searching up.
    const Run deep = sqpg(sysroot, "lint --explain --workspace '"
                                       + (made / "sq_app" / "src").string() + "'");
    (void)deep;   // --workspace bypasses the search; the next case exercises it

    // A directory with a Makefile and mk/ but no record is NOT a workspace.
    // Under the old fallback it would have been, and `sqpg lint` there would
    // have run make in a tree the generator never made.
    const fs::path impostor = sysroot.child("outside-project");
    fs::create_directories(impostor / "mk");
    std::ofstream(impostor / "Makefile") << "all:\n\t@true\n";
    const Run fooled = sqpg(sysroot, "lint --workspace '" + impostor.string() + "'");
    CHECK(fooled.code != 0);
}

/// selfdestruct: nothing is removed without both factors.
///
/// The command exists because `rm -rf ~/sqsysroot` is one tab-completion away
/// and has already cost a project. It only helps if it is worth reaching for,
/// so the assertions below are mostly about what it *refuses* -- a
/// confirmation flow that can be satisfied by accident is decoration.
void selfdestruct_needs_both_factors() {
    Sysroot sysroot("destruct");
    CHECK(sqpg(sysroot, "initialize").code == 0);
    const fs::path made = seed_workspace(sysroot, "doomed");

    // First invocation reports and stops. Non-zero, so a `&&` chain does not
    // continue into something that assumes the deletion happened.
    const Run survey = sqpg(sysroot, "selfdestruct doomed");
    CHECK(survey.code != 0);
    CHECK(survey.contains("nothing has been deleted"));
    CHECK(survey.contains("--confirm"));
    CHECK(fs::is_directory(made));

    // Extract the token it printed.
    const std::size_t at = survey.output.find("--confirm ");
    CHECK(at != std::string::npos);
    std::string token;
    if (at != std::string::npos) {
        token = survey.output.substr(at + 10, 8);
    }

    // Token alone is not enough: the phrase must be typed.
    const Run no_phrase = sqpg(sysroot, "selfdestruct doomed --confirm " + token);
    CHECK(no_phrase.code != 0);
    CHECK(fs::is_directory(made));

    // Wrong phrase, right token.
    const Run wrong = sqpg(sysroot, "selfdestruct doomed --confirm " + token, "yes");
    CHECK(wrong.code != 0);
    CHECK(fs::is_directory(made));

    // Right phrase, wrong token. This is the case that matters most: a
    // command line copied from an earlier session, after the tree changed.
    const Run stale = sqpg(sysroot, "selfdestruct doomed --confirm deadbeef", "destroy doomed");
    CHECK(stale.code != 0);
    CHECK(stale.contains("does not match"));
    CHECK(fs::is_directory(made));

    // Both factors: it goes.
    const Run done = sqpg(sysroot, "selfdestruct doomed --confirm " + token, "destroy doomed");
    CHECK(done.code == 0);
    CHECK(!fs::exists(made));

    // No target is a usage error, never a guess. A destructive command that
    // infers what you meant is the whole problem restated.
    const Run bare = sqpg(sysroot, "selfdestruct");
    CHECK(bare.code != 0);
    CHECK(bare.contains("needs a target"));
}

/// The token tracks the inventory, and the inventory is what was shown.
void selfdestruct_token_tracks_the_inventory() {
    Sysroot sysroot("destruct2");
    CHECK(sqpg(sysroot, "initialize").code == 0);
    seed_workspace(sysroot, "one");

    const Run first = sqpg(sysroot, "selfdestruct --sandbox");
    const std::size_t at = first.output.find("--confirm ");
    CHECK(at != std::string::npos);
    const std::string token = at == std::string::npos ? "" : first.output.substr(at + 10, 8);

    // A second workspace changes what the inventory reports, so the token
    // must stop matching -- otherwise a stale command line would delete a
    // tree the operator never saw listed.
    seed_workspace(sysroot, "two");
    const Run stale = sqpg(sysroot, "selfdestruct --sandbox --confirm " + token,
                           "destroy the sandbox");
    CHECK(stale.code != 0);
    CHECK(fs::is_directory(sysroot.child("sandbox/one")));
    CHECK(fs::is_directory(sysroot.child("sandbox/two")));

    // The tier survives its own emptying: an environment without its tiers
    // is broken, and the next `sqpg new` would fail on a missing directory.
    const Run now = sqpg(sysroot, "selfdestruct --sandbox");
    const std::size_t at2 = now.output.find("--confirm ");
    const std::string token2 = at2 == std::string::npos ? "" : now.output.substr(at2 + 10, 8);
    const Run cleared = sqpg(sysroot, "selfdestruct --sandbox --confirm " + token2,
                             "destroy the sandbox");
    CHECK(cleared.code == 0);
    CHECK(fs::is_directory(sysroot.child("sandbox")));
    CHECK(!fs::exists(sysroot.child("sandbox/one")));
}

void unknown_commands_and_help() {
    Sysroot sysroot("help");

    const Run bare = sqpg(sysroot, "");
    CHECK(bare.contains("sqpg new"));
    CHECK(bare.contains("initialize"));
    CHECK(bare.contains("--explain"));

    const Run nonsense = sqpg(sysroot, "frobnicate");
    CHECK(nonsense.code != 0);
    CHECK(nonsense.contains("unknown command"));
}

}  // namespace

int main() {
    initialize_creates_and_installs();
    promote_moves_and_is_reversible();
    demote_never_deletes_history();
    quarantine_copies_and_refuses_insiders();
    workflow_verbs_drive_make();
    switches_do_not_swallow_arguments();
    verify_reads_the_provenance_table();
    kit_authoring_round_trip();
    nested_workspaces_are_refused();
    workspace_detection_uses_the_record();
    selfdestruct_needs_both_factors();
    selfdestruct_token_tracks_the_inventory();
    unknown_commands_and_help();
    return test::report("test_cli");
}
