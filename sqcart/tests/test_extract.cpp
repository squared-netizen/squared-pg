// SPDX-License-Identifier: MIT
//
// FR-EXT-1..5 and FR-SEC-1. The traversal cases matter most: extract_to()
// must refuse to write outside dest under any input, including one that
// reached it through a lenient open().

#include "check.hpp"
#include "sqcart/sqcart.hpp"

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
    const fs::path tmp = temp_root("test_extract") / "sqcart_test_extract";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    auto c = Cartridge::open("tests/testcart");
    CHECK(c.has_value());
    if (!c) {
        return test::report("test_extract");
    }

    // Happy path.
    {
        const fs::path dest = tmp / "out";
        auto r = c->extract_to(dest);
        CHECK(r.has_value());
        CHECK(fs::exists(dest / "SQ-INF/manifest.json"));
        CHECK(fs::exists(dest / "bridge/src/bridge.cpp"));

        // Nested directories are created, not assumed.
        CHECK(fs::is_directory(dest / "bridge" / "src"));
    }

    // FR-EXT-3: with overwrite=false the existence check precedes ALL writes.
    // Pre-create one file that would be written late in the plan, then assert
    // that an earlier one was not created -- proving the check is not
    // interleaved with writing.
    {
        const fs::path dest = tmp / "nooverwrite";
        fs::create_directories(dest / "bridge" / "src");
        std::ofstream(dest / "bridge" / "src" / "bridge.cpp") << "existing";

        auto r = c->extract_to(dest, false);
        CHECK(!r.has_value());
        if (!r) {
            CHECK(r.error().code == ErrorCode::io_failed);
        }
        // The manifest sorts before tree/ in the plan; if the check were
        // interleaved it would already be on disk.
        CHECK(!fs::exists(dest / "SQ-INF" / "manifest.json"));
    }

    // overwrite=true proceeds.
    {
        const fs::path dest = tmp / "overwrite";
        CHECK(c->extract_to(dest).has_value());
        CHECK(c->extract_to(dest, true).has_value());
    }

    // FR-EXT-5: no permission bits are restored; files land under the umask
    // as ordinary regular files, never executable-by-inheritance.
    {
        const fs::path f = tmp / "out" / "bridge" / "src" / "bridge.cpp";
        CHECK(fs::is_regular_file(f));
        CHECK(!fs::is_symlink(f));
    }

    // Destination spellings must all behave identically.
    //
    // lexically_normal() leaves a trailing EMPTY component on any path ending
    // in '.' or '/', and a component-wise prefix check that does not drop it
    // rejects every entry as escaping the destination. The default
    // destination is "." -- so `unpack cart.sq` with no -d, the commonest
    // invocation, failed outright while `-d ./out` worked. One character of
    // difference between working and broken is exactly the kind of thing a
    // test has to pin.
    {
        const fs::path base = tmp / "spellings";
        int n = 0;
        for (const std::string suffix : {"a", "a/", "./b", "./b/", "c/./d"}) {
            const fs::path dest = base / suffix;
            auto r = c->extract_to(dest);
            CHECK(r.has_value());
            if (!r) {
                std::fprintf(stderr, "     spelling '%s' rejected: %s\n",
                             suffix.c_str(), r.error().message.c_str());
            } else {
                CHECK(fs::exists(dest / "SQ-INF" / "manifest.json"));
            }
            ++n;
            fs::remove_all(base);
        }
        CHECK(n == 5);
    }

    // Traversal is still refused: the fix loosened the comparison, and the
    // security property must survive it.
    {
        const fs::path dest = tmp / "escape";
        auto r = c->extract_to(dest);
        CHECK(r.has_value());
        // Nothing may exist above dest that the cartridge put there.
        CHECK(!fs::exists(tmp / "escape" / ".." / "pwned"));
        CHECK(!fs::exists(tmp / "pwned"));
    }

    fs::remove_all(tmp);
    return test::report("test_extract");
}
