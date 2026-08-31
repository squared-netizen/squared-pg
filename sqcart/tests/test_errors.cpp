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

    // Kind round-trips through its manifest spelling. Note "template", not
    // "project_template" -- the enumerator is renamed only because template
    // is a keyword.
    CHECK(to_string(Kind::project_template) == "template");
    CHECK(kind_from_string("template")      == Kind::project_template);
    CHECK(kind_from_string("asset-bundle")  == Kind::asset_bundle);
    CHECK(!kind_from_string("Template").has_value());   // no case folding
    CHECK(!kind_from_string("resource").has_value());

    // Error carries its category without a separate lookup.
    const Error e{ErrorCode::integrity_failed, "digest mismatch", {}, {}, false};
    CHECK(e.category() == EngineCategory::resource);

    return test::report("test_errors");
}
