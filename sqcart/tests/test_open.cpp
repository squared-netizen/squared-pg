// SPDX-License-Identifier: MIT
//
// Open tests/test.sq (produced by test_write) and exercise the reader facade:
// manifest()/kind(), entries(), find()/contains(), read()/read_into().

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

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
        return test::report("test_open");
    }
    const Cartridge& c = *open;

    CHECK(!c.exploded());
    CHECK(c.manifest().id() == "kit.testcart");
    CHECK(c.manifest().kind() == Kind::kit);

    // The writer wrote: manifest.json, the kit payload, and SQ-INF assets.
    CHECK(c.contains("SQ-INF/manifest.json"));
    CHECK(c.contains("README.md"));
    CHECK(c.contains("bridge/src/bridge.cpp"));

    auto readme = c.find("README.md");
    CHECK(readme.has_value());
    if (readme) {
        CHECK(readme->get().uncompressed_size > 0);
    }
    CHECK(!c.find("tree/nope").has_value());

    // Round-trip a payload file's bytes.
    auto hello = c.read("bridge/src/bridge.cpp");
    CHECK(hello.has_value());
    if (hello) {
        std::string_view text(reinterpret_cast<const char*>(hello->data()), hello->size());
        CHECK(text.find("initialise") != std::string_view::npos);
    }

    // read_into must fill a correctly sized buffer.
    if (readme) {
        std::vector<std::byte> buf(readme->get().uncompressed_size);
        auto n = c.read_into("README.md", buf);
        CHECK(n.has_value());
        CHECK(n && *n == buf.size());
    }

    return test::report("test_open");
}
