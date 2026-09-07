// SPDX-License-Identifier: MIT
//
// Format 2: envelope gating, the payload root grammar, opaque kind tokens,
// and consumer sections (§5.1, §5.2, §5.6).
//
// The through-line is §0.1. Format 1 made the independence requirement false
// in code -- six typed kind bodies meant a framework runtime linking this
// library got `integration_areas` and `project_types` compiled into it. Much
// of what is asserted below is a *refusal to know something*, which is an
// awkward thing to test and the reason the negative cases outnumber the
// positive ones.
//
// Four groups:
//
//   1. The envelope is checked before anything downstream of it is read.
//      Format 1 refused, absent `format_version` refused, wrong `format` tag
//      refused. None of these was enforced at parse time before: the version
//      check lived in validate(), which runs on an already-open cartridge and
//      only if asked, so a cartridge from a future format parsed to
//      completion against this reader's rules and reported nothing.
//
//   2. `tree` admits exactly two shapes and normalises neither.
//
//   3. `kind` is validated for shape and never for membership. The important
//      assertion is the one that *accepts* a token nobody has defined.
//
//   4. `consumers` sections are carried verbatim and never interpreted.

#include <sqcart/sqcart.hpp>

#include <string>

#include "check.hpp"

using namespace sqcart;

namespace {

/// A minimal well-formed manifest with the envelope under test spliced in, so
/// each case differs from a passing one by exactly one field.
std::string with_envelope(const std::string& envelope)
{
    return "{" + envelope + R"("kind":"kit","id":"kit.probe","version":"1.0.0"})";
}

std::string with_tree(const std::string& tree)
{
    return with_envelope(R"("format":"squared-cartridge","format_version":2,"tree":")"
                         + tree + R"(",)");
}

std::string with_kind(const std::string& kind)
{
    return R"({"format":"squared-cartridge","format_version":2,"kind":")" + kind
         + R"(","id":"probe.thing","version":"1.0.0","tree":"."})";
}

std::string with_consumers(const std::string& consumers)
{
    return R"({"format":"squared-cartridge","format_version":2,"kind":"kit",)"
           R"("id":"kit.probe","version":"1.0.0","tree":".","consumers":)"
         + consumers + "}";
}

void format_gating()
{
    {
        auto m = Manifest::parse(with_tree("."));
        CHECK(m.has_value());
        if (m) {
            CHECK(m->format_version() == kFormatVersion);
            CHECK(m->tree() == ".");
            CHECK(m->kind() == "kit");
            CHECK(m->consumers().empty());   // absent is legal
        }
    }

    // Format 1 is refused, and refused as a *format* problem rather than as a
    // complaint about the fields it happens to lack. "manifest requires a
    // string 'tree'" would be true here and useless: the manifest is not
    // missing a field, it is a different format.
    {
        auto m = Manifest::parse(with_envelope(
            R"("format":"squared-cartridge","format_version":1,)"));
        CHECK(!m.has_value());
        if (!m) CHECK(m.error().code == ErrorCode::format_unsupported);
    }

    // A future format is refused by the same path. This reader implements one
    // version; "newer" and "older" are the same answer.
    {
        auto m = Manifest::parse(with_envelope(
            R"("format":"squared-cartridge","format_version":99,"tree":".",)"));
        CHECK(!m.has_value());
        if (!m) CHECK(m.error().code == ErrorCode::format_unsupported);
    }

    // Absent format_version: always REQUIRED in the spec, defaulted to 1 by
    // the parser, so a manifest that never said what it was got treated as
    // whatever this reader wanted.
    CHECK(!Manifest::parse(with_envelope(
        R"("format":"squared-cartridge","tree":".",)")).has_value());

    // Wrong format tag: never read at all before, so any JSON document with
    // the right member names parsed as a cartridge manifest.
    {
        auto m = Manifest::parse(with_envelope(
            R"("format":"some-other-format","format_version":2,"tree":".",)"));
        CHECK(!m.has_value());
        if (!m) CHECK(m.error().code == ErrorCode::format_unsupported);
    }
    CHECK(!Manifest::parse(with_envelope(R"("format_version":2,"tree":".",)")).has_value());
}

