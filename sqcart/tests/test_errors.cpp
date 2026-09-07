// SPDX-License-Identifier: MIT

#include "check.hpp"
#include "sqcart/sqcart.hpp"

using namespace sqcart;

int main() {
    // Every code maps to a category, and the mapping matches format spec §13.
    CHECK(engine_category(ErrorCode::path_unsafe)    == EngineCategory::resource);
    CHECK(engine_category(ErrorCode::kind_invalid)   == EngineCategory::validation);
    CHECK(engine_category(ErrorCode::format_unsupported)
                                                     == EngineCategory::compatibility);
    CHECK(engine_category(ErrorCode::io_failed)      == EngineCategory::filesystem);

    // Wire codes are stable strings; Lua matches on these.
    CHECK(to_string(ErrorCode::limit_exceeded) == "cartridge.limit_exceeded");
    CHECK(to_string(ErrorCode::native_code_prohibited)
                                               == "cartridge.native_code_prohibited");

    // Kind is a token, not an enum, as of format 2. What is checkable is
    // shape; what is deliberately NOT checkable is membership of any list,
    // because this library does not know the ecosystem's roles (§0.1).
    CHECK(valid_kind_token("template"));
    CHECK(valid_kind_token("kit"));
    CHECK(valid_kind_token("asset_bundle"));
    CHECK(valid_kind_token("something_nobody_has_defined"));   // the point
    CHECK(!valid_kind_token("Template"));      // no case folding
    CHECK(!valid_kind_token("asset-bundle"));  // hyphen retired with `id`
    CHECK(!valid_kind_token("2fast"));         // must open on a letter
    CHECK(!valid_kind_token("dotted.kind"));   // one segment, not a hierarchy
    CHECK(!valid_kind_token(""));

    // Error carries its category without a separate lookup.
    const Error e{ErrorCode::integrity_failed, "digest mismatch", {}, {}, false};
    CHECK(e.category() == EngineCategory::resource);

    return test::report("test_errors");
}
