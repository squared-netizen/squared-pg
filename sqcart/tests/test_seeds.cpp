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

    // One seed as of format 2, not six. The per-kind seeds each carried that
    // kind's body, which meant this tool shipped the schema of every role in
    // the ecosystem in string literals -- the coupling §0.1 forbids, hiding
    // in the CLI rather than the library.
    const auto kinds = app::seed_kinds();
    CHECK(kinds.size() == 1);
    if (kinds.empty()) {
        return test::report("test_seeds");
    }
    CHECK(kinds[0] == "envelope");

    const fs::path file = seeds_dir / "envelope.json";
    CHECK(fs::is_regular_file(file));

    auto embedded = app::seed_for("envelope");
    CHECK(embedded.has_value());

    if (fs::is_regular_file(file) && embedded) {
        // The drift check proper: the canonical file and the embedded literal
        // must be byte-identical, because the literal is what ships and the
        // file is what gets edited.
        CHECK(slurp(file) == std::string(*embedded));

        // Filled, it must parse. The substitutions mirror cmd_create's, which
        // are now four rather than six -- {{external_id}} and
        // {{entry_module}} went with the kind bodies that needed them.
        std::string m(*embedded);
        m = replace_all(std::move(m), "{{kind}}", "kit");
        m = replace_all(std::move(m), "{{id}}", "kit.example");
        m = replace_all(std::move(m), "{{version}}", "1.0.0");
        m = replace_all(std::move(m), "{{title}}", "Example");

        auto parsed = Manifest::parse(m);
        CHECK(parsed.has_value());
        if (parsed) {
            CHECK(parsed->kind() == "kit");
            CHECK(parsed->tree() == ".");
            CHECK(parsed->consumers().empty());
        } else {
            std::fprintf(stderr, "     envelope seed does not parse: %s\n",
                         parsed.error().message.c_str());
        }

        // No placeholder may survive: an unreplaced {{...}} means cmd_create
        // and the seed disagree about the variable set.
        CHECK(m.find("{{") == std::string::npos);

        // The seed serves any kind, which is the whole reason there is one of
        // it. A role this library has never heard of must fill it just as
        // well as a role it ships fixtures for.
        std::string other(*embedded);
        other = replace_all(std::move(other), "{{kind}}", "holodisk_volume");
        other = replace_all(std::move(other), "{{id}}", "local.thing");
        other = replace_all(std::move(other), "{{version}}", "1.0.0");
        other = replace_all(std::move(other), "{{title}}", "Thing");
        auto parsed_other = Manifest::parse(other);
        CHECK(parsed_other.has_value());
        if (parsed_other) CHECK(parsed_other->kind() == "holodisk_volume");
    }

    // Every seed file on disk must be embedded. With one seed this is a thin
    // check, but it is the one that catches a stale file left behind by the
    // six-to-one collapse.
    int json_files = 0;
    for (const auto& e : fs::directory_iterator(seeds_dir)) {
        if (e.path().extension() != ".json") {
            continue;
        }
        ++json_files;
        CHECK(e.path().stem().string() == "envelope");
    }
    CHECK(json_files == 1);

    return test::report("test_seeds");
}
