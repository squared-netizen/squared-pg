// SPDX-License-Identifier: MIT
//
// Engine paths that template.android.cpp is the first resource to exercise.
//
// Each was implemented and unreachable until now: no shipped template had a
// binary payload, declared a single-arity integration area, used the
// java_package parameter type, or derived a default from another parameter.
// Code with no caller is code with no evidence.
//
// Runs against the real resources/ tree, matching test_generate.cpp: the thing
// under test is that the *shipped* template behaves, not that a fixture does.

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

class Scratch {
public:
    explicit Scratch(const char* name) {
        const char* base = std::getenv("TMPDIR");
        root_ = std::filesystem::path{base != nullptr ? base : "."} / (std::string{"sqpg-a-"} + name);
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
        std::filesystem::create_directories(root_, ec);
    }
    ~Scratch() {
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }
    [[nodiscard]] std::filesystem::path child(const char* name) const { return root_ / name; }

private:
    std::filesystem::path root_;
};

Value request(const std::string& output, bool with_kit, const char* package = "com.example.myapp") {
    Value config = Value::object();
    config.set("name", "myapp");
    config.set("output", output);
    config.set("template", "template.android.cpp");

    Array kits;
    if (with_kit) kits.emplace_back("kit.opengl");
    config.set("kits", Value{std::move(kits)});

    Array platforms;
    platforms.emplace_back("android");
    config.set("platforms", Value{std::move(platforms)});
    config.set("framework", "1.0.0");

    Value parameters = Value::object();
    if (package != nullptr) parameters.set("package_name", package);
    config.set("parameters", parameters);
    return config;
}

