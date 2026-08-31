// SPDX-License-Identifier: MIT
//
// Thin C++ wrapper over the vendored public-domain SHA-256 implementation.
// Confines the C API and its BYTE/WORD typedefs to one translation unit so
// no sqcart code handles raw unsigned char buffers directly.

#ifndef SQCART_SHA256_HPP
#define SQCART_SHA256_HPP

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace sqcart::detail {

/// Incremental SHA-256. Used both for whole-entry digests and for the
/// two-level content digest of format spec §9.2, which hashes a constructed
/// line stream rather than a contiguous buffer.
class Sha256 {
public:
    Sha256();
    void update(std::span<const std::byte> bytes);
    void update(std::string_view text);
    [[nodiscard]] std::array<std::byte, 32> finish();

private:
    // Sized to hold the vendored SHA256_CTX without including its header
    // here. Checked with a static_assert in sha256.cpp.
    alignas(8) std::array<std::byte, 128> ctx_{};
};

}  // namespace sqcart::detail

#endif  // SQCART_SHA256_HPP
