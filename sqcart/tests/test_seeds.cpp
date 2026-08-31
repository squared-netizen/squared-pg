// SPDX-License-Identifier: MIT
//
// Seed drift check.
//
// assets/seeds/*.json are the canonical manifest seeds; app/src/seeds.cpp
// embeds them as raw string literals so the CLI needs no runtime asset
// lookup. Two copies of anything drift. This asserts byte equality, so
// editing the JSON without mirroring it fails here rather than shipping a
// binary that disagrees with the repository.
//
// It also parses each seed with its placeholders filled, because a seed that
// does not produce a valid manifest is only discovered at `create` time
// otherwise -- and by then the user thinks their folder is at fault.

#include "check.hpp"
#include "sqcart/sqcart.hpp"

#include "../app/src/seeds.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace sqcart;
namespace fs = std::filesystem;

namespace {

std::string slurp(const fs::path& p)
{
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string replace_all(std::string s, std::string_view from, std::string_view to)
{
    std::size_t at = 0;
    while ((at = s.find(from, at)) != std::string::npos) {
        s.replace(at, from.size(), to);
        at += to.size();
    }
    return s;
}

}  // namespace

int main()
{
    const fs::path seeds_dir = "assets/seeds";
    CHECK(fs::is_directory(seeds_dir));
    if (!fs::is_directory(seeds_dir)) {
        std::fprintf(stderr, "     (run from the sqcart root)\n");
        return 1;
    }

    const auto kinds = app::seed_kinds();
    CHECK(!kinds.empty());

    for (auto kind : kinds) {
        const fs::path file = seeds_dir / (std::string(kind) + ".json");

        // Every embedded seed has a file behind it.
        CHECK(fs::is_regular_file(file));
        if (!fs::is_regular_file(file)) {
            continue;
        }

        auto embedded = app::seed_for(kind);
        CHECK(embedded.has_value());
        if (!embedded) {
            continue;
        }

        // The drift check proper.
        const std::string on_disk = slurp(file);
        CHECK(on_disk == std::string(*embedded));

        // And the seed must yield a parseable manifest once filled. The
        // substitutions mirror cmd_create's; asset-bundle needs a real entry
        // because the schema requires minItems 1.
        std::string m(*embedded);
        m = replace_all(std::move(m), "{{id}}", "kit.example");
        m = replace_all(std::move(m), "{{version}}", "1.0.0");
        m = replace_all(std::move(m), "{{title}}", "Example");
        m = replace_all(std::move(m), "{{external_id}}", "example");
        m = replace_all(std::move(m), "{{entry_module}}", "main.lua");
        m = replace_all(std::move(m), "{{asset_entries}}",
                        "      { \"id\": \"asset.example\", \"path\": \"a.png\", "
                        "\"type\": \"texture\" }");

        auto parsed = Manifest::parse(m);
        CHECK(parsed.has_value());
        if (!parsed) {
            std::fprintf(stderr, "     seed '%.*s' does not parse: %s\n",
                         static_cast<int>(kind.size()), kind.data(),
                         parsed.error().message.c_str());
        }

        // No placeholder may survive substitution: an unreplaced {{...}}
        // means cmd_create and the seed disagree about the variable set.
        CHECK(m.find("{{") == std::string::npos);
    }

    // Every seed file on disk must be embedded, not just the reverse --
    // otherwise a new seed is invisible to `create`.
    for (const auto& e : fs::directory_iterator(seeds_dir)) {
        if (e.path().extension() != ".json") {
            continue;
        }
        const auto stem = e.path().stem().string();
        CHECK(app::seed_for(stem).has_value());
    }

    return test::report("test_seeds");
}
