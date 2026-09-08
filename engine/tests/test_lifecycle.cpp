// SPDX-License-Identifier: MIT
// §2.4.11: the engine is always in exactly one state, and the legality matrix
// is enforced centrally rather than by each entry point.

#include "squared/pg/engine.hpp"

#include "check.hpp"

using namespace squared::pg;

namespace {

EngineConfig config() {
    EngineConfig cfg;
    // The same three trees the host scans (app/src/main.cpp): the framework's
    // resources arrive from the `squared` repository as a sibling directory,
    // squared-pg's own live under resources/generator/, and `resources/` is
    // the un-split or installed shape. A missing root is not an error, so
    // listing all three costs nothing and keeps the tests honest about where
    // resources actually come from after the split.
    for (const char* kind : {"templates", "kits", "packages", "assets"}) {
        const std::string base{SQUARED_PG_TEST_SOURCE_DIR};
        cfg.resource_roots.emplace_back(base + "/resources/" + kind);
        cfg.resource_roots.emplace_back(base + "/resources/generator/" + kind);
    }
    return cfg;
}

void states() {
    auto engine = Engine::create(config());
    CHECK(engine->state() == LifecycleState::created);

    // Operations must not execute before initialization, and the refusal is a
    // structured error rather than a crash or a silent no-op.
    const OperationResult early = engine->execute("engine.describe", {});
    CHECK(!early.succeeded());
    CHECK_EQ(early.errors().front().code, "lifecycle.invalid_state");

    CHECK(engine->initialize().succeeded());
    CHECK(engine->state() == LifecycleState::ready);

    // Initialization must not occur after it has already happened.
    CHECK(!engine->initialize().succeeded());
    CHECK(engine->state() == LifecycleState::ready);

    CHECK(engine->execute("engine.describe", {}).succeeded());

    CHECK(engine->shutdown().succeeded());
    CHECK(engine->state() == LifecycleState::destroyed);

    // A destroyed instance must not be reused.
    CHECK(!engine->execute("engine.describe", {}).succeeded());
    CHECK(!engine->shutdown().succeeded());
}

void registry() {
    auto engine = Engine::create(config());
    CHECK(engine->initialize().succeeded());

    CHECK(!engine->operations().empty());
    CHECK(!engine->capabilities().empty());

    // Every descriptor is reachable by its own identity. This is what the Lua
    // binding relies on when it generates the call surface (§2.6.3).
    for (const OperationDescriptor& descriptor : engine->operations()) {
        const OperationResult result = engine->execute(descriptor.id, Value::object());
        // A descriptor with required parameters fails validation, which is
        // still proof the operation was found; what must never happen is
        // "unknown operation".
        if (!result.succeeded()) {
            CHECK(result.errors().front().code != "validation.operation.unknown");
        }
    }

    const OperationResult unknown = engine->execute("no.such.operation", {});
    CHECK(!unknown.succeeded());
    CHECK_EQ(unknown.errors().front().code, "validation.operation.unknown");

    // §2.7.2: a missing required parameter is reported by name, and every
    // offender is listed, not just the first.
    const OperationResult empty = engine->execute("project.plan", Value::object());
    CHECK(!empty.succeeded());
    CHECK_EQ(empty.errors().front().code, "validation.parameter.invalid");
    const Value* missing = empty.errors().front().diagnostics.find("missing");
    CHECK(missing != nullptr && missing->as_array()->size() == 3);

    CHECK(engine->shutdown().succeeded());
}

void indexing() {
    auto engine = Engine::create(config());
    CHECK(engine->initialize().succeeded());

    const ResourceIndex& index = engine->resources();
    CHECK(index.size() >= 3);

    auto id = ResourceId::parse("template.terminal.cpp").value();
    CHECK(index.versions_of(id).size() == 1);
    CHECK(index.select(id, VersionRange::parse("^0.1.0").value()) != nullptr);
    CHECK(index.select(id, VersionRange::parse(">=9.0.0").value()) == nullptr);

    CHECK(index.of_kind(ResourceKind::kit).size() >= 2);

    CHECK(engine->shutdown().succeeded());
}

}  // namespace

/// A duplicate identity warns; it does not stop the index (D-058).
///
/// It used to fail `build_index`, which meant a duplicate anywhere stopped
/// the tool before it did anything -- including `sqpg list`, the command you
/// reach for to find out where the duplicate is. An index that refuses to be
/// inspected because it contains a problem is the worst shape for diagnosing
/// that problem.
void duplicate_identity_is_deferred() {
    ResourceIndex index;

    ResourceRecord first;
    first.id       = ResourceId::parse("template.probe").value();
    first.version  = Version::parse("1.0.0").value();
    first.kind     = ResourceKind::project_template;
    first.location = "/a/template.probe";

    ResourceRecord second = first;
    second.location = "/b/template.probe";

    CHECK(!index.insert(first).has_value());          // no conflict

    const auto clash = index.insert(second);
    CHECK(clash.has_value());
    if (clash) {
        // Both paths, because "two resources declare the same identity"
        // without saying which two leaves the reader to rediscover what the
        // tool already knew.
        CHECK(clash->kept == std::filesystem::path{"/a/template.probe"});
        CHECK(clash->discarded == std::filesystem::path{"/b/template.probe"});
    }

    // The index is usable. This is the whole point: the first copy is held,
    // the second dropped, and everything that does not touch the ambiguous
    // identity keeps working.
    CHECK(index.size() == 1);
    CHECK(index.of_kind(ResourceKind::project_template).size() == 1);

    // And resolution can find out. §2.8.1's prohibition on silent override is
    // kept here rather than at insert: refused where a wrong answer would do
    // harm, not where any answer would do.
    CHECK(index.conflict_for(first.id) != nullptr);
    CHECK(index.conflict_for(ResourceId::parse("template.other").value()) == nullptr);
    CHECK(index.conflicts().size() == 1);
}

int main() {
    duplicate_identity_is_deferred();
    states();
    registry();
    indexing();
    return squared::pg::test::report("test_lifecycle");
}
