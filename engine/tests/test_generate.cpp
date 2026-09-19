// SPDX-License-Identifier: MIT
// End to end: resolve, plan, generate, and the guarantees each step owes.

#include "squared/pg/engine.hpp"
#include "support/support.hpp"

#include "check.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

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

/// A request against the Android template with a rendering kit and an
/// optional package list — the set package.squared-core was written for.
Value request_android(const std::string& name, const std::string& output,
                      const std::vector<std::string>& packages) {
    Value config = Value::object();
    config.set("name", name);
    config.set("output", output);
    config.set("template", "template.android.cpp");
    Array kits;
    kits.emplace_back("kit.opengl");
    config.set("kits", Value{std::move(kits)});
    if (!packages.empty()) config.set("packages", Value::strings(packages));
    config.set("framework", "1.0.0");

    Value parameters = Value::object();
    parameters.set("package_name", "com.example." + name);
    config.set("parameters", parameters);
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

std::vector<Value> steps_named(const Value& plan, const std::string& path) {
    std::vector<Value> matches;
    for (const Value& step : *plan.find("steps")->as_array()) {
        if (step.string_or("path", {}) == path) matches.push_back(step);
    }
    return matches;
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
    const Value* header = step_for(plan, "sq_kit/include/terminal/terminal.hpp");
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
    CHECK(std::filesystem::exists(target / "sq_kit" / "include" / "terminal" / "terminal.hpp"));
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

// ---------------------------------------------------------------------------
// Package materialization (D-076 .. D-080)
// ---------------------------------------------------------------------------

/// An engine whose index also scans a scratch packages root, so the tests can
/// exercise synthetic packages a shipped manifest cannot describe.
std::unique_ptr<Engine> engine_with_packages_root(const std::filesystem::path& packages_root) {
    EngineConfig cfg;
    for (const char* kind : {"templates", "kits", "packages", "assets"}) {
        cfg.resource_roots.emplace_back(kSource + "/resources/" + kind);
        cfg.resource_roots.emplace_back(kSource + "/resources/generator/" + kind);
    }
    cfg.resource_roots.emplace_back(packages_root);
    auto engine = Engine::create(std::move(cfg));
    (void)engine->initialize();
    return engine;
}

void write_synthetic_package(const std::filesystem::path& packages_root, const std::string& id,
                             const std::vector<std::string>& overrides,
                             const std::map<std::string, std::string>& payload) {
    const std::filesystem::path location = packages_root / id;
    std::filesystem::create_directories(location / "SQ-INF");
    for (const auto& [path, content] : payload) {
        const std::filesystem::path full = location / path;
        std::filesystem::create_directories(full.parent_path());
        std::ofstream(full) << content;
    }

    std::string override_json;
    for (const std::string& override_path : overrides) {
        if (!override_json.empty()) override_json += ", ";
        override_json += "\"" + override_path + "\"";
    }
    const std::string manifest =
        "{\n"
        "  \"format\": \"squared-cartridge\",\n"
        "  \"format_version\": 2,\n"
        "  \"kind\": \"package\",\n"
        "  \"id\": \"" + id + "\",\n"
        "  \"version\": \"1.0.0\",\n"
        "  \"tree\": \"tree/\",\n"
        "  \"consumers\": {\n"
        "    \"squared_pg\": {\n"
        "      \"requires\": { \"framework\": \">=1.0.0 <2.0.0\" },\n"
        "      \"ownership\": { \"default\": \"shared\" },\n"
        "      \"overrides\": [ " + override_json + " ]\n"
        "    }\n"
        "  }\n"
        "}\n";
    std::ofstream(location / "SQ-INF" / "manifest.json") << manifest;
}

void package_materializes() {
    Scratch scratch("package");
    auto    engine = ready_engine();
    const auto target = scratch.child("demo");

    const Value config = request_android("demo", target.string(), {"package.squared-core"});
    const OperationResult planned = engine->execute("project.plan", config);
    CHECK(planned.succeeded());

    // D-079: the plan names packages exactly like kits -- the same shape,
    // with no package-shaped variant.
    const Value& plan = planned.data();
    const Array* pkgs = plan.find("packages")->as_array();
    CHECK(pkgs != nullptr && pkgs->size() == 1);
    if (pkgs != nullptr && pkgs->size() == 1) {
        CHECK_EQ(std::string{(*pkgs)[0].string_or("id", "")}, "package.squared-core");
        CHECK_EQ(std::string{(*pkgs)[0].string_or("version", "")}, "1.0.0");
    }

    // The payload is listed before anything is written (§6).
    const Value* header = step_for(plan, "squared/gui/include/squared/gui/button.hpp");
    CHECK(header != nullptr);
    CHECK_EQ(std::string{header->string_or("phase", "")}, "package.materialize");
    CHECK_EQ(std::string{header->string_or("origin", "")}, "package.squared-core");
    // D-077: framework source is `shared` -- written for the user to edit, and
    // absorptive on a later pass only while unchanged.
    CHECK_EQ(std::string{header->string_or("ownership", "")}, "shared");

    // The build fragment is the generator's, exactly like a kit's.
    const Value* fragment = step_for(plan, "mk/pkg_squared_core.mk");
    CHECK(fragment != nullptr);
    CHECK_EQ(std::string{fragment->string_or("ownership", "")}, "generated");

    // §2.7.3: nothing generator-owned lands inside the working directory,
    // package contributions included.
    for (const Value& step : *plan.find("steps")->as_array()) {
        const std::string path{step.string_or("path", "")};
        if (path.rfind("sq_app", 0) == 0) {
            CHECK(step.string_or("ownership", "") != "generated");
        }
    }

    const OperationResult generated = engine->execute("project.generate", config);
    CHECK(generated.succeeded());
    CHECK(std::filesystem::exists(target / "squared" / "gui" / "include" / "squared" / "gui" / "button.hpp"));
    CHECK(std::filesystem::exists(target / "mk" / "pkg_squared_core.mk"));

    // D-079: packages are recorded with id and version, like kits.
    Value inspect = Value::object();
    inspect.set("workspace", target.string());
    const OperationResult read_back = engine->execute("workspace.inspect", inspect);
    CHECK(read_back.succeeded());
    const Array* package_rows = read_back.data().find("resources")->find("packages")->as_array();
    CHECK(package_rows != nullptr && package_rows->size() == 1);
    if (package_rows != nullptr && package_rows->size() == 1) {
        CHECK_EQ(std::string{(*package_rows)[0].string_or("id", "")}, "package.squared-core");
        CHECK_EQ(std::string{(*package_rows)[0].string_or("version", "")}, "1.0.0");
    }

    // §2.7.10: provenance rows carry the hash for every package path, in the
    // `shared` class.
    {
        const Array* provenance = read_back.data().find("provenance")->as_array();
        std::size_t package_rows = 0;
        for (const Value& entry : *provenance) {
            if (entry.string_or("origin", "") != "package.squared-core") continue;
            ++package_rows;
            CHECK(entry.string_or("sha256", "").size() == 64U);
            CHECK(support::to_hex(support::sha256(
                      read(target / std::string{entry.string_or("path", "")}))) ==
                  entry.string_or("sha256", ""));
        }
        CHECK(package_rows == 329);

        Value verify = Value::object();
        verify.set("workspace", target.string());
        const OperationResult clean = engine->execute("workspace.verify", verify);
        CHECK(clean.succeeded());
        CHECK(clean.data().int_or("modified", -1) == 0);
        CHECK(clean.data().int_or("conflicts", -1) == 0);
    }

    (void)engine->shutdown();
}

/// D-078: a package claiming a path the template already wrote is refused at
/// plan time — the package must name the path in an explicit `overrides` list.
void package_collision_requires_override() {
    Scratch scratch("pkg-collision");
    const auto packages_root = scratch.child("packages");
    std::filesystem::create_directories(packages_root);

    const auto make_config = [&](const std::string& name, const std::filesystem::path& out) {
        Value config = request(name, out.string(), false);
        config.set("packages", Value::strings({"package.synthetic"}));
        return config;
    };

    // template.terminal.cpp writes a seeded Makefile; the package claims it too.
    write_synthetic_package(packages_root, "package.synthetic", {},
                            {{"tree/Makefile", "package copy\n"}, {"tree/app.mk", "x\n"}});
    {
        auto engine = engine_with_packages_root(packages_root);
        const OperationResult planned =
            engine->execute("project.plan", make_config("hello", scratch.child("out1")));
        CHECK(!planned.succeeded());
        CHECK_EQ(planned.errors().front().code, "kit.integration_point.conflict");
        (void)engine->shutdown();
    }

    // Same package, now declaring the override. The plan succeeds and the
    // package's copy owns the path.
    write_synthetic_package(packages_root, "package.synthetic", {"Makefile"},
                            {{"tree/Makefile", "package copy\n"}, {"tree/app.mk", "x\n"}});
    {
        auto engine = engine_with_packages_root(packages_root);
        const auto out = scratch.child("out2");
        const OperationResult planned =
            engine->execute("project.plan", make_config("hello", out));
        CHECK(planned.succeeded());
        // Both contributions stay in the plan; the later phase wins on disk
        // (§2.7.9 contributions are applied in order), and the overriding
        // package owns the path.
        const std::vector<Value> makefile_steps = steps_named(planned.data(), "Makefile");
        CHECK(makefile_steps.size() == 2);
        CHECK(std::any_of(makefile_steps.begin(), makefile_steps.end(), [](const Value& step) {
            return step.string_or("origin", "") == "package.synthetic";
        }));

        const OperationResult generated = engine->execute("project.generate", make_config("hello", out));
        CHECK(generated.succeeded());
        CHECK_EQ(read(out / "Makefile"), "package copy\n");
        (void)engine->shutdown();
    }
}

/// D-079: a package that resolves with no payload is an authoring bug and is
/// reported, not passed over.
void empty_payload_package_warns() {
    Scratch scratch("pkg-empty");
    const auto packages_root = scratch.child("packages");
    std::filesystem::create_directories(packages_root);
    write_synthetic_package(packages_root, "package.empty", {}, {});

    auto engine = engine_with_packages_root(packages_root);
    Value config = request("hello", scratch.child("out").string(), false);
    config.set("packages", Value::strings({"package.empty"}));

    const OperationResult planned = engine->execute("project.plan", config);
    CHECK(planned.succeeded());
    bool warned = false;
    for (const Diagnostic& warning : planned.warnings()) {
        if (warning.message.find("package.empty") != std::string::npos) warned = true;
    }
    CHECK(warned);

    (void)engine->shutdown();
}

/// D-080: a template's `requires.packages` is advisory -- leaving the package
/// out warns and still works, selecting it silences the warning.
void template_requires_packages_is_advisory() {
    Scratch scratch("pkg-requires");
    auto    engine = ready_engine();

    const Value without =
        request_android("hello", scratch.child("without").string(), {});
    const OperationResult planned = engine->execute("project.plan", without);
    CHECK(planned.succeeded());
    bool warned = false;
    for (const Diagnostic& warning : planned.warnings()) {
        if (warning.message.find("package.squared-core") != std::string::npos) warned = true;
    }
    CHECK(warned);

    const Value with =
        request_android("hello", scratch.child("with").string(), {"package.squared-core"});
    const OperationResult selected = engine->execute("project.plan", with);
    CHECK(selected.succeeded());
    for (const Diagnostic& warning : selected.warnings()) {
        CHECK(warning.message.find("package.squared-core") == std::string::npos);
    }

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
    package_materializes();
    package_collision_requires_override();
    empty_payload_package_warns();
    template_requires_packages_is_advisory();
    return squared::pg::test::report("test_generate");
}