void tree_grammar()
{
    CHECK(Manifest::parse(with_tree(".")).has_value());
    CHECK(Manifest::parse(with_tree("tree/")).has_value());
    CHECK(Manifest::parse(with_tree("a/b/c/")).has_value());
    CHECK(Manifest::parse(with_tree("payload_1/")).has_value());

    // No trailing separator: the likeliest authoring mistake, and the one a
    // permissive reader would silently repair into a second spelling.
    CHECK(!Manifest::parse(with_tree("tree")).has_value());
    CHECK(!Manifest::parse(with_tree("a/b")).has_value());

    // Absent. Required means required; there is no convention left to fall
    // back to, which is the point of the field.
    CHECK(!Manifest::parse(with_envelope(
        R"("format":"squared-cartridge","format_version":2,)")).has_value());

    // Traversal and absolute forms. Entry paths are validated relative to
    // this prefix, so a root that can escape defeats every check below it.
    CHECK(!Manifest::parse(with_tree("/tree/")).has_value());
    CHECK(!Manifest::parse(with_tree("../tree/")).has_value());
    CHECK(!Manifest::parse(with_tree("tree/../../x/")).has_value());
    CHECK(!Manifest::parse(with_tree("./tree/")).has_value());

    // The reserved directory is never payload (§4).
    CHECK(!Manifest::parse(with_tree("SQ-INF/")).has_value());
    CHECK(!Manifest::parse(with_tree("SQ-INF/payload/")).has_value());

    // Empty segments and characters §3.3 excludes from entry paths.
    CHECK(!Manifest::parse(with_tree("a//b/")).has_value());
    CHECK(!Manifest::parse(with_tree("c:/x/")).has_value());
    CHECK(!Manifest::parse(with_tree("")).has_value());
}

void kind_is_opaque()
{
    // Shape only.
    CHECK(valid_kind_token("kit"));
    CHECK(valid_kind_token("template"));
    CHECK(valid_kind_token("asset_bundle"));
    CHECK(valid_kind_token("trailing_"));      // an underscore may end it
    CHECK(!valid_kind_token("Kit"));           // no case folding
    CHECK(!valid_kind_token("asset-bundle"));  // hyphen retired with `id`
    CHECK(!valid_kind_token("2fast"));
    CHECK(!valid_kind_token("dotted.kind"));   // one segment, not a hierarchy
    CHECK(!valid_kind_token(""));

    // The assertion this whole change exists for: a kind this library has
    // never heard of parses. A closed enum could not do this, and every value
    // in that enum was a squared-pg concept.
    {
        auto m = Manifest::parse(with_kind("holodisk_volume"));
        CHECK(m.has_value());
        if (m) CHECK(m->kind() == "holodisk_volume");
    }
    CHECK(Manifest::parse(with_kind("kit")).has_value());
    CHECK(Manifest::parse(with_kind("something_nobody_has_defined")).has_value());

    // Malformed tokens are still refused, and as a kind problem.
    {
        auto m = Manifest::parse(with_kind("asset-bundle"));
        CHECK(!m.has_value());
        if (!m) CHECK(m.error().code == ErrorCode::kind_invalid);
    }
    CHECK(!Manifest::parse(with_kind("Kit")).has_value());

    // No body is required, and a body-shaped member is not special. Format 1
    // demanded a member matching `kind` and rejected foreign ones; neither
    // rule survives, because both require knowing what a body is.
    CHECK(Manifest::parse(
        R"({"format":"squared-cartridge","format_version":2,"kind":"kit",)"
        R"("id":"kit.probe","version":"1.0.0","tree":".","kit":{"anything":1}})").has_value());
}

