// SPDX-License-Identifier: MIT
//
// The create op: pack tests/testcart (an exploded kit) into tests/test.sq
// using sqcart::write, under tests/ (not the build dir), without hashes.
// test_open/test_manifest/test_entries DEPEND on this test so the artifact
// exists before the reader exercises it.

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>

#include "check.hpp"
#include "sqcart/sqcart.hpp"

using namespace sqcart;

namespace {

std::string slurp(const std::filesystem::path& p)
{
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

} // namespace

int main(int argc, char** argv)
{
    const std::filesystem::path src_dir =
        argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path(SQCART_TEST_SOURCE_DIR);
    const std::filesystem::path fixture = src_dir / "tests" / "testcart";
    const std::filesystem::path out = src_dir / "tests" / "test.sq";

    // The fixture must carry a non-empty manifest we hand to set_manifest.
    const std::filesystem::path manifest_path = fixture / "SQ-INF" / "manifest.json";
    if (!std::filesystem::exists(manifest_path)) {
        test::check(false, "fixture manifest exists", __FILE__, __LINE__);
        return test::report("test_write");
    }
    const std::string manifest_json = slurp(manifest_path);
    CHECK(!manifest_json.empty());

    WriteOptions opts;
    opts.write_hashes = false; // hashes deferred in this pass

    auto w = Writer::create(out, opts);
    CHECK(w.has_value());
    if (!w) {
        std::fprintf(stderr, "Writer::create: %s\n", w.error().message.c_str());
        return test::report("test_write");
    }

    CHECK((w->set_manifest(manifest_json)).has_value());

    auto tree = w->add_tree(fixture);
    CHECK(tree.has_value());
    if (!tree) {
        std::fprintf(stderr, "add_tree: %s\n", tree.error().message.c_str());
        return test::report("test_write");
    }

    auto done = std::move(*w).finish();
    CHECK(done.has_value());

    std::error_code ec;
    CHECK(std::filesystem::exists(out, ec));
    CHECK(std::filesystem::file_size(out, ec) > 0);

    return test::report("test_write");
}
