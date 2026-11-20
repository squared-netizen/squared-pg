// SPDX-License-Identifier: MIT
//
// cartridge.hpp — the engine's one door to sq.cart (sqcart 3.0).
//
// A cartridge is opened here, packed (`.sq`) or exploded (a folder holding
// SQ-INF/manifest.json), and read through sq.cart only: its manifest by
// sq.cart's rules (format_version 3, an id, a version, a kind), its entries
// and their bytes. A 2.0 cartridge is refused: there is no 2.0 reading
// (sqcart's spec §13).
//
// sq.cart is C++23 modules; this header is plain C++, so the engine's other
// sources include it as any other. cartridge.cpp is the one source that
// imports sq.cart.

#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace squared::pg::cart {

/// SQ-INF/: container metadata, never payload.
inline constexpr std::string_view kMetaDir = "SQ-INF/";

struct Error {
    std::string message;
};

template <typename T>
using Expected = std::expected<T, Error>;

/// An entry: its path, size and flags (sq.cart's §4.2 flags; r is 1, x 4).
struct EntryInfo {
    std::string path;
    std::uint64_t size = 0;
    std::uint16_t flags = 0;
};

/// The envelope members the engine reads, and the consumers' sections as
/// JSON text. The format reads none of a consumer's section (§7).
class Manifest {
public:
    [[nodiscard]] const std::string& id() const noexcept { return id_; }
    [[nodiscard]] const std::string& version() const noexcept { return version_; }
    [[nodiscard]] const std::string& kind() const noexcept { return kind_; }
    /// The payload root: "." or a folder path ending in '/'.
    [[nodiscard]] const std::string& tree() const noexcept { return tree_; }
    [[nodiscard]] std::optional<std::string_view> title() const {
        return title_ ? std::optional<std::string_view>(*title_) : std::nullopt;
    }
    [[nodiscard]] std::optional<std::string_view> description() const {
        return description_ ? std::optional<std::string_view>(*description_) : std::nullopt;
    }
    [[nodiscard]] const std::vector<std::string>& requires_features() const noexcept { return requires_features_; }
    /// `consumers.<name>`, as JSON text; nothing when the manifest has none.
    [[nodiscard]] std::optional<std::string> consumer(std::string_view name) const;
    /// The manifest's text, as stored.
    [[nodiscard]] const std::string& raw_json() const noexcept { return json_; }

private:
    friend class Cartridge;
    std::string id_, version_, kind_, tree_ = ".", json_;
    std::optional<std::string> title_, description_;
    std::vector<std::string> requires_features_;
    std::vector<std::pair<std::string, std::string>> consumers_;
};

/// An opened cartridge. Move-only.
class Cartridge {
public:
    /// A `.sq` file or a folder, by what is at `location`.
    [[nodiscard]] static Expected<Cartridge> open(const std::filesystem::path& location);

    Cartridge(Cartridge&&) noexcept;
    Cartridge& operator=(Cartridge&&) noexcept;
    ~Cartridge();

    [[nodiscard]] const Manifest& manifest() const noexcept { return manifest_; }
    /// Every entry, sorted by path.
    [[nodiscard]] const std::vector<EntryInfo>& entries() const noexcept { return entries_; }
    [[nodiscard]] bool exploded() const noexcept { return exploded_; }
    /// An entry's bytes: a packed entry's checked against its digest.
    [[nodiscard]] Expected<std::vector<std::byte>> read(std::string_view path) const;

private:
    struct Impl;
    Cartridge();
    std::unique_ptr<Impl> impl_;
    Manifest manifest_;
    std::vector<EntryInfo> entries_;
    bool exploded_ = false;
};

}  // namespace squared::pg::cart
