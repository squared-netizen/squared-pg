// SPDX-License-Identifier: MIT

#include "squared/pg/version.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>

namespace squared::pg {
namespace {

/// SemVer numeric identifiers admit no leading zeros and no sign.
[[nodiscard]] std::optional<std::uint32_t> parse_numeric(std::string_view text) {
    if (text.empty() || text.size() > 9) return std::nullopt;
    if (text.size() > 1 && text.front() == '0') return std::nullopt;
    std::uint32_t value = 0;
    for (char c : text) {
        if (c < '0' || c > '9') return std::nullopt;
        value = value * 10 + static_cast<std::uint32_t>(c - '0');
    }
    return value;
}

[[nodiscard]] bool is_identifier_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-';
}

[[nodiscard]] bool valid_dot_separated(std::string_view text) {
    if (text.empty()) return false;
    std::size_t start = 0;
    while (true) {
        std::size_t dot = text.find('.', start);
        std::string_view part = text.substr(start, dot == std::string_view::npos ? dot : dot - start);
        if (part.empty()) return false;
        for (char c : part) {
            if (!is_identifier_char(c)) return false;
        }
        if (dot == std::string_view::npos) return true;
        start = dot + 1;
    }
}

[[nodiscard]] bool all_digits(std::string_view text) {
    return !text.empty() && std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
}

/// SemVer §11.4 pre-release precedence.
[[nodiscard]] std::strong_ordering compare_prerelease(std::string_view lhs, std::string_view rhs) {
    // §11.3: a version with a pre-release has lower precedence than one
    // without. Absent beats present, which is the reverse of the intuition
    // that "more information sorts higher".
    if (lhs.empty() && rhs.empty()) return std::strong_ordering::equal;
    if (lhs.empty()) return std::strong_ordering::greater;
    if (rhs.empty()) return std::strong_ordering::less;

    std::size_t li = 0;
    std::size_t ri = 0;
    while (true) {
        const bool l_done = li > lhs.size();
        const bool r_done = ri > rhs.size();
        if (l_done && r_done) return std::strong_ordering::equal;
        if (l_done) return std::strong_ordering::less;
        if (r_done) return std::strong_ordering::greater;

        std::size_t l_dot = lhs.find('.', li);
        std::size_t r_dot = rhs.find('.', ri);
        std::string_view l_part = lhs.substr(li, l_dot == std::string_view::npos ? l_dot : l_dot - li);
        std::string_view r_part = rhs.substr(ri, r_dot == std::string_view::npos ? r_dot : r_dot - ri);

        const bool l_num = all_digits(l_part);
        const bool r_num = all_digits(r_part);
        if (l_num && r_num) {
            if (l_part.size() != r_part.size()) return l_part.size() <=> r_part.size();
            if (auto cmp = l_part <=> r_part; cmp != 0) return cmp;
        } else if (l_num != r_num) {
            // Numeric identifiers always have lower precedence than
            // alphanumeric ones.
            return l_num ? std::strong_ordering::less : std::strong_ordering::greater;
        } else if (auto cmp = l_part <=> r_part; cmp != 0) {
            return cmp;
        }

        li = l_dot == std::string_view::npos ? lhs.size() + 1 : l_dot + 1;
        ri = r_dot == std::string_view::npos ? rhs.size() + 1 : r_dot + 1;
    }
}

[[nodiscard]] std::string_view trim(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);
    return text;
}

}  // namespace

std::optional<Version> Version::parse(std::string_view text) {
    text = trim(text);
    if (text.empty()) return std::nullopt;
    // Tolerate a leading 'v'; manifests occasionally carry it and rejecting
    // the whole resource over one character helps nobody.
    if (text.front() == 'v' || text.front() == 'V') text.remove_prefix(1);

    Version version;

    std::string_view build;
    if (std::size_t plus = text.find('+'); plus != std::string_view::npos) {
        build = text.substr(plus + 1);
        text  = text.substr(0, plus);
        if (!valid_dot_separated(build)) return std::nullopt;
    }

    std::string_view prerelease;
    if (std::size_t dash = text.find('-'); dash != std::string_view::npos) {
        prerelease = text.substr(dash + 1);
        text       = text.substr(0, dash);
        if (!valid_dot_separated(prerelease)) return std::nullopt;
        // A numeric pre-release identifier may not carry leading zeros.
        std::size_t start = 0;
        while (start <= prerelease.size()) {
            std::size_t dot = prerelease.find('.', start);
            std::string_view part =
                prerelease.substr(start, dot == std::string_view::npos ? dot : dot - start);
            if (all_digits(part) && part.size() > 1 && part.front() == '0') return std::nullopt;
            if (dot == std::string_view::npos) break;
            start = dot + 1;
        }
    }

    const std::size_t first = text.find('.');
    if (first == std::string_view::npos) return std::nullopt;
    const std::size_t second = text.find('.', first + 1);
    if (second == std::string_view::npos) return std::nullopt;
    if (text.find('.', second + 1) != std::string_view::npos) return std::nullopt;

    auto major = parse_numeric(text.substr(0, first));
    auto minor = parse_numeric(text.substr(first + 1, second - first - 1));
    auto patch = parse_numeric(text.substr(second + 1));
    if (!major || !minor || !patch) return std::nullopt;

    version.major_      = *major;
    version.minor_      = *minor;
    version.patch_      = *patch;
    version.prerelease_ = std::string{prerelease};
    version.build_      = std::string{build};
    return version;
}