void consumer_sections()
{
    // Carried verbatim, addressed by namespace.
    {
        auto m = Manifest::parse(with_consumers(
            R"({"squared_pg":{"integration_areas":["render.backend"],)"
            R"("ownership":{"user":["**"]}}})"));
        CHECK(m.has_value());
        if (m) {
            CHECK(m->consumers().size() == 1);
            auto s = m->consumer("squared_pg");
            CHECK(s.has_value());
            if (s) {
                CHECK(s->find("render.backend") != std::string_view::npos);
                CHECK(s->find("ownership") != std::string_view::npos);
            }
            // Nothing was interpreted. That `ownership` carries an
            // overlapping `**` is a real defect in squared-pg's schema, and
            // this library now has no opinion about it whatsoever -- which is
            // precisely the change.
            CHECK(!m->consumer("someone_else").has_value());
        }
    }

    // Several namespaces, enumerable and sorted.
    {
        auto m = Manifest::parse(with_consumers(
            R"({"squared_pg":{"a":1},"squared_framework":{"b":2},"holodisk":{"c":3}})"));
        CHECK(m.has_value());
        if (m) {
            const auto names = m->consumers();
            CHECK(names.size() == 3);
            if (names.size() == 3) {
                CHECK(names[0] == "holodisk");            // std::map key order
                CHECK(names[1] == "squared_framework");
                CHECK(names[2] == "squared_pg");
            }
        }
    }

    // Absent and empty are both legal, and are different things.
    {
        auto m = Manifest::parse(with_consumers("{}"));
        CHECK(m.has_value());
        if (m) CHECK(m->consumers().empty());
    }

    // Arbitrary nesting survives the round trip. A consumer's schema may be
    // any shape; this library must not flatten, reorder or truncate it.
    {
        auto m = Manifest::parse(with_consumers(
            R"({"deep":{"a":{"b":{"c":[1,2,{"d":"e"}]}},"n":null,"t":true,"f":1.5}})"));
        CHECK(m.has_value());
        if (m) {
            auto s = m->consumer("deep");
            CHECK(s.has_value());
            if (s) {
                CHECK(s->find("\"e\"") != std::string_view::npos);
                CHECK(s->find("null") != std::string_view::npos);
                CHECK(s->find("true") != std::string_view::npos);
            }
        }
    }

    // Refused: not an object at the top.
    CHECK(!Manifest::parse(with_consumers(R"(["squared_pg"])")).has_value());
    CHECK(!Manifest::parse(with_consumers(R"("squared_pg")")).has_value());

    // Refused: a section that is not an object. The constraint is what makes
    // a section extensible without a version bump -- adding a field adds a
    // member. A bare array would make every addition positional.
    CHECK(!Manifest::parse(with_consumers(R"({"squared_pg":["a","b"]})")).has_value());
    CHECK(!Manifest::parse(with_consumers(R"({"squared_pg":"a string"})")).has_value());
    CHECK(!Manifest::parse(with_consumers(R"({"squared_pg":null})")).has_value());

    // Refused: malformed identities. A namespace is a map key and map keys
    // are attacker-controlled, so the grammar is checked even though the
    // value behind it never is.
    CHECK(!Manifest::parse(with_consumers(R"({"Squared_PG":{}})")).has_value());
    CHECK(!Manifest::parse(with_consumers(R"({"squared-pg":{}})")).has_value());
    CHECK(!Manifest::parse(with_consumers(R"({"":{}})")).has_value());
    CHECK(!Manifest::parse(with_consumers(R"({"trailing.":{}})")).has_value());
    CHECK(!Manifest::parse(with_consumers(R"({"double..dot":{}})")).has_value());
    CHECK(!Manifest::parse(with_consumers(R"({"2fast":{}})")).has_value());

    // Dotted identities are legal: a consumer may namespace itself.
    CHECK(Manifest::parse(with_consumers(R"({"squared.pg":{}})")).has_value());
}

}  // namespace

int main()
{
    format_gating();
    tree_grammar();
    kind_is_opaque();
    consumer_sections();
    return test::report("test_format2");
}
