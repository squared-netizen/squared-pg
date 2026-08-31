// SPDX-License-Identifier: MIT
//
// SHA-256 wrapper. The algorithm itself is the vendored public-domain
// implementation in third_party/crypto-algorithms (Brad Conte), chosen over
// hand-rolling because a wrong hash is silent and the vendored file ships
// with the NIST test vectors that test_digest.cpp re-checks.

#include "sha256.hpp"

#include <cstring>
#include <string_view>
#include <type_traits>

extern "C" {
#include <sha256.h>
}

namespace sqcart::detail {

static_assert(sizeof(SHA256_CTX) <= 128, "Sha256 inline context storage too small");
static_assert(std::is_trivially_copyable_v<SHA256_CTX>, "SHA256_CTX must be trivially copyable");

namespace {

SHA256_CTX* as_ctx(std::array<std::byte, 128>& storage)
{
    // The storage is raw bytes reinterpreted as the C context. This is the
    // one place the C type is named; nothing above this file sees it.
    return reinterpret_cast<SHA256_CTX*>(storage.data());
}

}  // namespace

Sha256::Sha256()
{
    sha256_init(as_ctx(ctx_));
}

void Sha256::update(std::span<const std::byte> bytes)
{
    if (bytes.empty()) {
        return;
    }
    sha256_update(as_ctx(ctx_), reinterpret_cast<const BYTE*>(bytes.data()), bytes.size());
}

void Sha256::update(std::string_view text)
{
    update(std::span<const std::byte>(reinterpret_cast<const std::byte*>(text.data()),
                                      text.size()));
}

std::array<std::byte, 32> Sha256::finish()
{
    std::array<std::byte, 32> out{};
    sha256_final(as_ctx(ctx_), reinterpret_cast<BYTE*>(out.data()));
    return out;
}

}  // namespace sqcart::detail