std::string read(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

/// Strip XML comments.
///
/// The manifest explains in prose why it carries no <uses-sdk>, which means a
/// naive search for that string finds the explanation. Testing the document
/// rather than its commentary is the point.
std::string without_comments(std::string text) {
    std::string out;
    std::size_t i = 0;
    while (i < text.size()) {
        const std::size_t open = text.find("<!--", i);
        if (open == std::string::npos) {
            out.append(text, i, std::string::npos);
            break;
        }
        out.append(text, i, open - i);
        const std::size_t close = text.find("-->", open);
        if (close == std::string::npos) break;
        i = close + 3;
    }
    return out;
}

const Value* step_for(const Value& plan, const std::string& path) {
    for (const Value& step : *plan.find("steps")->as_array()) {
        if (step.string_or("path", {}) == path) return &step;
    }
    return nullptr;
}

// --- binary payloads ------------------------------------------------------

void binary_payload_is_copied_verbatim() {
    Scratch scratch("binary");
    auto    engine = ready_engine();
    const auto target = scratch.child("myapp");

    CHECK(engine->execute("project.generate", request(target.string(), true)).succeeded());

    const auto icon = target / "sq_android" / "res" / "mipmap-hdpi" / "ic_launcher.png";
    CHECK(std::filesystem::exists(icon));

    const std::string generated = read(icon);

    // The template's own copy, wherever the framework resources happen to
    // live. After the repository split they arrive from `squared` as a
    // sibling; before it, and in an installed environment, they sit under
    // resources/. Searching rather than hardcoding one, because a test that
    // silently read an empty file would compare two empty strings and pass.
    const std::string relative =
        "resources/templates/template.android.cpp/tree/sq_android/res/mipmap-hdpi/"
        "ic_launcher.png";
    std::filesystem::path source_path;
    for (const char* prefix : {""}) {
        const auto candidate = std::filesystem::path{kSource} / (std::string{prefix} + relative);
        if (std::filesystem::exists(candidate)) {
            source_path = candidate;
            break;
        }
    }
    CHECK(!source_path.empty());
    const std::string source = read(source_path);

    // Byte-identical. §2.8.8: substitution into a binary payload would corrupt
    // it in a way nothing downstream would notice, so the NUL-byte check has
    // to actually fire on a real PNG rather than only on a unit-test string.
    CHECK(!generated.empty());
    CHECK_EQ(support::to_hex(support::sha256(generated)), support::to_hex(support::sha256(source)));

    // A PNG signature survived, which it would not have if a `{{` sequence in
    // the compressed data had been touched.
    CHECK(generated.size() > 8 && generated.compare(1, 3, "PNG") == 0);

    (void)engine->shutdown();
}

// --- integration arity ----------------------------------------------------

void single_arity_area_admits_one_contributor() {
    auto engine = ready_engine();

    // One renderer resolves.
    CHECK(engine->execute("project.plan", request("/nonexistent/a", true)).succeeded());

    // §2.9.3: a `single`-arity area with two contributors is a conflict
    // detected before any mutation. render.backend is the first area in the
    // repository where this can happen.
    Value two = request("/nonexistent/b", true);
    Array kits;
    kits.emplace_back("kit.opengl");
    kits.emplace_back("kit.opengl");  // the same kit twice still claims the area twice
    two.set("kits", Value{std::move(kits)});

    const OperationResult result = engine->execute("project.plan", two);
    CHECK(!result.succeeded());
    CHECK_EQ(result.errors().front().code, "kit.integration_point.conflict");

    (void)engine->shutdown();
}

// --- parameter types ------------------------------------------------------

void java_package_type_is_enforced() {
    auto engine = ready_engine();

    for (const char* bad : {"notapackage", "com..example", "com.9lives", "com.a-b", ""}) {
        const OperationResult result =
            engine->execute("project.plan", request("/nonexistent/x", true, bad));
        CHECK(!result.succeeded());
        if (!result.errors().empty()) {
            // An empty value reads as absent, so it is `missing` rather than
            // `invalid`; both are the contract being enforced.
            const std::string code = result.errors().front().code;
            CHECK(code == "template.parameter.invalid" || code == "template.parameter.missing");
        }
    }

    // `Com.Example` is deliberately absent from the list above. Uppercase
    // segments are unconventional for an Android application id but are legal
    // Java identifiers, and the type is `java_package` -- rejecting them would
    // be the engine enforcing a style preference rather than a grammar.
    for (const char* good : {"com.example.myapp", "io.squared.app", "a.b", "Com.Example"}) {
        CHECK(engine->execute("project.plan", request("/nonexistent/x", true, good)).succeeded());
    }

    (void)engine->shutdown();
}

void default_from_resolves_against_builtins() {
    Scratch scratch("defaults");
    auto    engine = ready_engine();
    const auto target = scratch.child("myapp");

    // app_label declares `default_from: project_name`, and project_name is an
    // engine built-in rather than a workflow-supplied value. Declared defaults
    // therefore have to be applied *after* the built-ins; applying them first
    // leaves the reference unresolved and substitution fails.
    CHECK(engine->execute("project.generate", request(target.string(), true)).succeeded());

    const std::string strings = read(target / "sq_android" / "res" / "values" / "strings.xml");
    CHECK(strings.find("<string name=\"app_name\">myapp</string>") != std::string::npos);

    // The literal default landed too.
    const std::string readme = read(target / "README.md");
    CHECK(readme.find("Squared framework") != std::string::npos);

    (void)engine->shutdown();
}

// --- the template's own guarantees ----------------------------------------

void platform_layer_is_seeded_not_generated() {
    auto engine = ready_engine();
    const OperationResult planned = engine->execute("project.plan", request("/nonexistent/x", true));
    CHECK(planned.succeeded());
    const Value& plan = planned.data();

    // sq_android/ is the escape hatch: the user may edit all of it, so nothing
    // in it may be generator-owned.
    for (const Value& step : *plan.find("steps")->as_array()) {
        const std::string path{step.string_or("path", "")};
        if (path.rfind("sq_android", 0) == 0 || path.rfind("sq_app", 0) == 0) {
            CHECK(step.string_or("ownership", "") != "generated");
        }
    }

    // The build fragments are the generator's, and the Makefile that includes
    // them is the user's. That split is what replaces in-place merging.
    const Value* fragment = step_for(plan, "mk/squared_generated.mk");
    CHECK(fragment != nullptr && fragment->string_or("ownership", "") == "generated");
    const Value* packaging = step_for(plan, "mk/squared_android_package.mk");
    CHECK(packaging != nullptr && packaging->string_or("ownership", "") == "generated");
    const Value* makefile = step_for(plan, "Makefile");
    CHECK(makefile != nullptr && makefile->string_or("ownership", "") == "seeded");

    // The kit's header is the kit's.
    const Value* header = step_for(plan, "sq_kit/include/opengl/gl.hpp");
    CHECK(header != nullptr && header->string_or("origin", "") == "kit.opengl");

    (void)engine->shutdown();
}

void builds_without_a_rendering_kit() {
    Scratch scratch("nokit");
    auto    engine = ready_engine();
    const auto target = scratch.child("nokit");

    // The template requires no rendering kit, which is what makes kit.opengl
    // and a future kit.sfml alternatives rather than one being baked in.
    CHECK(engine->execute("project.generate", request(target.string(), false)).succeeded());

    CHECK(std::filesystem::exists(target / "sq_android" / "entry.cpp"));
    CHECK(std::filesystem::exists(target / "Makefile"));
    CHECK(!std::filesystem::exists(target / "sq_kit"));
    CHECK(!std::filesystem::exists(target / "mk" / "kit_opengl.mk"));

    // The guard, not a hard include: this is the line that lets a kit be added
    // later without editing code already written.
    const std::string entry = read(target / "sq_android" / "entry.cpp");
    CHECK(entry.find("__has_include(<opengl/gl.hpp>)") != std::string::npos);

    (void)engine->shutdown();
}

void manifest_carries_no_uses_sdk() {
    Scratch scratch("manifest");
    auto    engine = ready_engine();
    const auto target = scratch.child("myapp");

    CHECK(engine->execute("project.generate", request(target.string(), true)).succeeded());

    const std::string manifest = without_comments(read(target / "sq_android" / "AndroidManifest.xml"));
    CHECK(manifest.find("package=\"com.example.myapp\"") != std::string::npos);
    CHECK(manifest.find("android.app.lib_name\" android:value=\"myapp\"") != std::string::npos);

    // SDK levels are build settings, not generation parameters: they reach
    // aapt2 through make variables, so retargeting never means editing a file.
    // Declaring them here as well would create a second source of truth, and
    // aapt2 would take this one.
    CHECK(manifest.find("<uses-sdk") == std::string::npos);
    CHECK(manifest.find("minSdkVersion") == std::string::npos);
    CHECK(manifest.find("android:debuggable") == std::string::npos);

    (void)engine->shutdown();
}

}  // namespace

int main() {
    binary_payload_is_copied_verbatim();
    single_arity_area_admits_one_contributor();
    java_package_type_is_enforced();
    default_from_resolves_against_builtins();
    platform_layer_is_seeded_not_generated();
    builds_without_a_rendering_kit();
    manifest_carries_no_uses_sdk();
    return squared::pg::test::report("test_android");
}
