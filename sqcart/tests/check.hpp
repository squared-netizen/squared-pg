// SPDX-License-Identifier: MIT
// Minimal assertion helper shared by the sqcart tests.

#ifndef SQCART_TESTS_CHECK_HPP
#define SQCART_TESTS_CHECK_HPP

#include <cstdio>

namespace sqcart::test {
inline int failures = 0;

inline void check(bool condition, const char* expr, const char* file, int line) {
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "FAIL %s:%d  %s\n", file, line, expr);
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
}  // namespace sqcart::test

#define CHECK(expr) ::sqcart::test::check((expr), #expr, __FILE__, __LINE__)

#endif  // SQCART_TESTS_CHECK_HPP