std::string Version::to_string() const {
    std::string out = std::to_string(major_) + '.' + std::to_string(minor_) + '.' + std::to_string(patch_);
    if (!prerelease_.empty()) out += '-' + prerelease_;
    if (!build_.empty()) out += '+' + build_;
    return out;
}

std::strong_ordering Version::operator<=>(const Version& other) const noexcept {
    if (auto cmp = major_ <=> other.major_; cmp != 0) return cmp;
    if (auto cmp = minor_ <=> other.minor_; cmp != 0) return cmp;
    if (auto cmp = patch_ <=> other.patch_; cmp != 0) return cmp;
    return compare_prerelease(prerelease_, other.prerelease_);
}

bool Version::operator==(const Version& other) const noexcept {
    return major_ == other.major_ && minor_ == other.minor_ && patch_ == other.patch_ &&
           prerelease_ == other.prerelease_;
}

std::optional<VersionRange> VersionRange::parse(std::string_view text) {
    VersionRange range;
    range.text_ = std::string{trim(text)};

    std::string_view rest = trim(text);
    if (rest.empty() || rest == "*" || rest == "any") return range;

    while (!rest.empty()) {
        rest = trim(rest);
        if (rest.empty()) break;

        Op op = Op::eq;
        if (rest.starts_with(">=")) {
            op = Op::gte;
            rest.remove_prefix(2);
        } else if (rest.starts_with("<=")) {
            op = Op::lte;
            rest.remove_prefix(2);
        } else if (rest.starts_with('>')) {
            op = Op::gt;
            rest.remove_prefix(1);
        } else if (rest.starts_with('<')) {
            op = Op::lt;
            rest.remove_prefix(1);
        } else if (rest.starts_with('=')) {
            op = Op::eq;
            rest.remove_prefix(1);
        } else if (rest.starts_with('^')) {
            op = Op::caret;
            rest.remove_prefix(1);
        } else if (rest.starts_with('~')) {
            op = Op::tilde;
            rest.remove_prefix(1);
        }
        rest = trim(rest);

        const std::size_t end = rest.find(' ');
        std::string_view token = rest.substr(0, end);
        if (token.empty()) return std::nullopt;

        auto version = Version::parse(token);
        if (!version) return std::nullopt;
        range.clauses_.push_back(Clause{op, *version});

        rest = end == std::string_view::npos ? std::string_view{} : rest.substr(end);
    }

    return range;
}

bool VersionRange::satisfied_by(const Version& version) const noexcept {
    if (clauses_.empty()) return true;

    // §2.8.2: a pre-release is only selectable when a clause explicitly names
    // a pre-release at the same (major, minor, patch). Without this rule an
    // innocuous ">=1.0.0" would happily select 2.0.0-alpha.1 over 1.9.0.
    if (version.is_prerelease()) {
        const bool invited = std::any_of(clauses_.begin(), clauses_.end(), [&](const Clause& clause) {
            return clause.version.is_prerelease() && clause.version.major() == version.major() &&
                   clause.version.minor() == version.minor() && clause.version.patch() == version.patch();
        });
        if (!invited) return false;
    }

    for (const Clause& clause : clauses_) {
        const Version& bound = clause.version;
        switch (clause.op) {
            case Op::eq:
                if (!(version == bound)) return false;
                break;
            case Op::lt:
                if (!(version < bound)) return false;
                break;
            case Op::lte:
                if (!(version <= bound)) return false;
                break;
            case Op::gt:
                if (!(version > bound)) return false;
                break;
            case Op::gte:
                if (!(version >= bound)) return false;
                break;
            case Op::caret: {
                // ^1.2.3 => >=1.2.3 <2.0.0; ^0.2.3 => >=0.2.3 <0.3.0;
                // ^0.0.3 => >=0.0.3 <0.0.4. Below 1.0.0 SemVer makes the
                // minor, then the patch, the compatibility boundary.
                if (version < bound) return false;
                if (bound.major() > 0) {
                    if (version.major() != bound.major()) return false;
                } else if (bound.minor() > 0) {
                    if (version.major() != 0 || version.minor() != bound.minor()) return false;
                } else {
                    if (version.major() != 0 || version.minor() != 0 || version.patch() != bound.patch())
                        return false;
                }
                break;
            }
            case Op::tilde: {
                // ~1.2.3 => >=1.2.3 <1.3.0
                if (version < bound) return false;
                if (version.major() != bound.major() || version.minor() != bound.minor()) return false;
                break;
            }
        }
    }
    return true;
}

}  // namespace squared::pg
