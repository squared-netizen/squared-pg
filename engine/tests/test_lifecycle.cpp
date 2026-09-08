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

int main() {
    states();
    registry();
    indexing();
    return squared::pg::test::report("test_lifecycle");
}
