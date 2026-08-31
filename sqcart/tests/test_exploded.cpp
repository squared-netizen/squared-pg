// SPDX-License-Identifier: MIT
//
// FR-OPEN-1 and the archived/exploded equivalence of format spec §3.5.
// The draft returned "exploded cartridges not implemented"; these are the
// tests that keep that from regressing.

#include "check.hpp"
#include "sqcart/sqcart.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <system_error>
#include <fstream>

namespace {

/// Non-throwing temp directory. The throwing overload aborts the whole test
/// binary when TMPDIR is unset or bad, hiding every later case; on Termux
/// TMPDIR is not /tmp, so this is a live concern rather than a theoretical
/// one.
std::filesystem::path temp_root(const char* suite)
{
    std::error_code ec;
    auto base = std::filesystem::temp_directory_path(ec);
    if (ec) {
        std::fprintf(stderr, "FAIL %s: no usable temp directory (set TMPDIR)\n", suite);
        base = std::filesystem::current_path(ec);
        if (ec) {
            return {};
        }
    }
    return base;
}

}  // namespace

using namespace sqcart;
namespace fs = std::filesystem;

int main()
{
    auto ex = Cartridge::open("tests/testcart");
    CHECK(ex.has_value());
    if (!ex) {
        return test::report("test_exploded");
    }

    CHECK(ex->exploded());
    CHECK(ex->manifest().kind() == Kind::kit);
    CHECK(ex->contains("SQ-INF/manifest.json"));
    CHECK(ex->contains("bridge/src/bridge.cpp"));
    CHECK(!ex->contains("tree/src/nonexistent.lua"));

    // FR-ENTRY-1: exploded order is lexicographic and stable across calls.
    {
        std::vector<std::string> a, b;
        for (const auto& e : ex->entries()) a.push_back(e.path);
        for (const auto& e : ex->entries()) b.push_back(e.path);
        CHECK(a == b);
        CHECK(std::is_sorted(a.begin(), a.end()));
    }

    // Reads return real content, not a stub.
    {
        auto data = ex->read("bridge/src/bridge.cpp");
        CHECK(data.has_value());
        if (data) {
            CHECK(!data->empty());
        }
    }

    // FR-ENTRY-4: too-small buffer fails and writes nothing.
    {
        auto info = ex->find("bridge/src/bridge.cpp");
        CHECK(info.has_value());
        if (info) {
            std::vector<std::byte> small(1, std::byte{0xAB});
            auto n = ex->read_into("bridge/src/bridge.cpp", small);
            CHECK(!n.has_value());
            CHECK(small[0] == std::byte{0xAB});   // untouched
        }
    }

    // A directory without SQ-INF/manifest.json is not a cartridge.
    {
        const fs::path tmp = temp_root("test_exploded") / "sqcart_not_a_cartridge";
        fs::remove_all(tmp);
        fs::create_directories(tmp);
        std::ofstream(tmp / "readme.txt") << "hi";
        auto bad = Cartridge::open(tmp);
        CHECK(!bad.has_value());
        if (!bad) {
            CHECK(bad.error().code == ErrorCode::manifest_missing);
        }
        fs::remove_all(tmp);
    }

    // A path that does not exist at all is an IO failure, distinct from a
    // directory that exists but is not a cartridge.
    {
        auto missing = Cartridge::open("tests/definitely_not_here");
        CHECK(!missing.has_value());
        if (!missing) {
            CHECK(missing.error().code == ErrorCode::io_failed);
        }
    }

    return test::report("test_exploded");
}
