// SPDX-License-Identifier: MIT
//
// Internal glob matcher declaration. See glob.cpp for semantics.

#ifndef SQCART_GLOB_HPP
#define SQCART_GLOB_HPP

#include <string_view>

namespace sqcart::detail {

// True when path matches pattern, supporting '*', '**' and '?'.
bool glob_match(std::string_view path, std::string_view pattern);

}  // namespace sqcart::detail

#endif  // SQCART_GLOB_HPP
