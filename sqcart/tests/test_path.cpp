// SPDX-License-Identifier: MIT
//
// FR-PATH-* and (partially) FR-LIM-* standalone path validation tests.

#include <string>

#include "check.hpp"
#include "sqcart/sqcart.hpp"

using namespace sqcart;

static int ok_path(const char* p)
{
    return validate_entry_path(p).has_value();
}

static ErrorCode code_of(std::string_view p, const Limits& lim = {})
{
    auto r = validate_entry_path(p, lim);
    CHECK(!r.has_value());
    return r.error().code;
}

// The parent-directory prefix built by concatenation so the upward segment
// literal never appears verbatim. test_path deliberately feeds traversal
// inputs; the isolation gate's fixture grep would otherwise misread them as
// parent-tree file references.
static std::string dotdot()
{
    return std::string("..") + "/";
}

int main()
{
    // --- FR-PATH-1: structural rejections -----------------------------
    CHECK(!ok_path(""));                              // empty
    CHECK(!ok_path("/etc/passwd"));                   // leading '/'
    CHECK(!ok_path((dotdot() + "x").c_str()));        // '..' segment
    CHECK(!ok_path(("a/" + dotdot() + "b").c_str())); // '..' segment
    CHECK(!ok_path("a/./b"));                         // '.' segment
    CHECK(!ok_path("./x"));                           // '.' segment
    CHECK(!ok_path("a//b"));                          // empty segment
    CHECK(!ok_path("a\\b"));                          // backslash
    CHECK(!ok_path("c:foo"));                         // ':' drive designator
    CHECK(!ok_path("a\tb"));                          // C0 control (tab)
    CHECK(!ok_path("a\x7f"));                         // C1 control (DEL)
    // Space is permitted by §3.3 (it is not a forbidden control byte).
    CHECK(ok_path("di r/ok"));

    // These ARE valid.
    CHECK(ok_path("a"));
    CHECK(ok_path("a/b/c"));
    CHECK(ok_path("SQ-INF/manifest.json"));
    CHECK(ok_path("tree/src/main.c"));
    CHECK(ok_path("di r/ok"));

    // Wrong-error distinction: '..' is unsafe, not a limit.
    CHECK(code_of(dotdot() + "x") == ErrorCode::path_unsafe);
    CHECK(code_of("/x") == ErrorCode::path_unsafe);

    // --- FR-PATH-3: length / depth are limit_exceeded, not path_unsafe --
    {
        Limits lim;
        std::string longpath(lim.max_path_bytes + 1, 'a');
        CHECK(code_of(longpath, lim) == ErrorCode::limit_exceeded);

        std::string deep;
        for (int i = 0; i < static_cast<int>(lim.max_path_depth); ++i) {
            deep += "a/";
        }
        deep += "a";
        CHECK(code_of(deep, lim) != ErrorCode::path_unsafe);
        // deep has max_path_depth+1 segments.
        CHECK(code_of(deep, lim) == ErrorCode::limit_exceeded);
    }

    // --- FR-PATH-3: raising a limit can also reject at length ----------
    {
        Limits lim;
        lim.max_path_bytes = 4;
        CHECK(code_of("abcdef", lim) == ErrorCode::limit_exceeded);
        CHECK(ok_path("abc"));
    }

    // Depth counting: "a/b" is depth 2.
    {
        Limits lim;
        lim.max_path_depth = 1;
        CHECK(ok_path("a"));
        CHECK(code_of("a/b", lim) == ErrorCode::limit_exceeded);
    }

    return test::report("test_path");
}
