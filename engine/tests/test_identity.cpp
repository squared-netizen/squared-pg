// SPDX-License-Identifier: MIT
// Resource identity grammar and references.

#include "squared/pg/identity.hpp"

#include "check.hpp"

using namespace squared::pg;

namespace {

void grammar() {
    CHECK(ResourceId::parse("template.terminal.cpp").has_value());
    CHECK(ResourceId::parse("kit.terminal").has_value());
    CHECK(ResourceId::parse("package.squared.gui").has_value());
    CHECK(ResourceId::parse("kit.acme.raylib").has_value());
    CHECK(ResourceId::parse("kit.lua_host").has_value());

    CHECK(!ResourceId::parse("kit").has_value());          // one segment
    CHECK(!ResourceId::parse("Kit.Terminal").has_value()); // uppercase
    CHECK(!ResourceId::parse("kit..terminal").has_value());
    CHECK(!ResourceId::parse("kit.9lives").has_value());   // segment starts with a digit
    CHECK(!ResourceId::parse("kit.terminal_").has_value());  // trailing separator
    CHECK(!ResourceId::parse("kit.term__inal").has_value());
    CHECK(!ResourceId::parse("").has_value());
    CHECK(!ResourceId::parse("a.b.c.d.e.f.g.h.i").has_value());  // nine segments
}

void kinds() {
    CHECK(ResourceId::parse("template.terminal.cpp", ResourceKind::project_template).has_value());
    // The prefix must agree with the kind; a kit claiming to be a template is
    // exactly the confusion the prefix rule exists to prevent.
    CHECK(!ResourceId::parse("kit.terminal", ResourceKind::project_template).has_value());
    CHECK(ResourceId::parse("kit.terminal")->kind() == ResourceKind::kit);

    CHECK(resource_kind_from_string("template") == ResourceKind::project_template);
    CHECK(resource_kind_from_string("asset_bundle") == ResourceKind::asset);
    CHECK(!resource_kind_from_string("widget").has_value());
}

void references() {
    auto plain = ResourceRef::parse("kit.terminal");
    CHECK(plain.has_value());
    CHECK(plain->range.empty());

    auto pinned = ResourceRef::parse("kit.terminal@^1.0.0");
    CHECK(pinned.has_value());
    CHECK_EQ(pinned->range, "^1.0.0");
    CHECK_EQ(pinned->to_string(), "kit.terminal@^1.0.0");

    CHECK(!ResourceRef::parse("nonsense").has_value());
}

void reserved() {
    CHECK(is_reserved_namespace(ResourceId::parse("kit.squared_pg.internal").value()));
    CHECK(!is_reserved_namespace(ResourceId::parse("kit.terminal").value()));
}

}  // namespace

int main() {
    grammar();
    kinds();
    references();
    reserved();
    return squared::pg::test::report("test_identity");
}
