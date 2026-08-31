// SPDX-License-Identifier: MIT
//
// Exercises the Result channel through whichever expected implementation the
// toolchain selected. These tests must pass identically with and without
// std::expected; if they diverge, the fallback is not source-compatible.

#include "check.hpp"
#include "sqcart/sqcart.hpp"

#include <string>

using namespace sqcart;

namespace {

Result<int> succeed() { return 42; }

Result<int> fail() {
    return unexpected(Error{ErrorCode::manifest_missing, "no manifest", {}, {}, false});
}

Result<void> succeed_void() { return {}; }

Result<void> fail_void() {
    return unexpected(Error{ErrorCode::io_failed, "read error", {}, {}, true});
}

}  // namespace

int main() {
    const auto ok = succeed();
    CHECK(ok.has_value());
    CHECK(static_cast<bool>(ok));
    CHECK(*ok == 42);
    CHECK(ok.value() == 42);

    const auto bad = fail();
    CHECK(!bad.has_value());
    CHECK(!static_cast<bool>(bad));
    CHECK(bad.error().code == ErrorCode::manifest_missing);
    CHECK(bad.error().category() == EngineCategory::resource);
    CHECK(bad.error().message == "no manifest");
    CHECK(!bad.error().entry.has_value());

    CHECK(succeed_void().has_value());

    const auto bad_void = fail_void();
    CHECK(!bad_void.has_value());
    CHECK(bad_void.error().code == ErrorCode::io_failed);
    CHECK(bad_void.error().recoverable);

    // Report which implementation ran, so an isolation-gate log shows it.
#if SQCART_HAVE_STD_EXPECTED
    std::fprintf(stderr, "     using std::expected\n");
#else
    std::fprintf(stderr, "     using sqcart fallback expected\n");
#endif

    return test::report("test_expected");
}
