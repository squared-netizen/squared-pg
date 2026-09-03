// SPDX-License-Identifier: MIT
// Glob matching, path normalisation, digests and substitution.

#include "services/services.hpp"
#include "support/support.hpp"

#include "check.hpp"

using namespace squared::pg;

namespace {

void globs() {
    CHECK(support::glob_match("**", "anything/at/all"));
    CHECK(support::glob_match("*.cpp", "main.cpp"));
    CHECK(!support::glob_match("*.cpp", "src/main.cpp"));  // * does not cross /
    CHECK(support::glob_match("src/*.cpp", "src/main.cpp"));
    CHECK(support::glob_match("sq_app/**", "sq_app/src/deep/file.cpp"));
    // A trailing /** covers the directory itself, so a manifest needs one
    // pattern rather than two.
    CHECK(support::glob_match("sq_app/**", "sq_app"));
    CHECK(!support::glob_match("sq_app/**", "sq_apple/x"));
    CHECK(support::glob_match("mk/kit_?.mk", "mk/kit_a.mk"));
    CHECK(support::glob_match("a/**/b.c", "a/b.c"));  // ** may match nothing
    CHECK(support::glob_match("a/**/b.c", "a/x/y/b.c"));

    // Specificity decides which of two matching rules wins.
    CHECK(support::glob_specificity("mk/squared_generated.mk") > support::glob_specificity("**"));
    CHECK(support::glob_specificity("src/*.cpp") > support::glob_specificity("src/**"));
}

void paths() {
    CHECK_EQ(support::normalize_relative("a/b/c"), "a/b/c");
    CHECK_EQ(support::normalize_relative("./a//b/"), "a/b");
    CHECK_EQ(support::normalize_relative("a\\b"), "a/b");

    // Anything that could leave the workspace is refused rather than resolved.
    // Resolving `..` would mean deciding it is safe, and there is no reading of
    // a manifest path containing `..` that is worth honouring.
    CHECK(support::normalize_relative("../escape").empty());
    CHECK(support::normalize_relative("a/../../escape").empty());
    CHECK(support::normalize_relative("/absolute").empty());
    CHECK(support::normalize_relative("C:/windows").empty());

    const auto ancestors = support::ancestor_directories("a/b/c/file.txt");
    CHECK(ancestors.size() == 3);
    CHECK_EQ(ancestors[0], "a");
    CHECK_EQ(ancestors[2], "a/b/c");
    CHECK(support::ancestor_directories("file.txt").empty());
}

void digests() {
    // FIPS 180-4 / NIST vector for the empty string and for "abc".
    CHECK_EQ(support::to_hex(support::sha256(std::string_view{})),
             "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK_EQ(support::to_hex(support::sha256(std::string_view{"abc"})),
             "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

void substitution() {
    Value parameters = Value::object();
    parameters.set("project_name", "hello");
    parameters.set("count", std::int64_t{3});
    parameters.set("flag", true);

    auto simple = detail::substitute("app {{project_name}} v{{count}} {{flag}}", parameters, "x");
    CHECK(simple.has_value());
    CHECK_EQ(*simple, "app hello v3 true");

    CHECK_EQ(*detail::substitute("{{ project_name }}", parameters, "x"), "hello");

    // §2.8.8: an unknown parameter is an error, never an empty string. A silent
    // substitution failure produces a project that looks right and does not
    // build.
    auto missing = detail::substitute("{{nope}}", parameters, "src/main.cpp");
    CHECK(!missing.has_value());
    CHECK_EQ(missing.error().code, "template.parameter.missing");
    CHECK_EQ(missing.error().path, "src/main.cpp");

    // C++ brace initialisation must survive untouched, or every template
    // containing real C++ would fail to substitute.
    auto braces = detail::substitute("std::vector<int> v{{1, 2}};", parameters, "x");
    CHECK(braces.has_value());
    CHECK_EQ(*braces, "std::vector<int> v{{1, 2}};");

    auto escape = detail::substitute("literal {{\"{{\"}} braces", parameters, "x");
    CHECK(escape.has_value());
    CHECK_EQ(*escape, "literal {{ braces");

    CHECK(detail::looks_textual("plain text"));
    CHECK(!detail::looks_textual(std::string_view{"PNG\0\r\n", 6}));
}

void ownership() {
    sqcart::OwnershipRules rules;
    rules.generated = {"mk/squared_generated.mk", "sq_kit/**"};
    rules.user      = {"**"};

    std::vector<Diagnostic> diagnostics;
    CHECK(detail::classify_path(rules, "mk/squared_generated.mk", &diagnostics) ==
          OwnershipClass::generated);
    CHECK(detail::classify_path(rules, "sq_kit/include/x.hpp", &diagnostics) ==
          OwnershipClass::generated);
    CHECK(detail::classify_path(rules, "sq_app/src/main.cpp", &diagnostics) == OwnershipClass::seeded);

    // §2.7.10: a path matching nothing defaults to seeded. Defaulting to
    // generated would mean the safe case is the one you have to remember.
    sqcart::OwnershipRules empty;
    CHECK(detail::classify_path(empty, "anything", &diagnostics) == OwnershipClass::seeded);
}

}  // namespace

int main() {
    globs();
    paths();
    digests();
    substitution();
    ownership();
    return squared::pg::test::report("test_support");
}
