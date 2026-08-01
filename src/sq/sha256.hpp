#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace squared::sq::internal {

/**
 * @brief Small streaming SHA-256 implementation owned by squared_sq_core.
 *
 * The implementation allocates no memory while digesting input.
 */
class Sha256 {
public:
    Sha256() noexcept;

    void update(std::span<const std::byte> bytes) noexcept;
    void update(std::string_view bytes) noexcept;

    [[nodiscard]] std::array<std::byte, 32> finish() noexcept;
    [[nodiscard]] std::string finish_hex() noexcept;

private:
    void transform(const std::byte* block) noexcept;

    std::array<std::uint32_t, 8> state_;
    std::array<std::byte, 64> buffer_{};
    std::uint64_t total_bytes_{0};
    std::size_t buffered_{0};
    bool finished_{false};
};

[[nodiscard]] std::string sha256_hex(std::string_view bytes) noexcept;
[[nodiscard]] bool sha256_file(
    const std::filesystem::path& path,
    std::string& digest,
    std::string& message
) noexcept;

}  // namespace squared::sq::internal
