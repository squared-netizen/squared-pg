// SPDX-License-Identifier: MIT
// Minimal assertion helper, matching the one the sqcart suite uses so the two
// test suites read the same way.

#ifndef SQUARED_PG_TESTS_CHECK_HPP
#define SQUARED_PG_TESTS_CHECK_HPP

#include <cstdio>
#include <string>

namespace squared::pg::test {

inline int failures = 0;

inline void check(bool condition, const char* expr, const char* file, int line) {
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "FAIL %s:%d  %s\n", file, line, expr);
    }
}

inline void check_eq(const std::string& actual, const std::string& expected, const char* expr,
                     const char* file, int line) {
    if (actual != expected) {
        ++failures;
        std::fprintf(stderr, "FAIL %s:%d  %s\n  expected: %s\n  actual:   %s\n", file, line, expr,
                     expected.c_str(), actual.c_str());
    }
}

inline int report(const char* suite) {
    if (failures == 0) {
        std::fprintf(stderr, "ok   %s\n", suite);
        return 0;
    }
    std::fprintf(stderr, "FAIL %s (%d failed)\n", suite, failures);
    return 1;
}

}  // namespace squared::pg::test

#define CHECK(expr) ::squared::pg::test::check((expr), #expr, __FILE__, __LINE__)
#define CHECK_EQ(actual, expected) \
    ::squared::pg::test::check_eq((actual), (expected), #actual " == " #expected, __FILE__, __LINE__)

#endif  // SQUARED_PG_TESTS_CHECK_HPP
