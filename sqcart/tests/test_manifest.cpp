// SPDX-License-Identifier: MIT
//
// Manifest envelope + kind-body parsing, and the pointer-free kind accessors
// (as_kit etc. return optional<reference_wrapper>).

#include <filesystem>
#include <string>
#include <string_view>

#include "check.hpp"
#include "sqcart/sqcart.hpp"

using namespace sqcart;


// ---------------------------------------------------------------------------
// requires_capabilities (§5.4)
//
// The consumer-facing sibling of requires_features. sqcart validates shape and
// nothing else: it must not learn what a token means, or it would need to be
// taught every consumer's vocabulary and every new consumer capability would
// become a sqcart release.
// ---------------------------------------------------------------------------

static void requires_capabilities_checks()
{
    const auto with = [](const char* capabilities) {
        return std::string(R"({"format":"squared-cartridge","format_version":1,)")
             + R"("kind":"kit","id":"kit.probe","version":"1.0.0",)"
             + R"("requires_capabilities":)" + capabilities + ","
             + R"("kit":{"external":{"id":"x","version":"1.0.0"},"provides":[]}})";
    };

    {
        auto m = Manifest::parse(with(R"(["capability.template.substitute@^1.0","capability.project.plan"])"));
        CHECK(m.has_value());
        if (m) {
            CHECK(m->requires_capabilities().size() == 2);
            CHECK(m->requires_capabilities()[0] == "capability.template.substitute@^1.0");
            CHECK(m->requires_capabilities()[1] == "capability.project.plan");
        }
    }

    // Absent is legal and means "nothing beyond the baseline".
    {
        auto m = Manifest::parse(
            R"({"format":"squared-cartridge","format_version":1,"kind":"kit","id":"kit.probe",)"
            R"("version":"1.0.0","kit":{"external":{"id":"x","version":"1.0.0"},"provides":[]}})");
        CHECK(m.has_value());
        if (m) CHECK(m->requires_capabilities().empty());
    }

    // Shape violations. Each is something the manifest author can fix; letting
    // one through would push the failure to a consumer that can only say it
    // does not recognise the token, not what is wrong with it.
    CHECK(!Manifest::parse(with(R"("not-an-array")")).has_value());
    CHECK(!Manifest::parse(with(R"([""])")).has_value());
    CHECK(!Manifest::parse(with(R"(["Capability.Upper"])")).has_value());
    CHECK(!Manifest::parse(with(R"(["trailing."])")).has_value());
    CHECK(!Manifest::parse(with(R"(["double..dot"])")).has_value());
    CHECK(!Manifest::parse(with(R"(["9leading"])")).has_value());
    CHECK(!Manifest::parse(with(R"(["trailing_at@"])")).has_value());

    // A repeat is an authoring mistake; a silently deduplicated list hides it.
    CHECK(!Manifest::parse(with(R"(["capability.a","capability.a"])")).has_value());

    // sqcart must NOT reject a token it has never heard of. Whether a
    // capability exists is the consumer's question.
    CHECK(Manifest::parse(with(R"(["something.nobody.implements@^99.0"])")).has_value());
}

int main(int argc, char** argv)
{
    const std::filesystem::path src_dir =
        argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path(SQCART_TEST_SOURCE_DIR);
    const std::filesystem::path sq_path = src_dir / "tests" / "test.sq";
    auto open = Cartridge::open(sq_path);
    CHECK(open.has_value());
    if (!open) {
        requires_capabilities_checks();

    return test::report("test_manifest");
    }
    const Manifest& m = open->manifest();

    CHECK(m.id() == "kit.testcart");
    CHECK(m.kind() == Kind::kit);
    CHECK(m.format_version() == 1);
    if (m.title()) {
        CHECK(*m.title() == "Testcart Integration Kit");
    }
    if (m.license()) {
        CHECK(*m.license() == "MIT");
    }
    CHECK(m.authors().size() == 1);
    CHECK(m.authors()[0].name == "sqcart maintainers");

    // Exactly the kit body is engaged; the other five accessors are empty.
    CHECK(m.as_kit().has_value());
    CHECK(m.as_cartridge().has_value() == false);
    CHECK(m.as_template().has_value() == false);
    CHECK(m.as_package().has_value() == false);
    CHECK(m.as_assets().has_value() == false);
    CHECK(m.as_plugin().has_value() == false);

    if (m.as_kit()) {
        const KitBody& kit = m.as_kit()->get();
        CHECK(kit.external.id == "testcart");
        CHECK(kit.external.version == ">=1.0.0 <2.0.0");
        CHECK(kit.provides.size() == 1 && kit.provides[0] == "testcart.bridge");
    }

    CHECK(!m.raw_json().empty());
    CHECK(m.raw_json().find("\"kind\"") != std::string_view::npos);

    // A wrong-kind guess is empty, never a throw (no raw pointers).
    auto c = m.as_cartridge();
    CHECK(!c.has_value());

    return test::report("test_manifest");
}
