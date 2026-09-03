// SPDX-License-Identifier: MIT

#include "support/support.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

extern "C" {
#include "sha256.h"
}

#include "yyjson.h"

namespace squared::pg::support {

// ---------------------------------------------------------------------------
// Digests
// ---------------------------------------------------------------------------

Digest sha256(std::span<const std::byte> bytes) {
    SHA256_CTX ctx;
    sha256_init(&ctx);
    if (!bytes.empty()) {
        sha256_update(&ctx, reinterpret_cast<const BYTE*>(bytes.data()), bytes.size());
    }
    Digest digest{};
    sha256_final(&ctx, reinterpret_cast<BYTE*>(digest.data()));
    return digest;
}

Digest sha256(std::string_view text) {
    return sha256(std::span<const std::byte>{reinterpret_cast<const std::byte*>(text.data()), text.size()});
}

std::string to_hex(const Digest& digest) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(digest.size() * 2);
    for (std::byte b : digest) {
        const auto value = static_cast<unsigned char>(b);
        out.push_back(kHex[value >> 4]);
        out.push_back(kHex[value & 0x0F]);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Glob
// ---------------------------------------------------------------------------

namespace {

/// Backtracking matcher over the pattern. Linear in the common case; the
/// worst case needs a pattern with several `**` segments against a long path,
/// which manifest patterns never are.
bool match_here(std::string_view pattern, std::string_view path) {
    std::size_t p = 0;
    std::size_t s = 0;
    std::size_t star_p = std::string_view::npos;  // position after a `**`
    std::size_t star_s = 0;

    while (s < path.size()) {
        if (p < pattern.size()) {
            const char pc = pattern[p];
            if (pc == '*') {
                const bool doubled = (p + 1 < pattern.size()) && pattern[p + 1] == '*';
                if (doubled) {
                    p += 2;
                    // `**/` should also match zero directories, so that
                    // `a/**/b.c` covers `a/b.c`.
                    if (p < pattern.size() && pattern[p] == '/') {
                        if (match_here(pattern.substr(p + 1), path.substr(s))) return true;
                    }
                    star_p = p;
                    star_s = s;
                    continue;
                }
                // Single `*`: consume within one path segment.
                if (match_here(pattern.substr(p + 1), path.substr(s))) return true;
                if (path[s] == '/') return false;
                ++s;
                continue;
            }
            if (pc == '?' && path[s] != '/') {
                ++p;
                ++s;
                continue;
            }
            if (pc == path[s]) {
                ++p;
                ++s;
                continue;
            }
        }
        if (star_p != std::string_view::npos) {
            // Re-enter the `**` and consume one more character.
            ++star_s;
            if (star_s > path.size()) return false;
            p = star_p;
            s = star_s;
            continue;
        }
        return false;
    }

    while (p < pattern.size() && pattern[p] == '*') ++p;
    return p == pattern.size();
}

}  // namespace

bool glob_match(std::string_view pattern, std::string_view path) {
    if (pattern.empty()) return false;
    if (pattern == "**" || pattern == "*") return true;
    // `dir/**` names the directory as well as its contents. Requiring a
    // manifest to write both `dir` and `dir/**` would be a papercut every
    // resource author hits once.
    if (pattern.ends_with("/**")) {
        const std::string_view prefix = pattern.substr(0, pattern.size() - 3);
        if (path == prefix) return true;
    }
    return match_here(pattern, path);
}

int glob_specificity(std::string_view pattern) {
    // Literal characters are evidence of intent; wildcards are the opposite.
    // A `**` costs more than a `*` because it crosses segment boundaries.
    int score = 0;
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i] == '*') {
            if (i + 1 < pattern.size() && pattern[i + 1] == '*') {
                score -= 8;
                ++i;
            } else {
                score -= 4;
            }
        } else if (pattern[i] == '?') {
            score -= 1;
        } else {
            score += 2;
        }
    }
    return score;
}

// ---------------------------------------------------------------------------
// JSON
// ---------------------------------------------------------------------------

namespace {

Value from_yyjson(yyjson_val* val) {
    if (val == nullptr) return {};
    switch (yyjson_get_type(val)) {
        case YYJSON_TYPE_NULL:
            return {};
        case YYJSON_TYPE_BOOL:
            return Value{yyjson_get_bool(val)};
        case YYJSON_TYPE_NUM:
            if (yyjson_is_int(val) || yyjson_is_uint(val)) {
                return Value{static_cast<std::int64_t>(yyjson_get_sint(val))};
            }
            return Value{yyjson_get_real(val)};
        case YYJSON_TYPE_STR:
            return Value{std::string{yyjson_get_str(val), yyjson_get_len(val)}};
        case YYJSON_TYPE_ARR: {
            Array array;
            array.reserve(yyjson_arr_size(val));
            yyjson_val*      item = nullptr;
            yyjson_arr_iter  iter;
            yyjson_arr_iter_init(val, &iter);
            while ((item = yyjson_arr_iter_next(&iter)) != nullptr) {
                array.push_back(from_yyjson(item));
            }
            return Value{std::move(array)};
        }
        case YYJSON_TYPE_OBJ: {
            Object object;
            object.reserve(yyjson_obj_size(val));
            yyjson_val*     key = nullptr;
            yyjson_obj_iter iter;
            yyjson_obj_iter_init(val, &iter);
            while ((key = yyjson_obj_iter_next(&iter)) != nullptr) {
                yyjson_val* value = yyjson_obj_iter_get_val(key);
                object.emplace_back(std::string{yyjson_get_str(key), yyjson_get_len(key)},
                                    from_yyjson(value));
            }
            return Value{std::move(object)};
        }
        default:
            return {};
    }
}

void write_escaped(std::string& out, std::string_view text) {
    out.push_back('"');
    for (char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buffer[8];
                    std::snprintf(buffer, sizeof buffer, "\\u%04x", static_cast<unsigned char>(c));
                    out += buffer;
                } else {
                    out.push_back(c);
                }
        }
    }
    out.push_back('"');
}

