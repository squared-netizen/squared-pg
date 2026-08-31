// SPDX-License-Identifier: MIT
//
// Fixture header. Present so the kit has plausible payload with a nested
// path; the tests care that it round-trips byte-identically, not what it says.

#ifndef TESTCART_BRIDGE_HPP
#define TESTCART_BRIDGE_HPP

namespace testcart {

/// Initialise the fixture bridge. Returns true on success.
bool initialise() noexcept;

/// Release anything initialise() acquired.
void shutdown() noexcept;

}  // namespace testcart

#endif  // TESTCART_BRIDGE_HPP
