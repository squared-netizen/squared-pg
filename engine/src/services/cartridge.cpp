// SPDX-License-Identifier: MIT
//
// cartridge.cpp — cartridge.hpp over sq.cart: the engine's one source that
// imports a module. Headers first, then the modules (the order clang asks
// for when a source does both).

#include "services/cartridge.hpp"

#include <system_error>
#include <utility>
#include <variant>

import sq.data;
import sq.cart;

namespace squared::pg::cart {

namespace {

[[nodiscard]] Error from(const sq::cart::Error& e) {
    std::string message(e.message);
    if (e.entry) message += " (" + *e.entry + ")";
    return Error{std::move(message)};
}

/// `tree` as the format allows it (2.0 §5.1, kept): "." or a relative
/// folder path ending in '/', never into SQ-INF/, no empty, '.' or '..'
/// segment.
[[nodiscard]] bool valid_tree(std::string_view tree) {
    if (tree == ".") return true;
    if (tree.empty() || tree.size() > 1024 || tree.front() == '/' || tree.back() != '/') return false;
    if (tree.starts_with(kMetaDir)) return false;
    for (std::size_t start = 0; start < tree.size();) {
        const std::size_t slash = tree.find('/', start);
        const std::string_view segment = tree.substr(start, slash - start);
        if (segment.empty() || segment == "." || segment == "..") return false;
        start = slash + 1;
    }
    return true;
}

[[nodiscard]] std::optional<std::string> string_member(const sq::data::JsonValue& o, std::string_view name) {
    const sq::data::JsonValue* v = o.find(name);
    if (v == nullptr) return std::nullopt;
    if (const std::string* s = v->string_if()) return *s;
    return std::nullopt;
}

}  // namespace

struct Cartridge::Impl {
    std::variant<sq::cart::Archive, sq::cart::Folder> source;
};

Cartridge::Cartridge() = default;
Cartridge::Cartridge(Cartridge&&) noexcept = default;
Cartridge& Cartridge::operator=(Cartridge&&) noexcept = default;
Cartridge::~Cartridge() = default;

std::optional<std::string> Manifest::consumer(std::string_view name) const {
    for (const auto& [key, text] : consumers_) {
        if (key == name) return text;
    }
    return std::nullopt;
}

Expected<Cartridge> Cartridge::open(const std::filesystem::path& location) {
    Cartridge c;
    const sq::cart::Manifest* m = nullptr;
    std::error_code ec;
    if (std::filesystem::is_directory(std::filesystem::symlink_status(location, ec))) {
        auto f = sq::cart::Folder::open(location);
        if (!f) return std::unexpected(from(f.error()));
        c.exploded_ = true;
        for (const sq::cart::Entry& e : f->entries()) c.entries_.push_back(EntryInfo{e.path, e.size, e.flags});
        c.impl_ = std::make_unique<Impl>(Impl{std::move(*f)});
        m = &std::get<sq::cart::Folder>(c.impl_->source).manifest();
    } else {
        auto a = sq::cart::Archive::open(location);
        if (!a) return std::unexpected(from(a.error()));
        auto live = a->entries();
        if (!live) return std::unexpected(from(live.error()));
        for (const sq::cart::Entry& e : *live) c.entries_.push_back(EntryInfo{e.path, e.size, e.flags});
        c.impl_ = std::make_unique<Impl>(Impl{std::move(*a)});
        m = &std::get<sq::cart::Archive>(c.impl_->source).manifest();
    }

    // sq.cart checked the envelope's identity; the rest of what the engine
    // reads is taken from the text here.
    Manifest& out = c.manifest_;
    out.id_ = m->id;
    out.version_ = m->version;
    out.kind_ = m->kind;
    out.json_ = m->json;
    auto parsed = sq::data::parse_json(m->json, {.ijson = true});
    if (!parsed) return std::unexpected(Error{"SQ-INF/manifest.json: " + parsed.error.message});
    const sq::data::JsonValue& json = parsed.value;
    if (auto tree = string_member(json, "tree")) {
        if (!valid_tree(*tree)) return std::unexpected(Error{"SQ-INF/manifest.json: tree '" + *tree + "' is not \".\" or a folder path ending in '/'"});
        out.tree_ = *tree;
    }
    out.title_ = string_member(json, "title");
    out.description_ = string_member(json, "description");
    if (const sq::data::JsonValue* features = json.find("requires_features")) {
        const auto* list = features->array_if();
        if (list == nullptr) return std::unexpected(Error{"SQ-INF/manifest.json: requires_features is not a list"});
        for (const sq::data::JsonValue& f : *list) {
            const std::string* s = f.string_if();
            if (s == nullptr) return std::unexpected(Error{"SQ-INF/manifest.json: requires_features holds a value that is not a string"});
            out.requires_features_.push_back(*s);
        }
    }
    if (const sq::data::JsonValue* consumers = json.find("consumers")) {
        const auto* object = consumers->object_if();
        if (object == nullptr) return std::unexpected(Error{"SQ-INF/manifest.json: consumers is not an object"});
        for (const auto& [name, section] : *object) {
            auto text = sq::data::write_json(section);
            if (!text) return std::unexpected(Error{"SQ-INF/manifest.json: consumers." + name + ": " + text.error.message});
            out.consumers_.emplace_back(name, std::move(text.text));
        }
    }
    return c;
}

Expected<std::vector<std::byte>> Cartridge::read(std::string_view path) const {
    return std::visit(
        [&](const auto& source) -> Expected<std::vector<std::byte>> {
            auto e = source.find(path);
            if (!e) return std::unexpected(from(e.error()));
            auto bytes = source.read(*e);
            if (!bytes) return std::unexpected(from(bytes.error()));
            return std::move(*bytes);
        },
        impl_->source);
}

}  // namespace squared::pg::cart
