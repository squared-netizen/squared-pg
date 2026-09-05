# Engine tests.
#
# These run against the real resources/ tree rather than a fixture copy. The
# thing under test is that the *shipped* template and kits resolve, plan and
# generate correctly; a fixture would test a copy that can drift from what
# users actually get.

set(SQUARED_PG_TESTS
    test_version
    test_identity
    test_value
    test_support
    test_lifecycle
    test_generate
    test_android)

foreach(test IN LISTS SQUARED_PG_TESTS)
    add_executable(${test} ${test}.cpp)
    target_link_libraries(${test} PRIVATE squared_pg_engine)
    target_include_directories(${test} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../src)
    target_compile_definitions(${test} PRIVATE
        SQUARED_PG_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_options(${test} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)
    add_test(NAME ${test} COMMAND ${test})
endforeach()

# test_support and test_generate reach into engine internals, so they need what
# the engine links privately.
target_link_libraries(test_support PRIVATE yyjson squared_pg_sha256)
target_link_libraries(test_value PRIVATE yyjson squared_pg_sha256)
target_link_libraries(test_generate PRIVATE squared_pg_sha256)
target_link_libraries(test_android PRIVATE squared_pg_sha256)

# The smoke test drives the CLI end to end: generate a workspace, build it, run
# it. It is the only test that proves the *output* works, not just the engine.
if(SQUARED_PG_BUILD_CLI)
    add_test(NAME smoke
             COMMAND ${CMAKE_COMMAND} -E env
                     sh "${CMAKE_SOURCE_DIR}/tools/smoke.sh" "${CMAKE_BINARY_DIR}")
    set_tests_properties(smoke PROPERTIES TIMEOUT 300)
endif()
