#include "sha256.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace squared::sq::internal {
namespace {

constexpr std::array<std::uint32_t, 64> constants{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

constexpr std::uint32_t rotate_right(
    std::uint32_t value,
    unsigned count
) noexcept
{
    return (value >> count) | (value << (32U - count));
}

std::string hex(const std::array<std::byte, 32>& digest)
{
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const std::byte value : digest) {
        output << std::setw(2)
               << static_cast<unsigned>(std::to_integer<unsigned char>(value));
    }
    return output.str();
}

}  // namespace

Sha256::Sha256() noexcept
    : state_{
          0x6a09e667U,
          0xbb67ae85U,
          0x3c6ef372U,
          0xa54ff53aU,
          0x510e527fU,
          0x9b05688cU,
          0x1f83d9abU,
          0x5be0cd19U}
{
}

void Sha256::update(std::span<const std::byte> bytes) noexcept
{
    if (finished_ || bytes.empty()) return;
    total_bytes_ += bytes.size();

    std::size_t offset = 0;
    if (buffered_ != 0) {
        const std::size_t count =
            std::min(buffer_.size() - buffered_, bytes.size());
        std::copy_n(bytes.data(), count, buffer_.data() + buffered_);
        buffered_ += count;
        offset += count;
        if (buffered_ == buffer_.size()) {
            transform(buffer_.data());
            buffered_ = 0;
        }
    }

    while (bytes.size() - offset >= buffer_.size()) {
        transform(bytes.data() + offset);
        offset += buffer_.size();
    }

    if (offset < bytes.size()) {
        buffered_ = bytes.size() - offset;
        std::copy_n(bytes.data() + offset, buffered_, buffer_.data());
    }
}

void Sha256::update(std::string_view bytes) noexcept
{
    update(std::as_bytes(std::span(bytes.data(), bytes.size())));
}

void Sha256::transform(const std::byte* block) noexcept
{
    std::array<std::uint32_t, 64> words{};
    for (std::size_t index = 0; index < 16; ++index) {
        const std::size_t offset = index * 4;
        words[index] =
            (std::to_integer<std::uint32_t>(block[offset]) << 24U) |
            (std::to_integer<std::uint32_t>(block[offset + 1]) << 16U) |
            (std::to_integer<std::uint32_t>(block[offset + 2]) << 8U) |
            std::to_integer<std::uint32_t>(block[offset + 3]);
    }
    for (std::size_t index = 16; index < words.size(); ++index) {
        const std::uint32_t s0 =
            rotate_right(words[index - 15], 7) ^
            rotate_right(words[index - 15], 18) ^
            (words[index - 15] >> 3U);
        const std::uint32_t s1 =
            rotate_right(words[index - 2], 17) ^
            rotate_right(words[index - 2], 19) ^
            (words[index - 2] >> 10U);
        words[index] =
            words[index - 16] + s0 + words[index - 7] + s1;
    }

    std::uint32_t a = state_[0];
    std::uint32_t b = state_[1];
    std::uint32_t c = state_[2];
    std::uint32_t d = state_[3];
    std::uint32_t e = state_[4];
    std::uint32_t f = state_[5];
    std::uint32_t g = state_[6];
    std::uint32_t h = state_[7];

    for (std::size_t index = 0; index < words.size(); ++index) {
        const std::uint32_t sigma1 =
            rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
        const std::uint32_t choice = (e & f) ^ ((~e) & g);
        const std::uint32_t first =
            h + sigma1 + choice + constants[index] + words[index];
        const std::uint32_t sigma0 =
            rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
        const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t second = sigma0 + majority;

        h = g;
        g = f;
        f = e;
        e = d + first;
        d = c;
        c = b;
        b = a;
        a = first + second;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

std::array<std::byte, 32> Sha256::finish() noexcept
{
    if (!finished_) {
        const std::uint64_t bit_count = total_bytes_ * 8ULL;
        buffer_[buffered_++] = std::byte{0x80};

        if (buffered_ > 56) {
            std::fill(buffer_.begin() + buffered_, buffer_.end(), std::byte{0});
            transform(buffer_.data());
            buffered_ = 0;
        }

        std::fill(
            buffer_.begin() + buffered_,
            buffer_.begin() + 56,
            std::byte{0}
        );
        for (std::size_t index = 0; index < 8; ++index) {
            buffer_[63 - index] =
                std::byte((bit_count >> (index * 8U)) & 0xffU);
        }
        transform(buffer_.data());
        buffered_ = 0;
        finished_ = true;
    }

    std::array<std::byte, 32> digest{};
    for (std::size_t index = 0; index < state_.size(); ++index) {
        digest[index * 4] = std::byte((state_[index] >> 24U) & 0xffU);
        digest[index * 4 + 1] = std::byte((state_[index] >> 16U) & 0xffU);
        digest[index * 4 + 2] = std::byte((state_[index] >> 8U) & 0xffU);
        digest[index * 4 + 3] = std::byte(state_[index] & 0xffU);
    }
    return digest;
}

std::string Sha256::finish_hex() noexcept
{
    try {
        return hex(finish());
    } catch (...) {
        return {};
    }
}

std::string sha256_hex(std::string_view bytes) noexcept
{
    Sha256 digest;
    digest.update(bytes);
    return digest.finish_hex();
}

bool sha256_file(
    const std::filesystem::path& path,
    std::string& digest,
    std::string& message
) noexcept
{
    try {
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            message = "cannot open file for SHA-256";
            return false;
        }

        Sha256 hash;
        std::array<char, 128 * 1024> buffer{};
        while (input) {
            input.read(buffer.data(), buffer.size());
            const auto count = input.gcount();
            if (count > 0) {
                hash.update(std::string_view(
                    buffer.data(),
                    static_cast<std::size_t>(count)
                ));
            }
        }
        if (!input.eof()) {
            message = "cannot read file for SHA-256";
            return false;
        }
        digest = hash.finish_hex();
        return !digest.empty();
    } catch (const std::exception& exception) {
        message = exception.what();
        return false;
    } catch (...) {
        message = "unexpected SHA-256 failure";
        return false;
    }
}

}  // namespace squared::sq::internal
