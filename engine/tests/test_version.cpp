// SPDX-License-Identifier: MIT
// SemVer parsing, precedence and the §2.8.2 comparator grammar.

#include "squared/pg/version.hpp"

#include "check.hpp"

using namespace squared::pg;

namespace {

Version v(const char* text) { return Version::parse(text).value(); }

bool accepts(const char* range, const char* version) {
    return VersionRange::parse(range).value().satisfied_by(v(version));
}

void parsing() {
    CHECK(Version::parse("1.2.3").has_value());
    CHECK(Version::parse("0.0.0").has_value());
    CHECK(Version::parse("1.0.0-alpha.1+build.7").has_value());
    CHECK(Version::parse("v1.2.3").has_value());  // tolerated leading v

    CHECK(!Version::parse("1.2").has_value());
    CHECK(!Version::parse("1.2.3.4").has_value());
    CHECK(!Version::parse("01.2.3").has_value());   // leading zero
    CHECK(!Version::parse("1.2.x").has_value());
    CHECK(!Version::parse("").has_value());
    CHECK(!Version::parse("1.0.0-01").has_value());  // numeric prerelease, leading zero

    CHECK_EQ(v("1.2.3-rc.1+meta").to_string(), "1.2.3-rc.1+meta");
}

void precedence() {
    CHECK(v("1.0.0") < v("2.0.0"));
    CHECK(v("1.2.0") < v("1.10.0"));
    // SemVer §11.3: a pre-release ranks below the release it precedes. This is
    // the rule people get backwards, so it is worth asserting directly.
    CHECK(v("1.0.0-alpha") < v("1.0.0"));
    CHECK(v("1.0.0-alpha") < v("1.0.0-alpha.1"));
    CHECK(v("1.0.0-alpha.1") < v("1.0.0-alpha.beta"));
    CHECK(v("1.0.0-beta") < v("1.0.0-beta.2"));
    CHECK(v("1.0.0-beta.11") > v("1.0.0-beta.2"));  // numeric, not lexical
    CHECK(v("1.0.0-rc.1") < v("1.0.0"));
    // Build metadata is ignored for ordering.
    CHECK(v("1.0.0+a") == v("1.0.0+b"));
}

void ranges() {
    CHECK(VersionRange::parse("")->unconstrained());
    CHECK(VersionRange::parse("*")->unconstrained());

    CHECK(accepts("1.2.3", "1.2.3"));
    CHECK(!accepts("1.2.3", "1.2.4"));

    CHECK(accepts(">=1.0.0 <2.0.0", "1.9.9"));
    CHECK(!accepts(">=1.0.0 <2.0.0", "2.0.0"));
    CHECK(!accepts(">=1.0.0 <2.0.0", "0.9.0"));

    CHECK(accepts("^1.2.3", "1.9.0"));
    CHECK(!accepts("^1.2.3", "2.0.0"));
    CHECK(!accepts("^1.2.3", "1.2.2"));
    // Below 1.0.0 SemVer moves the compatibility boundary down a level.
    CHECK(accepts("^0.2.3", "0.2.9"));
    CHECK(!accepts("^0.2.3", "0.3.0"));
    CHECK(accepts("^0.0.3", "0.0.3"));
    CHECK(!accepts("^0.0.3", "0.0.4"));

    CHECK(accepts("~1.2.3", "1.2.9"));
    CHECK(!accepts("~1.2.3", "1.3.0"));

    // §2.8.2: a pre-release is never selected unless a clause named one at the
    // same version. Without this, ">=1.0.0" would happily pick 2.0.0-alpha.
    CHECK(!accepts(">=1.0.0", "2.0.0-alpha.1"));
    CHECK(accepts(">=2.0.0-alpha.1", "2.0.0-alpha.1"));
}

}  // namespace

int main() {
    parsing();
    precedence();
    ranges();
    return squared::pg::test::report("test_version");
}
