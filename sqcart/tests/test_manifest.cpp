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

int main(int argc, char** argv)
{
    const std::filesystem::path src_dir =
        argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path(SQCART_TEST_SOURCE_DIR);
    const std::filesystem::path sq_path = src_dir / "tests" / "test.sq";
    auto open = Cartridge::open(sq_path);
    CHECK(open.has_value());
    if (!open) {
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
