# Build fragment contributed by kit.testcart.
#
# Corresponds to the declared "build.cmake.targets" integration area: this is
# the file the generated project's build system is expected to include.

add_library(testcart_bridge STATIC
    ${CMAKE_CURRENT_LIST_DIR}/../bridge/src/bridge.cpp)

target_include_directories(testcart_bridge PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../bridge/include)

add_library(Testcart::Bridge ALIAS testcart_bridge)
