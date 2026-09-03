# sqpg — the reference CLI host.
#
# A thin host and nothing more (§2.1.6). It contains no argument grammar and no
# generation policy; both live in the Lua workflow.

add_executable(sqpg src/main.cpp)

target_link_libraries(sqpg PRIVATE squared_pg_lua)

target_compile_options(sqpg PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

# The host locates resources and workflows relative to its own executable, so
# building into the tree root makes an in-place run work without installation.
set_target_properties(sqpg PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
