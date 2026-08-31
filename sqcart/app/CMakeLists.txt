# The sqcart command-line tool.
#
# Split into a library (sqcart_app) and a one-file executable so the command
# logic is linkable into tests. main.cpp is the only translation unit that
# names a terminal; everything else takes an Environment.

add_library(sqcart_app STATIC
    src/cli.cpp
    src/commands_read.cpp
    src/commands_write.cpp
    src/cmd_create.cpp
    src/seeds.cpp)

target_include_directories(sqcart_app
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(sqcart_app PUBLIC sqcart_read)
if(TARGET sqcart_write)
    target_link_libraries(sqcart_app PUBLIC sqcart_write)
endif()

target_compile_options(sqcart_app PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

add_executable(sqcart_cli src/main.cpp)
set_target_properties(sqcart_cli PROPERTIES OUTPUT_NAME sqcart)
target_link_libraries(sqcart_cli PRIVATE sqcart_app)
