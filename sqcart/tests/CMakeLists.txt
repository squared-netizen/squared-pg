# Dependency-free tests.
#
# No GoogleTest, no Catch2: the repository is offline-first (§1.13) and a
# fetched framework would break a clean checkout. Each test is a plain
# executable returning non-zero on failure, registered with CTest.
#
# Tests run with WORKING_DIRECTORY set to the source tree so relative fixture
# paths ("tests/testsqcart") resolve identically under CTest and when a
# binary is run by hand from the project root.

function(sqcart_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE sqcart_read)
    if(TARGET sqcart_write)
        target_link_libraries(${name} PRIVATE sqcart_write)
    endif()
    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src)
    target_compile_definitions(${name} PRIVATE
        SQCART_TEST_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    target_compile_options(${name} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
endfunction()

sqcart_add_test(test_errors)
sqcart_add_test(test_expected)
sqcart_add_test(test_path)
sqcart_add_test(test_manifest)
sqcart_add_test(test_conformance)
sqcart_add_test(test_seeds)
sqcart_add_test(test_symlinks)
sqcart_add_test(test_open)
sqcart_add_test(test_exploded)
sqcart_add_test(test_digest)
sqcart_add_test(test_validate)
sqcart_add_test(test_extract)

if(TARGET sqcart_write)
    sqcart_add_test(test_write)
endif()
