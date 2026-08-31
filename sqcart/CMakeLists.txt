# sqcart — Squared Cartridge reference implementation
#
# This file is a real top-level CMake project, not merely an add_subdirectory
# target. It MUST configure, build and test with no reference to the parent
# squared-pg tree; tools/sqcart-isolation.fish enforces that by copying this
# directory elsewhere and building it there.

cmake_minimum_required(VERSION 3.21)

project(sqcart
    VERSION     0.2.0
    DESCRIPTION "Reference implementation of the Squared Cartridge Specification 1.0"
    LANGUAGES   C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------

option(SQCART_USE_EXTERNAL_MINIZ  "Link a host-provided miniz target"   OFF)
option(SQCART_USE_EXTERNAL_YYJSON "Link a host-provided yyjson target"  OFF)
option(SQCART_ENABLE_WRITER       "Build the cartridge writer"          ON)
option(SQCART_ENABLE_CLI          "Build the sqcart command-line tool"  ${PROJECT_IS_TOP_LEVEL})
option(SQCART_ENABLE_LUA          "Build the Lua 5.4 binding"           OFF)
option(SQCART_BUILD_TESTS         "Build the test suite"                ${PROJECT_IS_TOP_LEVEL})

# The CLI is the product; it needs the writer for `pack`. Rather than fail
# late at link time, say so here.
if(SQCART_ENABLE_CLI AND NOT SQCART_ENABLE_WRITER)
    message(STATUS "sqcart: CLI enabled without the writer; `pack` will report "
                   "that this build cannot write cartridges")
endif()

# ---------------------------------------------------------------------------
# std::expected availability
#
# <expected> is C++23; this project targets C++20, so it may be absent.
# sqcart/expected.hpp aliases std::expected when present and supplies a
# minimal substitute otherwise. See docs/developer/sqcart/expected.md.
# ---------------------------------------------------------------------------

include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "-std=c++20")
check_cxx_source_compiles("
    #include <expected>
    #if !defined(__cpp_lib_expected)
    #error no expected
    #endif
    int main() { std::expected<int,int> e{1}; return e.has_value() ? 0 : 1; }
" SQCART_HAVE_STD_EXPECTED)
unset(CMAKE_REQUIRED_FLAGS)

# ---------------------------------------------------------------------------
# Backends
#
# Standalone builds use the copies vendored under third_party/. squared-pg
# sets both SQCART_USE_EXTERNAL_* to ON and supplies its own, so one build
# never contains two copies. Vendored headers are attached with SYSTEM so
# their warnings do not drown ours.
# ---------------------------------------------------------------------------

if(SQCART_USE_EXTERNAL_MINIZ)
    if(NOT TARGET miniz)
        message(FATAL_ERROR
            "SQCART_USE_EXTERNAL_MINIZ is ON but no 'miniz' target exists. "
            "The parent project must define it before add_subdirectory(sqcart).")
    endif()
    set(SQCART_MINIZ_TARGET miniz)
else()
    set(_miniz_dir "${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniz-3.1.2")
    if(NOT EXISTS "${_miniz_dir}/miniz.c")
        message(FATAL_ERROR
            "Vendored miniz not found at ${_miniz_dir}. A standalone sqcart "
            "build cannot fall back to the parent tree.")
    endif()
    add_library(sqcart_miniz STATIC
        "${_miniz_dir}/miniz.c"
        "${_miniz_dir}/miniz_tdef.c"
        "${_miniz_dir}/miniz_tinfl.c"
        "${_miniz_dir}/miniz_zip.c")
    target_include_directories(sqcart_miniz SYSTEM PUBLIC "${_miniz_dir}")
    set(SQCART_MINIZ_TARGET sqcart_miniz)
endif()

if(SQCART_USE_EXTERNAL_YYJSON)
    if(NOT TARGET yyjson)
        message(FATAL_ERROR "SQCART_USE_EXTERNAL_YYJSON is ON but no 'yyjson' target exists.")
    endif()
    set(SQCART_YYJSON_TARGET yyjson)
else()
    set(_yyjson_dir "${CMAKE_CURRENT_SOURCE_DIR}/third_party/yyjson-0.12.0/src")
    if(NOT EXISTS "${_yyjson_dir}/yyjson.c")
        message(FATAL_ERROR "Vendored yyjson not found at ${_yyjson_dir}.")
    endif()
    add_library(sqcart_yyjson STATIC "${_yyjson_dir}/yyjson.c")
    target_include_directories(sqcart_yyjson SYSTEM PUBLIC "${_yyjson_dir}")
    set(SQCART_YYJSON_TARGET sqcart_yyjson)
endif()

# SHA-256. Always vendored: the algorithm is frozen by FIPS 180-4 and there is
# no host-provided variant worth an option. test_digest.cpp re-checks it
# against the published vectors on every run.
set(_sha_dir "${CMAKE_CURRENT_SOURCE_DIR}/third_party/crypto-algorithms")
if(NOT EXISTS "${_sha_dir}/sha256.c")
    message(FATAL_ERROR "Vendored SHA-256 not found at ${_sha_dir}.")
endif()
add_library(sqcart_sha256 STATIC "${_sha_dir}/sha256.c")
target_include_directories(sqcart_sha256 SYSTEM PUBLIC "${_sha_dir}")

# ---------------------------------------------------------------------------
# sqcart::read — the reader. Always built.
# ---------------------------------------------------------------------------

set(SQCART_READ_SOURCES
    src/errors.cpp
    src/path.cpp
    src/glob.cpp
    src/manifest.cpp
    src/reader.cpp
    src/digest.cpp
    src/validate.cpp
    src/extract.cpp
    src/sha256.cpp
    src/symlink.cpp)

add_library(sqcart_read STATIC ${SQCART_READ_SOURCES})
add_library(sqcart::read ALIAS sqcart_read)

target_include_directories(sqcart_read
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(sqcart_read
    PRIVATE ${SQCART_MINIZ_TARGET} ${SQCART_YYJSON_TARGET} sqcart_sha256)

if(SQCART_HAVE_STD_EXPECTED)
    target_compile_definitions(sqcart_read PUBLIC SQCART_HAVE_STD_EXPECTED=1)
endif()

target_compile_options(sqcart_read PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wconversion>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>)

# ---------------------------------------------------------------------------
# sqcart::write — the writer. Separable so a game runtime or launcher carries
# no compressor and no manifest serialiser (component spec §4.1).
# ---------------------------------------------------------------------------

if(SQCART_ENABLE_WRITER)
    add_library(sqcart_write STATIC src/writer.cpp)
    add_library(sqcart::write ALIAS sqcart_write)

    target_include_directories(sqcart_write PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(sqcart_write
        PUBLIC  sqcart_read
        PRIVATE ${SQCART_MINIZ_TARGET} ${SQCART_YYJSON_TARGET} sqcart_sha256)
    target_compile_definitions(sqcart_write PUBLIC SQCART_ENABLE_WRITER=1)
    target_compile_options(sqcart_write PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wconversion>)
endif()

# ---------------------------------------------------------------------------
# The CLI
# ---------------------------------------------------------------------------

if(SQCART_ENABLE_CLI)
    add_subdirectory(app)
endif()

# ---------------------------------------------------------------------------
# Isolation assertion
#
# When built standalone, no target may see an include directory outside this
# source or binary tree. This catches the failure a no-upward-includes review
# rule does not: silent inheritance of parent configuration.
# ---------------------------------------------------------------------------

function(_sqcart_assert_contained target)
    if(NOT PROJECT_IS_TOP_LEVEL)
        return()
    endif()
    foreach(prop INCLUDE_DIRECTORIES INTERFACE_INCLUDE_DIRECTORIES)
        get_target_property(dirs ${target} ${prop})
        if(NOT dirs)
            continue()
        endif()
        foreach(dir ${dirs})
            string(REGEX REPLACE "\\$<BUILD_INTERFACE:(.*)>" "\\1" dir "${dir}")
            if(dir MATCHES "^\\$<")
                continue()
            endif()
            get_filename_component(dir "${dir}" ABSOLUTE)
            string(FIND "${dir}" "${PROJECT_SOURCE_DIR}" in_src)
            string(FIND "${dir}" "${PROJECT_BINARY_DIR}" in_bin)
            if(NOT in_src EQUAL 0 AND NOT in_bin EQUAL 0)
                message(FATAL_ERROR
                    "isolation violation: target '${target}' includes '${dir}', "
                    "which is outside the sqcart tree.")
            endif()
        endforeach()
    endforeach()
endfunction()

_sqcart_assert_contained(sqcart_read)
if(TARGET sqcart_write)
    _sqcart_assert_contained(sqcart_write)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

if(SQCART_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
    if(SQCART_ENABLE_CLI)
        add_subdirectory(app/tests)
    endif()
endif()

if(PROJECT_IS_TOP_LEVEL)
    message(STATUS "sqcart ${PROJECT_VERSION} (standalone)")
    message(STATUS "  std::expected : ${SQCART_HAVE_STD_EXPECTED}")
    message(STATUS "  writer        : ${SQCART_ENABLE_WRITER}")
    message(STATUS "  cli           : ${SQCART_ENABLE_CLI}")
    message(STATUS "  lua binding   : ${SQCART_ENABLE_LUA}")
    message(STATUS "  tests         : ${SQCART_BUILD_TESTS}")
endif()
