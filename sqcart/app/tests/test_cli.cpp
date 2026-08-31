// SPDX-License-Identifier: MIT
//
// FR-CLI-1..5.
//
// Every case here drives the real command functions over string streams. No
// subprocess is spawned, no shell is involved, no temp file is scraped for
// output. That is the whole return on the Environment seam: if these tests
// needed a terminal, they would not exist.

#include "check.hpp"
#include "sqcart/app/cli.hpp"
#include "sqcart/sqcart.hpp"

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

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

using namespace sqcart::app;
namespace fs = std::filesystem;

namespace {

struct Run {
    ExitCode    code;
    std::string out;
    std::string err;
};

Run invoke(std::vector<std::string> args)
{
    std::ostringstream out, err;
    std::istringstream in;
    Environment        env{out, err, in, fs::current_path(), false};
    const ExitCode     code = run(args, env);
    return Run{code, out.str(), err.str()};
}

std::string slurp(const fs::path& p)
{
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool contains(const std::string& haystack, std::string_view needle)
{
    return haystack.find(needle) != std::string::npos;
}

std::size_t count_lines(const std::string& s)
{
    return static_cast<std::size_t>(std::count(s.begin(), s.end(), '\n'));
}

}  // namespace

int main()
{
    const std::string kCart = "tests/testcart";

    // A missing fixture produces a cascade of unrelated-looking failures.
    // Say so once, plainly, and stop.
    if (!fs::is_regular_file(fs::path(kCart) / "SQ-INF" / "manifest.json")) {
        std::fprintf(stderr,
                     "FAIL fixture missing: %s/SQ-INF/manifest.json\n"
                     "     (run from the sqcart root; check tests/testcart/ exists)\n",
                     kCart.c_str());
        return 1;
    }

    // --- usage -----------------------------------------------------------

    {
        auto r = invoke({});
        CHECK(r.code == ExitCode::usage);
        CHECK(r.out.empty());              // usage goes to stderr when unsolicited
        CHECK(contains(r.err, "usage:"));
    }
    {
        auto r = invoke({"--help"});
        CHECK(r.code == ExitCode::ok);
        CHECK(contains(r.out, "usage:"));  // ...and to stdout when asked for
    }
    {
        auto r = invoke({"frobnicate", kCart});
        CHECK(r.code == ExitCode::usage);
        CHECK(contains(r.err, "unknown command"));
    }
    {
        auto r = invoke({"list"});         // missing required argument
        CHECK(r.code == ExitCode::usage);
    }
    {
        auto r = invoke({"--output"});     // option demanding a value it lacks
        CHECK(r.code == ExitCode::usage);
        CHECK(contains(r.err, "requires a value"));
    }

    // --- list: FR-CLI-3 machine-parseable, stdout clean ------------------

    {
        auto r = invoke({"list", kCart});
        CHECK(r.code == ExitCode::ok);
        CHECK(r.err.empty());              // no diagnostics leak onto a clean run
        // Count comes from the library, not a literal: a fixture gaining a
        // file should not break an assertion about output shape.
        auto cart = sqcart::Cartridge::open(kCart);
        CHECK(cart.has_value());
        if (cart) {
            CHECK(count_lines(r.out) == cart->entries().size());
        }
        CHECK(contains(r.out, "\tSQ-INF/manifest.json"));
        // Tab-separated with exactly four fields.
        if (!r.out.empty()) {
            const auto first = r.out.substr(0, r.out.find('\n'));
            CHECK(std::count(first.begin(), first.end(), '\t') == 3);
        }
    }
    {
        // -v adds a header on STDERR, so a pipe consuming stdout is unaffected.
        auto r = invoke({"list", "-v", kCart});
        CHECK(r.code == ExitCode::ok);
        CHECK(contains(r.err, "path"));
        auto cart2 = sqcart::Cartridge::open(kCart);
        if (cart2) {
            CHECK(count_lines(r.out) == cart2->entries().size());
        }
    }

    // --- show / info -----------------------------------------------------

    {
        auto r = invoke({"show", kCart});
        CHECK(r.code == ExitCode::ok);
        CHECK(contains(r.out, "\"kind\""));
        CHECK(contains(r.out, "kit.testcart"));
    }
    {
        auto r = invoke({"info", kCart});
        CHECK(r.code == ExitCode::ok);
        CHECK(contains(r.out, "kind"));
        CHECK(contains(r.out, "exploded"));
    }

    // --- cat: raw bytes to stdout ----------------------------------------

    {
        auto r = invoke({"cat", kCart, "bridge/src/bridge.cpp"});
        CHECK(r.code == ExitCode::ok);
        CHECK(!r.out.empty());
    }
    {
        auto r = invoke({"cat", kCart});   // entry argument missing
        CHECK(r.code == ExitCode::usage);
    }
    {
        auto r = invoke({"cat", kCart, "no/such/entry"});
        CHECK(r.code == ExitCode::failure);
        CHECK(contains(r.err, "cartridge.payload_missing"));
    }

    // --- digest ----------------------------------------------------------

    {
        auto r = invoke({"digest", kCart});
        CHECK(r.code == ExitCode::ok);
        CHECK(r.out.size() >= 66);
        // sha256sum-compatible: 64 hex, two spaces, name. Guarded, because an
        // unguarded substr turns a failed assertion into an abort that hides
        // every case after it.
        if (r.out.size() >= 66) {
            CHECK(r.out.substr(64, 2) == "  ");
        }
    }

    // --- verify: FR-CLI-4, exit 3 is reserved for non-conforming ---------

    {
        auto r = invoke({"verify", kCart});
        CHECK(r.code == ExitCode::ok);
        CHECK(r.out.empty());              // stdout reserved for future machine output
    }
    {
        auto r = invoke({"verify", "tests/definitely_not_here"});
        CHECK(r.code == ExitCode::failure);   // not 3: it never got to validate
    }

    // --- option spellings are equivalent ---------------------------------

    {
        const fs::path tmp = temp_root("test_cli") / "sqcart_cli_opts";
        fs::remove_all(tmp);
        auto a = invoke({"unpack", kCart, "-d", (tmp / "a").string()});
        auto b = invoke({"unpack", kCart, "--dest=" + (tmp / "b").string()});
        CHECK(a.code == ExitCode::ok);
        CHECK(b.code == ExitCode::ok);
        CHECK(fs::exists(tmp / "a" / "SQ-INF" / "manifest.json"));
        CHECK(fs::exists(tmp / "b" / "SQ-INF" / "manifest.json"));

        // Second extract without --overwrite must refuse.
        auto again = invoke({"unpack", kCart, "-d", (tmp / "a").string()});
        CHECK(again.code == ExitCode::failure);
        auto forced = invoke({"unpack", kCart, "-d", (tmp / "a").string(), "--overwrite"});
        CHECK(forced.code == ExitCode::ok);

        fs::remove_all(tmp);
    }

    // --- pack round-trip through the CLI ---------------------------------

    {
        const fs::path tmp = temp_root("test_cli") / "sqcart_cli_pack";
        fs::remove_all(tmp);
        fs::create_directories(tmp);
        const auto out = (tmp / "out.sq").string();

        auto p = invoke({"pack", kCart, "-o", out});
        CHECK(p.code == ExitCode::ok);

        // FR-WRITE-7: pack prints the digest of what it wrote, and it must
        // equal the digest the reader computes from the source tree.
        auto d_src = invoke({"digest", kCart});
        if (p.out.size() >= 64 && d_src.out.size() >= 64) {
            CHECK(p.out.substr(0, 64) == d_src.out.substr(0, 64));
        } else {
            CHECK(false);   // digest output too short to compare
        }

        auto d_packed = invoke({"digest", out});
        CHECK(d_packed.code == ExitCode::ok);
        if (d_packed.out.size() >= 64 && d_src.out.size() >= 64) {
            CHECK(d_packed.out.substr(0, 64) == d_src.out.substr(0, 64));
        }

        auto v = invoke({"verify", out});
        CHECK(v.code == ExitCode::ok);

        fs::remove_all(tmp);
    }
    {
        auto r = invoke({"pack", kCart});   // no -o
        CHECK(r.code == ExitCode::usage);
    }


    // --- create: the jar `cf` equivalent -------------------------------

    {
        // A plain folder that knows nothing about sqcart, with a name that is
        // not a legal identifier and files whose stems are not either.
        const fs::path root = temp_root("test_cli") / "sqcart_cli_create";
        fs::remove_all(root);
        const fs::path plain = root / "My Game Assets";
        fs::create_directories(plain / "textures");
        std::ofstream(plain / "textures" / "enemy-01.png") << "px";
        std::ofstream(plain / "settings.json") << "{}";

        const auto out = (root / "assets.sq").string();
        auto c = invoke({"create", plain.string(), "-o", out});
        CHECK(c.code == ExitCode::ok);
        CHECK(fs::is_regular_file(plain / "SQ-INF" / "manifest.json"));

        // What it wrote must open, and the derived id must be spec-legal:
        // lowercase dotted segments, kind-prefixed (§5.2).
        auto opened = sqcart::Cartridge::open(plain);
        CHECK(opened.has_value());
        if (opened) {
            CHECK(opened->manifest().id() == "asset.my_game_assets");
            CHECK(opened->manifest().kind() == sqcart::Kind::asset_bundle);
            auto assets = opened->manifest().as_assets();
            CHECK(assets.has_value());
            if (assets) {
                CHECK(assets->get().entries.size() == 2);
            }
        }

        // And it must be conforming, not merely parseable.
        auto v = invoke({"verify", plain.string()});
        CHECK(v.code == ExitCode::ok);

        // Running create twice must not silently reseed a manifest someone
        // may have hand-edited.
        auto again = invoke({"create", plain.string(), "-o", out});
        CHECK(again.code == ExitCode::failure);
        CHECK(contains(again.err, "already has"));

        // --force is the escape hatch.
        auto forced = invoke({"create", plain.string(), "--force", "--in-place"});
        CHECK(forced.code == ExitCode::ok);

        // Determinism: the same folder seeded twice yields identical bytes,
        // so a created manifest is a function of the tree and the flags only.
        const auto first = slurp(plain / "SQ-INF" / "manifest.json");
        auto again2 = invoke({"create", plain.string(), "--force", "--in-place"});
        CHECK(again2.code == ExitCode::ok);
        CHECK(slurp(plain / "SQ-INF" / "manifest.json") == first);

        fs::remove_all(root);
    }

    {
        // Kind is auto-detected when an entry module is present.
        const fs::path root = temp_root("test_cli") / "sqcart_cli_create_game";
        fs::remove_all(root);
        fs::create_directories(root / "mygame" / "game");
        std::ofstream(root / "mygame" / "game" / "main.lua") << "print(1)\n";

        auto c = invoke({"create", (root / "mygame").string(), "--in-place"});
        CHECK(c.code == ExitCode::ok);

        auto opened = sqcart::Cartridge::open(root / "mygame");
        CHECK(opened.has_value());
        if (opened) {
            CHECK(opened->manifest().kind() == sqcart::Kind::cartridge);
            auto body = opened->manifest().as_cartridge();
            CHECK(body.has_value());
            if (body) {
                CHECK(body->get().entry.module == "game/main.lua");
            }
        }
        fs::remove_all(root);
    }

    {
        // An unknown kind is a usage error naming the known ones, and an
        // empty directory has nothing to package.
        const fs::path root = temp_root("test_cli") / "sqcart_cli_create_bad";
        fs::remove_all(root);
        fs::create_directories(root / "empty");

        auto e = invoke({"create", (root / "empty").string(), "--in-place"});
        CHECK(e.code == ExitCode::failure);
        CHECK(contains(e.err, "no files"));

        std::ofstream(root / "empty" / "a.txt") << "a";
        auto k = invoke({"create", (root / "empty").string(), "--kind", "nonsense",
                         "--in-place"});
        CHECK(k.code == ExitCode::usage);
        CHECK(contains(k.err, "asset-bundle"));

        auto n = invoke({"create"});
        CHECK(n.code == ExitCode::usage);

        fs::remove_all(root);
    }

    return sqcart::test::report("test_cli");
}
