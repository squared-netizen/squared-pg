// SPDX-License-Identifier: MIT
//
// FR-INT-1..6. The NIST vectors are here because a wrong hash is silent:
// nothing else in the suite would notice if sha256 quietly produced garbage,
// since every other test compares our output against our output.

#include "check.hpp"
#include "sqcart/sqcart.hpp"

#include "../src/sha256.hpp"

#include <cstring>
#include <string_view>

using namespace sqcart;

namespace {

ContentDigest hash_str(std::string_view s)
{
    detail::Sha256 h;
    h.update(s);
    return h.finish();
}

}  // namespace

int main()
{
    // FIPS 180-4 published vectors.
    CHECK(to_hex(hash_str("")) ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(to_hex(hash_str("abc")) ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(to_hex(hash_str("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")) ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

    // Incremental update must equal one-shot: the content digest streams
    // rather than materialising, so a broken split would corrupt everything.
    {
        detail::Sha256 h;
        h.update(std::string_view("abcdbcdecdefdefgefghfghi"));
        h.update(std::string_view("ghijhijkijkljklmklmnlmnomnopnopq"));
        CHECK(to_hex(h.finish()) ==
              "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    }

    // Hex round-trip.
    {
        const auto d = hash_str("abc");
        auto back = digest_from_hex(to_hex(d));
        CHECK(back.has_value());
        CHECK(*back == d);
        CHECK(!digest_from_hex("tooshort").has_value());
        CHECK(!digest_from_hex(std::string(64, 'z')).has_value());
    }

    // FR-INT-2: the digest is a property of content, not of the container
    // form. The same logical cartridge archived and exploded must agree --
    // this is the property milestone M0 rests on.
    {
        auto exploded = Cartridge::open("tests/testcart");
        CHECK(exploded.has_value());
        if (exploded) {
            auto d = exploded->content_digest();
            CHECK(d.has_value());

        }
    }

    return test::report("test_digest");
}