void write_value(std::string& out, const Value& value, bool pretty, int depth) {
    const std::string indent = pretty ? std::string(static_cast<std::size_t>(depth + 1) * 2, ' ') : std::string{};
    const std::string closing = pretty ? std::string(static_cast<std::size_t>(depth) * 2, ' ') : std::string{};

    switch (value.kind()) {
        case ValueKind::null:
            out += "null";
            return;
        case ValueKind::boolean:
            out += *value.as_bool() ? "true" : "false";
            return;
        case ValueKind::integer:
            out += std::to_string(*value.as_int());
            return;
        case ValueKind::number: {
            char buffer[40];
            std::snprintf(buffer, sizeof buffer, "%.17g", *value.as_number());
            out += buffer;
            return;
        }
        case ValueKind::string:
            write_escaped(out, *value.as_string());
            return;
        case ValueKind::array: {
            const Array& array = *value.as_array();
            if (array.empty()) {
                out += "[]";
                return;
            }
            out += '[';
            for (std::size_t i = 0; i < array.size(); ++i) {
                if (i > 0) out += ',';
                if (pretty) {
                    out += '\n';
                    out += indent;
                }
                write_value(out, array[i], pretty, depth + 1);
            }
            if (pretty) {
                out += '\n';
                out += closing;
            }
            out += ']';
            return;
        }
        case ValueKind::object: {
            const Object& object = *value.as_object();
            if (object.empty()) {
                out += "{}";
                return;
            }
            out += '{';
            for (std::size_t i = 0; i < object.size(); ++i) {
                if (i > 0) out += ',';
                if (pretty) {
                    out += '\n';
                    out += indent;
                }
                write_escaped(out, object[i].first);
                out += pretty ? ": " : ":";
                write_value(out, object[i].second, pretty, depth + 1);
            }
            if (pretty) {
                out += '\n';
                out += closing;
            }
            out += '}';
            return;
        }
    }
}

}  // namespace

Result<Value> json_parse(std::string_view text) {
    yyjson_read_err error{};
    yyjson_doc* doc = yyjson_read_opts(const_cast<char*>(text.data()), text.size(),
                                       YYJSON_READ_NOFLAG, nullptr, &error);
    if (doc == nullptr) {
        return fail(ErrorCategory::configuration, "configuration.json.malformed",
                    std::string{"invalid JSON at byte "} + std::to_string(error.pos) + ": " +
                        (error.msg != nullptr ? error.msg : "unknown"));
    }
    Value value = from_yyjson(yyjson_doc_get_root(doc));
    yyjson_doc_free(doc);
    return value;
}

std::string json_write(const Value& value, bool pretty) {
    std::string out;
    write_value(out, value, pretty, 0);
    if (pretty) out.push_back('\n');
    return out;
}

// ---------------------------------------------------------------------------
// Paths
// ---------------------------------------------------------------------------

std::vector<std::string_view> split_path(std::string_view path) {
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (start <= path.size()) {
        const std::size_t slash = path.find('/', start);
        const std::string_view part = path.substr(start, slash == std::string_view::npos ? slash : slash - start);
        if (!part.empty()) parts.push_back(part);
        if (slash == std::string_view::npos) break;
        start = slash + 1;
    }
    return parts;
}

std::string normalize_relative(std::string_view path) {
    if (path.empty()) return {};
    // Reject anything that could leave the workspace before doing any work.
    // A Windows drive letter counts: `C:foo` is not relative on the platform
    // that matters for it, and admitting it here would be a defence that only
    // works where the attack does not exist.
    if (path.front() == '/' || path.front() == '\\') return {};
    if (path.size() >= 2 && path[1] == ':') return {};

    std::string flattened;
    flattened.reserve(path.size());
    for (char c : path) flattened.push_back(c == '\\' ? '/' : c);

    std::vector<std::string_view> out;
    for (std::string_view part : split_path(flattened)) {
        if (part == ".") continue;
        if (part == "..") return {};  // never resolve upward; refuse instead
        out.push_back(part);
    }
    if (out.empty()) return {};

    std::string result;
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (i > 0) result.push_back('/');
        result.append(out[i]);
    }
    return result;
}

std::vector<std::string> ancestor_directories(std::string_view path) {
    std::vector<std::string> directories;
    const auto parts = split_path(path);
    if (parts.size() < 2) return directories;
    std::string current;
    for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
        if (!current.empty()) current.push_back('/');
        current.append(parts[i]);
        directories.push_back(current);
    }
    return directories;
}

std::string to_lower(std::string_view text) {
    std::string out{text};
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

}  // namespace squared::pg::support
