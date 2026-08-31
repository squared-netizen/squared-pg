// SPDX-License-Identifier: MIT
//
// Symbolic link policy.
//
// The first implementation refused outright on any link, which made the tool
// unusable on a normal terminal workspace -- a built binary linked back into
// its source directory is enough to stop it. These tests pin the three
// behaviours that replaced it, and in particular the one rule that does not
// bend: a link resolving outside the cartridge root is never packed, whatever
// the policy says.

#include "check.hpp"
#include "sqcart/sqcart.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <system_error>
#include <fstream>
#include <string>

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

namespace {

/// Build a workspace shaped like a real one: a cartridge directory with a
/// self-link, a link escaping the root, a link to a secret, and a dangling
/// link.
fs::path make_workspace()
{
    const fs::path root = temp_root("test_symlinks") / "sqcart_symlink_test";
    fs::remove_all(root);
    fs::create_directories(root / "cart" / "src");
    fs::create_directories(root / "outside");

    std::ofstream(root / "cart" / "SQ-INF" / ".keep");   // fails; dir made below
    fs::create_directories(root / "cart" / "SQ-INF");

    std::ofstream(root / "cart" / "src" / "real.txt") << "payload\n";
    std::ofstream(root / "outside" / "secret.txt") << "SHOULD-NEVER-BE-PACKED\n";

    std::ofstream(root / "cart" / "SQ-INF" / "manifest.json") << R"({
  "format": "squared-cartridge",
  "format_version": 1,
  "kind": "asset-bundle",
  "id": "asset.linktest",
  "version": "1.0.0",
  "assets": {
    "entries": [
      { "id": "asset.real", "path": "src/real.txt", "type": "data" }
    ]
  }
})";

    std::error_code ec;
    fs::create_symlink(root / "cart" / "src" / "real.txt", root / "cart" / "selflink", ec);
    fs::create_symlink(root / "outside" / "secret.txt", root / "cart" / "leak", ec);
    fs::create_symlink(root / "nowhere", root / "cart" / "dangling", ec);
    return root;
}

bool has_entry(const Cartridge& c, std::string_view path)
{
    const auto e = c.entries();
    return std::any_of(e.begin(), e.end(),
                       [&](const EntryInfo& i) { return i.path == path; });
}

bool skipped(const Cartridge& c, std::string_view path)
{
    const auto s = c.skipped_symlinks();
    return std::find(s.begin(), s.end(), path) != s.end();
}

}  // namespace

int main()
{
    const fs::path root = make_workspace();
    const fs::path cart = root / "cart";

    // --- default: skip, but never silently -------------------------------
    {
        auto c = Cartridge::open(cart);
        CHECK(c.has_value());
        if (c) {
            CHECK(has_entry(*c, "src/real.txt"));

            // Every link omitted, and each one recorded by its own path --
            // not by its target, which is what a canonicalising relative()
            // would have produced.
            CHECK(!has_entry(*c, "selflink"));
            CHECK(!has_entry(*c, "leak"));
            CHECK(!has_entry(*c, "dangling"));
            CHECK(c->skipped_symlinks().size() == 3);
            CHECK(skipped(*c, "selflink"));
            CHECK(skipped(*c, "leak"));
            CHECK(skipped(*c, "dangling"));

            // validate() surfaces them as warnings: structurally fine, but a
            // human should know files present on disk are absent here.
            const ValidationReport r = validate(*c);
            CHECK(r.conforming);              // warnings do not fail a cartridge
            int warnings = 0;
            for (const auto& d : r.diagnostics) {
                if (d.severity == Severity::warning &&
                    d.message.find("symbolic link") != std::string::npos) {
                    ++warnings;
                }
            }
            CHECK(warnings == 3);
        }
    }

    // --- follow_internal: packs the safe one, refuses the escaping one ---
    {
        OpenOptions o;
        o.symlinks = SymlinkPolicy::follow_internal;
        auto c = Cartridge::open(cart, o);
        CHECK(c.has_value());
        if (c) {
            // The self-link resolves inside the root, so its bytes are real
            // payload already present in the cartridge.
            CHECK(has_entry(*c, "selflink"));

            // This is the rule that does not bend. Following `leak` would
            // copy bytes the author never put in the directory.
            CHECK(!has_entry(*c, "leak"));
            CHECK(skipped(*c, "leak"));
            CHECK(skipped(*c, "dangling"));

            // Belt and braces: prove the secret is not reachable through any
            // entry, not merely that the entry name is absent.
            for (const auto& e : c->entries()) {
                auto data = c->read(e.path);
                if (data) {
                    const std::string_view text(
                        reinterpret_cast<const char*>(data->data()), data->size());
                    CHECK(text.find("SHOULD-NEVER-BE-PACKED") == std::string_view::npos);
                }
            }
        }
    }

    // --- reject: fails on the first link ---------------------------------
    {
        OpenOptions o;
        o.symlinks = SymlinkPolicy::reject;
        auto c = Cartridge::open(cart, o);
        CHECK(!c.has_value());
        if (!c) {
            CHECK(c.error().code == ErrorCode::path_unsafe);
            CHECK(c.error().entry.has_value());
        }
    }

    // --- a tree with no links at all is unaffected -----------------------
    {
        auto c = Cartridge::open("tests/testcart");
        CHECK(c.has_value());
        if (c) {
            CHECK(c->skipped_symlinks().empty());
            CHECK(validate(*c).diagnostics.empty());
        }
    }

    fs::remove_all(root);
    return test::report("test_symlinks");
}
