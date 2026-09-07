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
// requires_capabilities used to be checked here.
//
// It was an envelope field this library shape-checked (`name` or
// `name@range`) and never resolved. That was defensible while the argument
// held that a *foreign* tool could enumerate what a cartridge demanded of it
// without knowing whose namespace the tokens belonged to.
//
// The argument did not survive contact with the data. Every token ever
// written is squared-pg's -- `capability.project.plan@^1.0` -- so the field
// was addressed to exactly one consumer while sitting outside that
// consumer's section. It now lives at `consumers.<id>.requires.capabilities`,
// with `engine` alongside it for the same reason: `engine.id` restated the
// consumer key it should have been inside.
//
// What is lost is the shape check, and losing it is right. A malformed token
// now reaches the consumer, which owns the grammar and can say which
// capability is missing and what provides it. This library could only ever
// say that a string it did not understand looked wrong.
//
// requires_features stays in the envelope and is still checked, because it
// names container capabilities that guard *this reader* and fail at open.
// ---------------------------------------------------------------------------


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
    CHECK(m.kind() == "kit");
    CHECK(m.format_version() == kFormatVersion);
    CHECK(m.tree() == ".");
    if (m.title()) {
        CHECK(*m.title() == "Testcart Integration Kit");
    }
    if (m.license()) {
        CHECK(*m.license() == "MIT");
    }
    CHECK(m.authors().size() == 1);
    CHECK(m.authors()[0].name == "sqcart maintainers");

    // The consumer section is carried, addressed and unopened. The fixture
    // names `sqcart_test` rather than a real consumer deliberately: it is a
    // namespace this library has never heard of, which is the whole claim.
    CHECK(m.consumers().size() == 1);
    if (m.consumers().size() == 1) {
        CHECK(m.consumers()[0] == "sqcart_test");
    }

    auto section = m.consumer("sqcart_test");
    CHECK(section.has_value());
    if (section) {
        // Text, not structure. The assertion is that the bytes came through,
        // not that this library understood any of them.
        CHECK(section->find("\"external\"") != std::string_view::npos);
        CHECK(section->find("testcart.bridge") != std::string_view::npos);
        CHECK(section->front() == '{' && section->back() == '}');
    }

    // A namespace nobody wrote is absent, not empty. The distinction matters:
    // a consumer finding nothing addressed to it must say so, not proceed
    // with defaults it invented.
    CHECK(!m.consumer("squared_pg").has_value());
    CHECK(!m.consumer("").has_value());

    CHECK(!m.raw_json().empty());
    CHECK(m.raw_json().find("\"kind\"") != std::string_view::npos);

    return test::report("test_manifest");
}
