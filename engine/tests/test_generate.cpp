// SPDX-License-Identifier: MIT
// End to end: resolve, plan, generate, and the guarantees each step owes.

#include "squared/pg/engine.hpp"
#include "support/support.hpp"

#include "check.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

using namespace squared::pg;

namespace {

const std::string kSource{SQUARED_PG_TEST_SOURCE_DIR};

std::unique_ptr<Engine> ready_engine() {
    EngineConfig cfg;
    // The same three trees the host scans (app/src/main.cpp): the framework's
    // resources arrive from the `squared` repository as a sibling directory,
    // squared-pg's own live under resources/generator/, and `resources/` is
    // the un-split or installed shape. A missing root is not an error, so
    // listing all three costs nothing and keeps the tests honest about where
    // resources actually come from after the split.
    for (const char* kind : {"templates", "kits", "packages", "assets"}) {
        cfg.resource_roots.emplace_back(kSource + "/resources/" + kind);
        cfg.resource_roots.emplace_back(kSource + "/resources/generator/" + kind);
    }
    auto engine = Engine::create(std::move(cfg));
    (void)engine->initialize();
    return engine;
}

/// A scratch directory that cleans up after itself.
///
/// $TMPDIR, never a hardcoded /tmp: Termux has no /tmp, and a test that only
/// passes on a desktop is a test that will be discovered broken on a phone.
class Scratch {
public:
    explicit Scratch(const char* name) {
        const char* base = std::getenv("TMPDIR");
        root_ = std::filesystem::path{base != nullptr ? base : "."} / (std::string{"sqpg-"} + name);
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
        std::filesystem::create_directories(root_, ec);
    }
    ~Scratch() {
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }
    [[nodiscard]] std::filesystem::path child(const char* name) const { return root_ / name; }
    [[nodiscard]] const std::filesystem::path& root() const { return root_; }

private:
    std::filesystem::path root_;
};

Value request(const std::string& name, const std::string& output, bool with_lua) {
    Value config = Value::object();
    config.set("name", name);
    config.set("output", output);
    config.set("template", "template.terminal.cpp");

    Array kits;
    kits.emplace_back("kit.terminal");
    if (with_lua) kits.emplace_back("kit.lua");
    config.set("kits", Value{std::move(kits)});

    Array platforms;
    platforms.emplace_back("termux");
    config.set("platforms", Value{std::move(platforms)});
    config.set("framework", "1.0.0");
    return config;
}

std::string read(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

const Value* step_for(const Value& plan, const std::string& path) {
    for (const Value& step : *plan.find("steps")->as_array()) {
        if (step.string_or("path", {}) == path) return &step;
    }
    return nullptr;
}

void resolution() {
    auto engine = ready_engine();

    Value params = Value::object();
    params.set("id", "template.terminal.cpp");
    const OperationResult resolved = engine->execute("template.resolve", params);
    CHECK(resolved.succeeded());
    CHECK_EQ(std::string{resolved.data().string_or("working_directory", "")}, "sq_app");

    // §2.3.5: the engine never substitutes an alternative when resolution
    // fails. It reports, and the workflow decides.
    Value absent = Value::object();
    absent.set("id", "template.does.not_exist");
    const OperationResult missing = engine->execute("template.resolve", absent);
    CHECK(!missing.succeeded());
    CHECK_EQ(missing.errors().front().code, "template.not_found");
    CHECK(missing.errors().front().recoverable);
    // §2.15.3: the diagnostic must be actionable, so what was available is
    // part of the error rather than something the user has to go and find.
    CHECK(missing.errors().front().diagnostics.find("available") != nullptr);

    Value unsatisfiable = Value::object();
    unsatisfiable.set("id", "kit.terminal@^9.0.0");
    const OperationResult too_new = engine->execute("kit.resolve", unsatisfiable);
    CHECK(!too_new.succeeded());
    CHECK_EQ(too_new.errors().front().code, "kit.version.unsatisfiable");
    CHECK(too_new.errors().front().diagnostics.find("considered") != nullptr);

    (void)engine->shutdown();
}

void planning() {
    auto engine = ready_engine();
    const OperationResult planned =
        engine->execute("project.plan", request("hello", "/nonexistent/hello", true));
    CHECK(planned.succeeded());

    const Value& plan = planned.data();
    CHECK_EQ(std::string{plan.string_or("working_directory", "")}, "sq_app");
    CHECK(plan.int_or("step_count", 0) > 10);

    // The generated fragment is generator-owned; the Makefile that includes it
    // is the user's. That split is §2.7.10's answer to in-place merging.
    const Value* fragment = step_for(plan, "mk/squared_generated.mk");
    CHECK(fragment != nullptr);
    CHECK_EQ(std::string{fragment->string_or("ownership", "")}, "generated");

    const Value* makefile = step_for(plan, "Makefile");
    CHECK(makefile != nullptr);
    CHECK_EQ(std::string{makefile->string_or("ownership", "")}, "seeded");

    // §2.7.3: nothing generator-owned lands inside the working directory.
    for (const Value& step : *plan.find("steps")->as_array()) {
        const std::string path{step.string_or("path", "")};
        if (path.rfind("sq_app", 0) == 0) {
            CHECK(step.string_or("ownership", "") != "generated");
        }
    }

    // Kit contributions are attributed to the kit, not to the template.
    const Value* header = step_for(plan, "sq_kit/include/squared/kit/terminal.hpp");
    CHECK(header != nullptr);
    CHECK_EQ(std::string{header->string_or("origin", "")}, "kit.terminal");

    const Value* lua_workspace = step_for(plan, "sq_lua/main.lua");
    CHECK(lua_workspace != nullptr);
    CHECK_EQ(std::string{lua_workspace->string_or("ownership", "")}, "seeded");

    // §2.7.9: planning creates nothing, even when the output path is absurd.
    CHECK(!std::filesystem::exists("/nonexistent/hello"));

    (void)engine->shutdown();
}

void plan_matches_generation() {
    Scratch scratch("plan-match");
    auto    engine = ready_engine();
    const auto target = scratch.child("hello");

    const Value config = request("hello", target.string(), true);
    const OperationResult planned = engine->execute("project.plan", config);
    CHECK(planned.succeeded());

    const OperationResult generated = engine->execute("project.generate", config);
    CHECK(generated.succeeded());

    // §2.7.9: executing a plan must produce the workspace the plan described.
    // A dry run that can disagree with the real run is worse than none.
    for (const Value& step : *planned.data().find("steps")->as_array()) {
        const std::filesystem::path path = target / std::string{step.string_or("path", "")};
        CHECK(std::filesystem::exists(path));
    }

    (void)engine->shutdown();
}

void generation() {
    Scratch scratch("generate");
    auto    engine = ready_engine();
    const auto target = scratch.child("hello");

    const OperationResult result =
        engine->execute("project.generate", request("hello", target.string(), false));
    CHECK(result.succeeded());
    CHECK(result.data().int_or("files_written", 0) > 5);

    CHECK(std::filesystem::exists(target / "Makefile"));
    CHECK(std::filesystem::exists(target / "sq_app" / "src" / "main.cpp"));
    CHECK(std::filesystem::exists(target / "sq_kit" / "include" / "squared" / "kit" / "terminal.hpp"));
    CHECK(std::filesystem::exists(target / ".squared" / "metadata.json"));

    // kit.lua was not selected, so nothing it contributes may appear.
    CHECK(!std::filesystem::exists(target / "sq_lua"));
    CHECK(!std::filesystem::exists(target / "mk" / "kit_lua.mk"));

    // Substitution actually happened, and the placeholder is gone.
    const std::string main_cpp = read(target / "sq_app" / "src" / "main.cpp");
    CHECK(main_cpp.find("hello::App") != std::string::npos);
    CHECK(main_cpp.find("{{") == std::string::npos);

    // The executable bit came from the manifest, not from a filename guess.
    const auto permissions = std::filesystem::status(target / "run.sh").permissions();
    CHECK((permissions & std::filesystem::perms::owner_exec) != std::filesystem::perms::none);

    // Staging left nothing behind.
    CHECK(!std::filesystem::exists(scratch.root() / ".hello.squared-tmp"));

    (void)engine->shutdown();
}

void metadata_and_provenance() {
    Scratch scratch("metadata");
    auto    engine = ready_engine();
    const auto target = scratch.child("hello");

    CHECK(engine->execute("project.generate", request("hello", target.string(), true)).succeeded());

    Value inspect = Value::object();
    inspect.set("workspace", target.string());
    const OperationResult read_back = engine->execute("workspace.inspect", inspect);
    CHECK(read_back.succeeded());

    const Value& record = read_back.data();
    CHECK(record.int_or("record_version", 0) == 1);
    CHECK_EQ(std::string{record.find("project")->string_or("name", "")}, "hello");
    CHECK_EQ(std::string{record.find("resources")->find("template")->string_or("id", "")},
             "template.terminal.cpp");

    // §2.7.3 forbids absolute host paths in the record, so a workspace stays
    // relocatable and its identity does not depend on where it sits.
    const std::string raw = read(target / ".squared" / "metadata.json");
    CHECK(raw.find(scratch.root().string()) == std::string::npos);

    // §2.7.10: one provenance entry per generated path, carrying the hash the
    // file had when it was written.
    const Array* provenance = record.find("provenance")->as_array();
    CHECK(provenance != nullptr && !provenance->empty());
    for (const Value& entry : *provenance) {
        const std::string path{entry.string_or("path", "")};
        const std::string hash{entry.string_or("sha256", "")};
        CHECK(hash.size() == 64);
        CHECK(!entry.string_or("origin", "").empty());
        // The recorded hash must match the file actually on disk, or conflict
        // detection would report edits nobody made.
        CHECK_EQ(support::to_hex(support::sha256(read(target / path))), hash);
    }

    (void)engine->shutdown();
}

/// workspace.verify: the first reader of the provenance table.
///
/// The table has been written on every generation since provenance existed
/// and read by nothing, which meant it was correct only by coincidence --
/// unread data drifts without failing. These assertions are what make it
/// correct on purpose.
void verify_detects_drift() {
    Scratch scratch("verify");
    auto    engine = ready_engine();
    const auto target = scratch.child("hello");

    CHECK(engine->execute("project.generate", request("hello", target.string(), true)).succeeded());

    Value params = Value::object();
    params.set("workspace", target.string());

    const auto verify = [&]() { return engine->execute("workspace.verify", params); };

    // A freshly generated workspace verifies clean. If this ever fails, the
    // hashes being written do not describe the bytes being written, and every
    // other assertion here is meaningless.
    {
        const OperationResult clean = verify();
        CHECK(clean.succeeded());
        CHECK(clean.data().int_or("missing", -1) == 0);
        CHECK(clean.data().int_or("modified", -1) == 0);
        CHECK(clean.data().int_or("conflicts", -1) == 0);
        CHECK(clean.data().int_or("recorded", 0) > 0);
    }

    // Editing a `seeded` file is the system working, not a conflict. It was
    // handed over at generation and has been written in since, which is what
    // seeded means -- counted as `edited`, reported as nothing.
    {
        std::ofstream(target / "sq_app" / "src" / "main.cpp", std::ios::app) << "\n// mine\n";
        const OperationResult edited = verify();
        CHECK(edited.succeeded());
        CHECK(edited.data().int_or("edited", -1) == 1);
        CHECK(edited.data().int_or("modified", -1) == 0);
        CHECK(edited.data().int_or("conflicts", -1) == 0);
    }

    // Editing a `generated` file is a conflict: a future generation would
    // overwrite it, and the point of verify is to say so before that happens.
    {
        std::ofstream(target / "mk" / "squared_generated.mk", std::ios::app) << "\n# hand edit\n";
        const OperationResult conflict = verify();
        CHECK(conflict.succeeded());   // a finding is a report, not a failure
        CHECK(conflict.data().int_or("modified", -1) == 1);
        CHECK(conflict.data().int_or("conflicts", -1) == 1);

        const Array* findings = conflict.data().find("findings")->as_array();
        CHECK(findings != nullptr && findings->size() == 1);
        if (findings != nullptr && findings->size() == 1) {
            CHECK_EQ(std::string{(*findings)[0].string_or("finding", "")}, "modified");
            CHECK_EQ(std::string{(*findings)[0].string_or("path", "")},
                     "mk/squared_generated.mk");
        }
    }

    // A deleted recorded file is found, and distinguished from a modified one
    // -- the remedy differs, so the report must too.
    {
        std::filesystem::remove(target / "mk" / "squared_generated.mk");
        const OperationResult gone = verify();
        CHECK(gone.succeeded());
        CHECK(gone.data().int_or("missing", -1) == 1);

        const Array* findings = gone.data().find("findings")->as_array();
        CHECK(findings != nullptr && findings->size() == 1);
        if (findings != nullptr && findings->size() == 1) {
            CHECK_EQ(std::string{(*findings)[0].string_or("finding", "")}, "missing");
        }
    }

    (void)engine->shutdown();
}

void refuses_existing_workspace() {
    Scratch scratch("existing");
    auto    engine = ready_engine();
    const auto target = scratch.child("hello");
    std::filesystem::create_directories(target);

    const OperationResult result =
        engine->execute("project.generate", request("hello", target.string(), false));
    CHECK(!result.succeeded());
    CHECK_EQ(result.errors().front().code, "filesystem.workspace.exists");

    (void)engine->shutdown();
}

void determinism() {
    Scratch scratch("determinism");
    auto    engine = ready_engine();

    // §2.7.13: equivalent inputs produce equivalent output, byte-identical
    // where nothing is timestamp-sensitive. Metadata is generated output, so
    // it is included in the comparison rather than excused from it.
    const auto first  = scratch.child("one");
    const auto second = scratch.child("two");
    CHECK(engine->execute("project.generate", request("hello", first.string(), true)).succeeded());
    CHECK(engine->execute("project.generate", request("hello", second.string(), true)).succeeded());

    CHECK_EQ(read(first / ".squared" / "metadata.json"), read(second / ".squared" / "metadata.json"));
    CHECK_EQ(read(first / "sq_app" / "src" / "app.cpp"), read(second / "sq_app" / "src" / "app.cpp"));
    CHECK_EQ(read(first / "mk" / "squared_generated.mk"), read(second / "mk" / "squared_generated.mk"));

    (void)engine->shutdown();
}

void required_kit_is_enforced() {
    auto engine = ready_engine();

    Value config = Value::object();
    config.set("name", "hello");
    config.set("output", "/nonexistent/hello");
    config.set("template", "template.terminal.cpp");
    config.set("kits", Value::array());  // template requires kit.terminal

    const OperationResult result = engine->execute("project.plan", config);
    CHECK(!result.succeeded());
    CHECK_EQ(result.errors().front().code, "template.kit.required");

    (void)engine->shutdown();
}

void configuration_is_validated() {
    auto engine = ready_engine();

    Value bad_name = request("9lives", "/nonexistent/x", false);
    const OperationResult named = engine->execute("config.validate", bad_name);
    CHECK(!named.succeeded());
    CHECK_EQ(named.errors().front().code, "configuration.name.invalid");

    Value bad_id = request("hello", "/nonexistent/x", false);
    bad_id.set("template", "not-an-identity");
    const OperationResult identity = engine->execute("config.validate", bad_id);
    CHECK(!identity.succeeded());
    CHECK_EQ(identity.errors().front().code, "configuration.identity.invalid");

    CHECK(engine->execute("config.validate", request("hello", "/nonexistent/x", false)).succeeded());

    (void)engine->shutdown();
}

}  // namespace

int main() {
    resolution();
    planning();
    plan_matches_generation();
    generation();
    metadata_and_provenance();
    verify_detects_drift();
    refuses_existing_workspace();
    determinism();
    required_kit_is_enforced();
    configuration_is_validated();
    return squared::pg::test::report("test_generate");
}
