// SPDX-License-Identifier: MIT
//
// Manifest conformance, format spec §5 and the schema.
//
// The fixture is the positive case. These are the negative ones: each takes
// the conforming manifest and breaks exactly one rule, so a failure names the
// rule rather than "something about manifests".
//
// This file exists because the first draft accepted a fixture violating three
// separate MUSTs -- a single-segment id, empty integration_areas, and a
// missing "format" -- and nothing noticed. A parser is only as strict as its
// rejection tests.

#include "check.hpp"
#include "sqcart/sqcart.hpp"

#include <cstdio>
#include <filesystem>
#include <system_error>
#include <fstream>
#include <sstream>
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

std::string slurp(const fs::path& p)
{
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

/// Replace the first occurrence of `from` with `to`. Asserts the edit landed,
/// so a fixture change that invalidates a test is a failure rather than a
/// silently vacuous pass.
std::string edited(const std::string& src, std::string_view from, std::string_view to)
{
    const auto at = src.find(from);
    if (at == std::string::npos) {
        return {};
    }
    return src.substr(0, at) + std::string(to) + src.substr(at + from.size());
}

}  // namespace

int main()
{
    const std::string good = slurp("tests/testcart/SQ-INF/manifest.json");
    CHECK(!good.empty());
    if (good.empty()) {
        // Native code is contained freely and loaded never.
    //
    // The first draft refused to OPEN a cartridge containing .so/.o, which
    // defeats the format: kits ship bridge code and packages ship libraries.
    // Containment is an archive property; loading is a runtime decision under
    // the trust model of §10. These assert the split.
    {
        const fs::path tmp = temp_root("test_conformance") / "sqcart_native_test";
        fs::remove_all(tmp);
        fs::create_directories(tmp / "SQ-INF");
        fs::create_directories(tmp / "lib");
        std::ofstream(tmp / "lib" / "libthing.so") << "\x7f" "ELF";   // split: \x7fE parses as one out-of-range escape
        std::ofstream(tmp / "lib" / "thing.o") << "obj";
        std::ofstream(tmp / "SQ-INF" / "manifest.json") << R"({
  "format": "squared-cartridge",
  "format_version": 1,
  "kind": "asset-bundle",
  "id": "asset.native",
  "version": "1.0.0",
  "assets": { "entries": [
    { "id": "asset.lib.libthing", "path": "lib/libthing.so", "type": "data" },
    { "id": "asset.lib.thing", "path": "lib/thing.o", "type": "data" }
  ] }
})";

        // Opens by default.
        auto c = Cartridge::open(tmp);
        CHECK(c.has_value());
        if (c) {
            const ValidationReport r = validate(*c);
            CHECK(r.conforming);          // native code does not make it non-conforming
            int warned = 0;
            for (const auto& d : r.diagnostics) {
                if (d.severity == Severity::warning &&
                    d.message.find("native code") != std::string::npos) {
                    ++warned;
                }
            }
            CHECK(warned == 2);           // ...but it is never invisible
        }

        // A caller with a real policy can still refuse.
        OpenOptions strict_native;
        strict_native.reject_native_code = true;
        auto refused = Cartridge::open(tmp, strict_native);
        CHECK(!refused.has_value());
        if (!refused) {
            CHECK(refused.error().code == ErrorCode::native_code_prohibited);
        }

        fs::remove_all(tmp);
    }

    return test::report("test_conformance");
    }

    // Positive control: the fixture parses.
    {
        auto m = Manifest::parse(good);
        CHECK(m.has_value());
        if (m) {
            CHECK(m->id() == "kit.testcart");
            CHECK(m->kind() == Kind::kit);
            CHECK(m->format_version() == kFormatVersion);

            // The schema's nested requires block must actually reach the
            // model. The first draft read flat siblings and silently dropped
            // these, which is the worst outcome for a resolver input.
            auto kit = m->as_kit();
            CHECK(kit.has_value());
            if (kit) {
                CHECK(kit->get().required_packages.size() == 1);
                CHECK(kit->get().framework_range.has_value());
                CHECK(kit->get().integration_areas.size() == 2);
            }
        }
    }

    // FR-KIND-2: integration_areas must be non-empty. It is the basis of the
    // engine's pre-mutation kit conflict check (§2.7.4).
    {
        const auto broken = edited(good,
            "\"build.cmake.targets\",\n      \"app.entrypoint\"\n    ]", "]");
        CHECK(!broken.empty());
        auto m = Manifest::parse(broken);
        CHECK(!m.has_value());
    }

    // FR-MAN-5: a foreign kind body is rejected even when well formed on its
    // own, so a resource cannot masquerade as two roles.
    {
        const auto broken = edited(good, "\"kit\": {", "\"package\": {");
        CHECK(!broken.empty());
        auto m = Manifest::parse(broken);
        CHECK(!m.has_value());
        if (!m) {
            CHECK(m.error().code == ErrorCode::kind_invalid ||
                  m.error().code == ErrorCode::manifest_malformed);
        }
    }

    // FR-MAN-1: UTF-8 without a BOM.
    {
        auto m = Manifest::parse(std::string("\xEF\xBB\xBF") + good);
        CHECK(!m.has_value());
    }

    // Malformed JSON is manifest_malformed, not a crash.
    {
        auto m = Manifest::parse("{ \"format\": ");
        CHECK(!m.has_value());
        if (!m) {
            CHECK(m.error().code == ErrorCode::manifest_malformed);
        }
    }

    // An empty document is not a manifest.
    {
        auto m = Manifest::parse("");
        CHECK(!m.has_value());
    }

    // FR-MAN-7: unrecognised members are ignored, not rejected, and survive
    // in raw_json for consumers modelling fields this version does not.
    {
        const auto extended = edited(good, "\"kind\": \"kit\",",
                                     "\"kind\": \"kit\",\n  \"x_future_field\": 42,");
        CHECK(!extended.empty());
        auto m = Manifest::parse(extended);
        CHECK(m.has_value());
        if (m) {
            CHECK(m->raw_json().find("x_future_field") != std::string_view::npos);
        }
    }

    // Native code is contained freely and loaded never.
    //
    // The first draft refused to OPEN a cartridge containing .so/.o, which
    // defeats the format: kits ship bridge code and packages ship libraries.
    // Containment is an archive property; loading is a runtime decision under
    // the trust model of §10. These assert the split.
    {
        const fs::path tmp = temp_root("test_conformance") / "sqcart_native_test";
        fs::remove_all(tmp);
        fs::create_directories(tmp / "SQ-INF");
        fs::create_directories(tmp / "lib");
        std::ofstream(tmp / "lib" / "libthing.so") << "\x7f" "ELF";   // split: \x7fE parses as one out-of-range escape
        std::ofstream(tmp / "lib" / "thing.o") << "obj";
        std::ofstream(tmp / "SQ-INF" / "manifest.json") << R"({
  "format": "squared-cartridge",
  "format_version": 1,
  "kind": "asset-bundle",
  "id": "asset.native",
  "version": "1.0.0",
  "assets": { "entries": [
    { "id": "asset.lib.libthing", "path": "lib/libthing.so", "type": "data" },
    { "id": "asset.lib.thing", "path": "lib/thing.o", "type": "data" }
  ] }
})";

        // Opens by default.
        auto c = Cartridge::open(tmp);
        CHECK(c.has_value());
        if (c) {
            const ValidationReport r = validate(*c);
            CHECK(r.conforming);          // native code does not make it non-conforming
            int warned = 0;
            for (const auto& d : r.diagnostics) {
                if (d.severity == Severity::warning &&
                    d.message.find("native code") != std::string::npos) {
                    ++warned;
                }
            }
            CHECK(warned == 2);           // ...but it is never invisible
        }

        // A caller with a real policy can still refuse.
        OpenOptions strict_native;
        strict_native.reject_native_code = true;
        auto refused = Cartridge::open(tmp, strict_native);
        CHECK(!refused.has_value());
        if (!refused) {
            CHECK(refused.error().code == ErrorCode::native_code_prohibited);
        }

        fs::remove_all(tmp);
    }

    return test::report("test_conformance");
}
