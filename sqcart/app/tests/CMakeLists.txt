# CLI tests. These link sqcart_app and drive the real command functions over
# string streams -- no subprocess, no shell. See test_cli.cpp.

add_executable(test_cli test_cli.cpp)
target_link_libraries(test_cli PRIVATE sqcart_app)
target_include_directories(test_cli PRIVATE
    ${PROJECT_SOURCE_DIR}/tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_compile_definitions(test_cli PRIVATE
    SQCART_TEST_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_compile_options(test_cli PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

add_test(NAME test_cli COMMAND test_cli)
set_tests_properties(test_cli PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
