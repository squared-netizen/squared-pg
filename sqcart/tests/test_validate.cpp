// SPDX-License-Identifier: MIT
//
// FR-VAL-1..5.

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

namespace fs = std::filesystem;

using namespace sqcart;

int main()
{
    auto c = Cartridge::open("tests/testcart");
    CHECK(c.has_value());
    if (!c) {
        return test::report("test_validate");
    }

    const ValidationReport r = validate(*c);

    // FR-VAL-2: conforming iff no error-severity diagnostic. Asserted as the
    // equivalence, not just "it passed", so a future change that sets
    // conforming independently of the diagnostics is caught.
    const bool any_error = std::any_of(r.diagnostics.begin(), r.diagnostics.end(),
                                       [](const Diagnostic& d) {
                                           return d.severity == Severity::error;
                                       });
    CHECK(r.conforming == !any_error);
    CHECK(r.conforming);

    // The fixture uses only reserved SQ-INF/ names, so a conforming cartridge
    // should produce no diagnostics at all -- not merely no errors.
    CHECK(r.diagnostics.empty());

    // FR-VAL-3: an unrecognised SQ-INF/ file is info, never error. Tested by
    // building a cartridge that has one, rather than by leaving stray files
    // in the shared fixture where they would mask a real regression.
    {
        const fs::path tmp = temp_root("test_validate") / "sqcart_validate_stray";
        fs::remove_all(tmp);
        fs::copy("tests/testcart", tmp, fs::copy_options::recursive);
        std::ofstream(tmp / "SQ-INF" / "notes.txt") << "not a reserved name\n";

        auto stray = Cartridge::open(tmp);
        CHECK(stray.has_value());
        if (stray) {
            const ValidationReport sr = validate(*stray);
            CHECK(sr.conforming);           // info does not make it non-conforming
            bool found = false;
            for (const auto& d : sr.diagnostics) {
                if (d.entry && *d.entry == "SQ-INF/notes.txt") {
                    found = true;
                    CHECK(d.severity == Severity::info);
                }
            }
            CHECK(found);
        }
        fs::remove_all(tmp);
    }

    // Every diagnostic naming an entry must name one that exists, or the
    // message is unactionable.
    for (const auto& d : r.diagnostics) {
        if (d.entry && d.message.find("missing") == std::string::npos) {
            CHECK(c->contains(*d.entry));
        }
    }

    return test::report("test_validate");
}
