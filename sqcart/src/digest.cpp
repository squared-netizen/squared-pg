// SPDX-License-Identifier: MIT
//
// Integrity: content digest, declared digests, verification. Format spec §9.
//
// The load-bearing property here is FR-INT-2 / FR-WRITE-7: the digest is
// computed over *content and paths*, never over archive bytes. Two cartridges
// with identical payloads but different deflate settings therefore produce
// the same digest, which is what makes milestone M0's cross-binary
// reproducibility check possible at all. Reader and writer call this one
// implementation; there is deliberately not a second copy in writer.cpp.

#include "impl.hpp"
#include "sha256.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

extern "C" {
#include <yyjson.h>
}

namespace sqcart {

ContentDigest sha256(std::span<const std::byte> bytes)
{
    detail::Sha256 h;
    h.update(bytes);
    return h.finish();
}

std::string to_hex(const ContentDigest& digest)
{
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out;
    out.reserve(digest.size() * 2);
    for (std::byte b : digest) {
        const auto v = static_cast<unsigned>(b);
        out.push_back(kDigits[(v >> 4) & 0xF]);
        out.push_back(kDigits[v & 0xF]);
    }
    return out;
}

std::optional<ContentDigest> digest_from_hex(std::string_view hex)
{
    if (hex.size() != 64) {
        return std::nullopt;
    }
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    ContentDigest out{};
    for (std::size_t i = 0; i < 32; ++i) {
        const int hi = nibble(hex[i * 2]);
        const int lo = nibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            return std::nullopt;
        }
        out[i] = static_cast<std::byte>((hi << 4) | lo);
    }
    return out;
}

bool participates_in_digest(std::string_view path) noexcept
{
    // §9.2 step 1: everything except the two files that cannot contain their
    // own digest.
    if (path == "SQ-INF/hashes.json") {
        return false;
    }
    return !path.starts_with("SQ-INF/signatures/");
}

void DigestAccumulator::add(std::string_view path, std::span<const std::byte> content)
{
    if (!participates_in_digest(path)) {
        return;
    }
    entries_.emplace_back(std::string(path), sha256(content));
}

ContentDigest DigestAccumulator::finish()
{
    // §9.2 step 2: sort byte-wise ascending over the UTF-8 encoding. Imposed
    // here rather than relying on caller order, because the entry index order
    // is form-dependent (FR-ENTRY-1) and archived/exploded forms of one
    // cartridge must still agree.
    std::sort(entries_.begin(), entries_.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    detail::Sha256 outer;
    for (const auto& [path, digest] : entries_) {
        outer.update(path);
        outer.update(std::string_view("\n"));
        outer.update(to_hex(digest));
        outer.update(std::string_view("\n"));
    }
    return outer.finish();
}

Result<ContentDigest> Cartridge::content_digest() const
{
    const auto& st = impl_->st;

    DigestAccumulator acc;
    for (const auto& e : st.entries) {
        if (!participates_in_digest(e.path)) {
            continue;   // Skip the read entirely; cheaper than hashing to discard.
        }
        auto data = read_entry(st, e);
        if (!data) {
            return unexpected(data.error());
        }
        acc.add(e.path, *data);
    }
    return acc.finish();
}

Result<EntryDigestMap> Cartridge::declared_digests() const
{
    const auto& st = impl_->st;

    auto found = find_entry(st, "SQ-INF/hashes.json");
    if (!found) {
        // FR-INT-3: absence is not a fault. An empty map means "nothing was
        // declared", which is distinct from "the declaration failed to parse".
        return EntryDigestMap{};
    }

    auto data = read_entry(st, found->get());
    if (!data) {
        return unexpected(data.error());
    }

    yyjson_doc* doc = yyjson_read(reinterpret_cast<const char*>(data->data()), data->size(), 0);
    if (doc == nullptr) {
        return unexpected(Error{ErrorCode::manifest_malformed, "SQ-INF/hashes.json is not valid JSON",
                                std::string("SQ-INF/hashes.json"), std::nullopt, false});
    }
    struct DocGuard {
        yyjson_doc* d;
        ~DocGuard() { yyjson_doc_free(d); }
    } guard{doc};

    yyjson_val* root = yyjson_doc_get_root(doc);
    if (root == nullptr || !yyjson_is_obj(root)) {
        return unexpected(Error{ErrorCode::manifest_malformed, "hashes.json root is not an object",
                                std::string("SQ-INF/hashes.json"), std::nullopt, false});
    }

    yyjson_val* algo = yyjson_obj_get(root, "algorithm");
    if (algo != nullptr && yyjson_is_str(algo)) {
        const std::string_view name(yyjson_get_str(algo), yyjson_get_len(algo));
        if (name != "sha256") {
            return unexpected(Error{ErrorCode::feature_unsupported,
                                    "hashes.json declares an unsupported digest algorithm",
                                    std::string("SQ-INF/hashes.json"), std::nullopt, true});
        }
    }

    EntryDigestMap out;
    yyjson_val* entries = yyjson_obj_get(root, "entries");
    if (entries != nullptr && yyjson_is_obj(entries)) {
        yyjson_obj_iter iter;
        yyjson_obj_iter_init(entries, &iter);
        yyjson_val* key = nullptr;
        while ((key = yyjson_obj_iter_next(&iter)) != nullptr) {
            yyjson_val* val = yyjson_obj_iter_get_val(key);
            if (!yyjson_is_str(key) || val == nullptr || !yyjson_is_str(val)) {
                return unexpected(Error{ErrorCode::manifest_malformed,
                                        "hashes.json entry is not a string pair",
                                        std::string("SQ-INF/hashes.json"), std::nullopt, false});
            }
            const std::string path(yyjson_get_str(key), yyjson_get_len(key));
            const std::string_view hex(yyjson_get_str(val), yyjson_get_len(val));
            auto digest = digest_from_hex(hex);
            if (!digest) {
                return unexpected(Error{ErrorCode::manifest_malformed,
                                        "hashes.json contains a malformed digest", path,
                                        std::nullopt, false});
            }
            out.emplace(path, *digest);
        }
    }
    return out;
}

Result<void> Cartridge::verify_integrity() const
{
    auto declared = declared_digests();
    if (!declared) {
        return unexpected(declared.error());
    }

    // FR-INT-5: vacuous success when nothing is declared. See functional spec
    // open issue 1 — a caller needing mandatory integrity must check
    // declared_digests() for non-emptiness itself, because sqcart does not
    // invent a requirement the format does not impose.
    if (declared->empty()) {
        return {};
    }

    const auto& st = impl_->st;
    for (const auto& [path, expected_digest] : *declared) {
        auto found = find_entry(st, path);
        if (!found) {
            return unexpected(Error{ErrorCode::integrity_failed,
                                    "hashes.json names an entry absent from the cartridge", path,
                                    st.manifest->id(), false});
        }
        auto data = read_entry(st, found->get());
        if (!data) {
            return unexpected(data.error());
        }
        if (sha256(*data) != expected_digest) {
            // FR-INT-4: name the first mismatching entry.
            return unexpected(Error{ErrorCode::integrity_failed, "entry digest mismatch", path,
                                    st.manifest->id(), false});
        }
    }
    return {};
}

}  // namespace sqcart
