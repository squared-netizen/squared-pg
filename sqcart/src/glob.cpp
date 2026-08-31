// SPDX-License-Identifier: MIT
//
// Minimal glob matcher for TemplateBody::ownership patterns (format spec
// §6.2) and any other path-vs-pattern classification in validate().
//
// First-draft subset: '*' (within a single segment), '**' (across segment
// boundaries), '?' (single byte), and literal characters. This is the
// minimal set the ownership globs need; brace expansion, character classes
// and negation are deliberately out of scope. Paths and patterns both use
// '/' as separator.

#include "glob.hpp"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace sqcart::detail {
namespace {

// Match a single pattern segment against a single path segment. '?' matches
// one byte; '*' matches any run of bytes within this segment.
bool match_segment(std::string_view seg, std::string_view pat) {
    if (pat.empty()) return seg.empty();

    const size_t star = pat.find('*');
    if (star == std::string_view::npos) {
        if (pat.size() != seg.size()) return false;
        for (size_t i = 0; i < pat.size(); ++i) {
            if (pat[i] != '?' && pat[i] != seg[i]) return false;
        }
        return true;
    }

    const std::string_view pre = pat.substr(0, star);
    if (pre.size() > seg.size()) return false;
    for (size_t i = 0; i < pre.size(); ++i) {
        if (pre[i] != '?' && pre[i] != seg[i]) return false;
    }

    std::string_view tail = pat.substr(star + 1);
    if (tail.empty()) return true;  // trailing '*'

    // Try every split of the remaining segment bytes.
    for (size_t k = pre.size(); k <= seg.size(); ++k) {
        if (match_segment(seg.substr(k), tail)) return true;
    }
    return false;
}

}  // namespace

bool glob_match(std::string_view path, std::string_view pattern) {
    auto split = [](std::string_view s) -> std::vector<std::string_view> {
        std::vector<std::string_view> out;
        size_t start = 0;
        while (true) {
            const size_t slash = s.find('/', start);
            if (slash == std::string_view::npos) {
                out.push_back(s.substr(start));
                break;
            }
            out.push_back(s.substr(start, slash - start));
            start = slash + 1;
        }
        if (!s.empty() && s.back() == '/') out.pop_back();
        return out;
    };

    const auto pseg = split(path);
    const auto mseg = split(pattern);

    std::function<bool(size_t, size_t)> rec =
        [&](size_t pi, size_t mi) -> bool {
            if (mi == mseg.size()) return pi == pseg.size();
            const std::string_view m = mseg[mi];
            if (m == "**") {
                for (size_t k = pi; k <= pseg.size(); ++k) {
                    if (rec(k, mi + 1)) return true;
                }
                return false;
            }
            if (pi == pseg.size()) return false;
            if (!match_segment(pseg[pi], m)) return false;
            return rec(pi + 1, mi + 1);
        };

    return rec(0, 0);
}

}  // namespace sqcart::detail
