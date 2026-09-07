// SPDX-License-Identifier: MIT
//
// Access to the embedded manifest seeds. See seeds.cpp for why they are
// embedded rather than loaded from assets/ at runtime.

#ifndef SQCART_APP_SEEDS_HPP
#define SQCART_APP_SEEDS_HPP

#include <optional>
#include <string_view>
#include <vector>

namespace sqcart::app {

/// The envelope seed. One seed serves every kind as of format 2: the kind
/// is a substituted token, not a schema selector.
[[nodiscard]] std::optional<std::string_view> seed_for(std::string_view kind);

/// Every kind that has a seed, in declaration order. Used for the usage text
/// so a new seed cannot be added without appearing in --help.
[[nodiscard]] std::vector<std::string_view> seed_kinds();

}  // namespace sqcart::app

#endif  // SQCART_APP_SEEDS_HPP
