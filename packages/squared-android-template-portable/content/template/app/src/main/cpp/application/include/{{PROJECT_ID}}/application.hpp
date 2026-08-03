#pragma once

#include <memory>

namespace squared::application {
class Application;
}

namespace {{PROJECT_ID}} {

/**
 * @brief Construct the developer-owned application.
 *
 * This factory is the only symbol the generated SDL adapter imports from the
 * developer application target.
 */
[[nodiscard]]
std::unique_ptr<squared::application::Application>
create_application();

}  // namespace {{PROJECT_ID}}
