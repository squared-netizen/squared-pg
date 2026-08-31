// SPDX-License-Identifier: MIT
//
// Fixture platform entrypoint. Referenced by the kit's declared
// "app.entrypoint" integration area.

#include "testcart/bridge.hpp"

int main()
{
    if (!testcart::initialise()) {
        return 1;
    }
    testcart::shutdown();
    return 0;
}
