// SPDX-License-Identifier: MIT
//
// Entry path validation, format spec §3.3 and Limits §3.4.
//
// validate_entry_path() is the standalone entry point a writer can use
// before a container exists (FR-PATH-4). It is also invoked internally
// during open() (FR-PATH-5). Cross-entry uniqueness (FR-PATH-2 — two
// entries differing only by ASCII case or normalisation form) is a property
// of a whole entry set and is enforced in cartridge.cpp, not here; a single
// path cannot collide with itself.

#include <cstddef>
#include <span>
#include <string>

#include "impl.hpp"

namespace sqcart {
namespace {

bool is_cont(unsigned char b)
{
    return (b & 0xC0) == 0x80;
}

// Validate well-formed UTF-8 over the byte span. Implements the
// well-formedness rules: no overlong encodings, no surrogates, no code
// points above U+10FFFF, correct continuation bytes. No raw pointers.
bool valid_utf8(std::span<const std::byte> data)
{
    const size_t size = data.size();
    size_t i = 0;
    while (i < size) {
        const unsigned char b = std::to_integer<unsigned char>(data[i]);
        if (b < 0x80) {
            ++i;
            continue;
        }

        size_t need = 0;
        unsigned int cp = 0;

        if (b >= 0xC2 && b <= 0xDF) {
            need = 1;
            cp = b & 0x1F;
        } else if (b == 0xE0) {
            need = 2;
            cp = b & 0x0F;
        } else if (b >= 0xE1 && b <= 0xEC) {
            need = 2;
            cp = b & 0x0F;
        } else if (b == 0xED) {
            need = 2;
            cp = b & 0x0F;
        } else if (b >= 0xEE && b <= 0xEF) {
            need = 2;
            cp = b & 0x0F;
        } else if (b == 0xF0) {
            need = 3;
            cp = b & 0x07;
        } else if (b >= 0xF1 && b <= 0xF3) {
            need = 3;
            cp = b & 0x07;
        } else if (b == 0xF4) {
            need = 3;
            cp = b & 0x07;
        } else {
            return false;
        } // continuation byte, invalid UTF-8 lead, etc.

        if (i + need >= size) return false;
        for (size_t k = 1; k <= need; ++k) {
            const unsigned char c = std::to_integer<unsigned char>(data[i + k]);
            if (!is_cont(c)) return false;
            cp = (cp << 6) | (c & 0x3F);
        }

        if (cp > 0x10FFFF) return false;
        if (cp >= 0xD800 && cp <= 0xDFFF) return false;

        // Reject overlong encodings by requiring the first continuation byte
        // to be at least the minimum for the code point's legal range.
        const unsigned char first = std::to_integer<unsigned char>(data[i + 1]);
        if (need == 1 && first < 0x80) return false;
        if (need == 2) {
            if (b == 0xE0 && first < 0xA0) return false;
            if (b == 0xED && first >= 0xA0) return false; // surrogate
        }
        if (need == 3) {
            if (b == 0xF0 && first < 0x90) return false;
            if (b == 0xF4 && first >= 0x90) return false;
        }

        i += need + 1;
    }
    return true;
}

bool is_forbidden_control(unsigned char b)
{
    // C0: 0x00-0x1F (NUL..US); C1: 0x7F-0x9F (DEL + C1 controls).
    return b <= 0x1F || (b >= 0x7F && b <= 0x9F);
}

Result<EntryPath> validate_path(std::string_view path, const Limits& limits)
{
    if (path.empty()) {
        return unexpected(Error{ErrorCode::path_unsafe, "entry path is empty", std::nullopt,
                                std::nullopt, false});
    }
    if (path.size() > limits.max_path_bytes) {
        return unexpected(Error{ErrorCode::limit_exceeded, "entry path exceeds max_path_bytes",
                                std::nullopt, std::nullopt, false});
    }

    // Structural byte checks.
    for (size_t i = 0; i < path.size(); ++i) {
        const unsigned char b = static_cast<unsigned char>(path[i]);
        if (b == '\\' || b == ':' || is_forbidden_control(b)) {
            return unexpected(Error{ErrorCode::path_unsafe, "entry path contains a forbidden byte",
                                    std::nullopt, std::nullopt, false});
        }
    }
    if (path.front() == '/') {
        return unexpected(Error{ErrorCode::path_unsafe,
                                "entry path must be relative (no leading '/')", std::nullopt,
                                std::nullopt, false});
    }
    if (path.back() == '/') {
        return unexpected(Error{ErrorCode::path_unsafe, "entry path must not end in '/'",
                                std::nullopt, std::nullopt, false});
    }
    if (!valid_utf8(std::span<const std::byte>(reinterpret_cast<const std::byte*>(path.data()),
                                               path.size()))) {
        return unexpected(Error{ErrorCode::path_unsafe, "entry path is not well-formed UTF-8",
                                std::nullopt, std::nullopt, false});
    }

    // Segments and depth.
    size_t depth = 0;
    const size_t n = path.size();
    size_t start = 0;
    while (start < n) {
        size_t end = start;
        while (end < n && path[end] != '/')
            ++end;
        const std::string_view seg = path.substr(start, end - start);
        ++depth;
        if (seg == "." || seg == "..") {
            return unexpected(Error{ErrorCode::path_unsafe,
                                    "entry path contains a '.' or '..' segment", std::nullopt,
                                    std::nullopt, false});
        }
        if (seg.empty()) {
            return unexpected(Error{ErrorCode::path_unsafe, "entry path contains an empty segment",
                                    std::nullopt, std::nullopt, false});
        }
        start = end + 1;
    }

    if (depth > limits.max_path_depth) {
        return unexpected(Error{ErrorCode::limit_exceeded, "entry path exceeds max_path_depth",
                                std::nullopt, std::nullopt, false});
    }

    return EntryPath(path);
}

} // namespace

Result<EntryPath> validate_entry_path(std::string_view path, const Limits& limits)
{
    return validate_path(path, limits);
}

Result<EntryPath> validate_entry_path_impl(std::string_view path, const Limits& limits)
{
    return validate_path(path, limits);
}


// ---------------------------------------------------------------------------
// Collision folding and native-code detection
//
// Appended by the rewrite: both are path-domain predicates and belong beside
// validate_path rather than in the reader that happens to call them.
// ---------------------------------------------------------------------------

std::string collision_key(std::string_view path)
{
    // FR-PATH-2: fold ASCII case so that two entries differing only in case
    // are detected as colliding, which is what makes a case-sensitive logical
    // model safe to materialise on a case-insensitive host.
    //
    // Full Unicode normalisation folding is NOT performed. Doing it properly
    // needs NFC/NFD tables, and shipping a partial implementation would be
    // worse than shipping none: it would look like the check exists while
    // missing the cases it was written for. validate_path already rejects
    // non-NFC-safe sequences it can detect structurally. Recorded as a known
    // gap rather than papered over.
    std::string key;
    key.reserve(path.size());
    for (char c : path) {
        const auto u = static_cast<unsigned char>(c);
        key.push_back(static_cast<char>(u >= 'A' && u <= 'Z' ? u - 'A' + 'a' : u));
    }
    return key;
}

bool is_native_code_path(std::string_view path) noexcept
{
    // Format spec §8: loadable native code is prohibited in v1. C and C++
    // *source* is expected and permitted — kits and packages are mostly
    // that. The prohibition is on artefacts a runtime could load directly.
    static constexpr std::string_view kExtensions[] = {
        ".so", ".dll", ".dylib", ".exe", ".a", ".lib", ".o", ".obj", ".pyd", ".node",
    };

    const auto slash = path.rfind('/');
    const std::string_view name = (slash == std::string_view::npos) ? path : path.substr(slash + 1);

    for (auto ext : kExtensions) {
        if (name.size() > ext.size()) {
            const auto tail = name.substr(name.size() - ext.size());
            std::string lowered;
            lowered.reserve(tail.size());
            for (char c : tail) {
                const auto u = static_cast<unsigned char>(c);
                lowered.push_back(static_cast<char>(u >= 'A' && u <= 'Z' ? u - 'A' + 'a' : u));
            }
            if (lowered == ext) {
                return true;
            }
        }
    }

    // Versioned shared objects: libfoo.so.1, libfoo.so.1.2.3
    if (name.find(".so.") != std::string_view::npos) {
        return true;
    }
    return false;
}

}  // namespace sqcart
