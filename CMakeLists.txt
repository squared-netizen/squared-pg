# squared-pg — offline-first project generator for the Squared framework.
#
# Offline is a hard constraint (§1.13). There is no FetchContent here, no
# find_package that may reach a network, and no submodule to initialise: a
# clean checkout plus third-party/ is the whole dependency list.
#
#   cmake -S . -B build && cmake --build build && ctest --test-dir build
#
# Makefile in this directory is a hand-written equivalent for hosts without
# CMake (Termux, chiefly). The two must stay behaviourally identical.

cmake_minimum_required(VERSION 3.21)

project(squared-pg
    VERSION     0.1.0
    DESCRIPTION "Offline-first project generator for the Squared framework"
    LANGUAGES   C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

option(SQUARED_PG_BUILD_TESTS "Build the engine test suite" ${PROJECT_IS_TOP_LEVEL})
option(SQUARED_PG_BUILD_CLI   "Build the sqpg command-line host" ${PROJECT_IS_TOP_LEVEL})

set(SQUARED_PG_THIRD_PARTY "${CMAKE_CURRENT_SOURCE_DIR}/third-party")

# ---------------------------------------------------------------------------
# Vendored C dependencies
#
# Built here and handed to sqcart through its SQCART_USE_EXTERNAL_* options, so
# one build never links two copies of miniz or yyjson (§1.6).
# ---------------------------------------------------------------------------

foreach(_required
        "${SQUARED_PG_THIRD_PARTY}/miniz-3.1.2/miniz.c"
        "${SQUARED_PG_THIRD_PARTY}/yyjson-0.12.0/src/yyjson.c"
        "${SQUARED_PG_THIRD_PARTY}/crypto-algorithms/sha256.c"
        "${SQUARED_PG_THIRD_PARTY}/lua-5.4.8/src/lapi.c")
    if(NOT EXISTS "${_required}")
        message(FATAL_ERROR
            "vendored dependency missing: ${_required}\n"
            "squared-pg builds offline from third-party/ and cannot fetch it.")
    endif()
endforeach()

add_library(miniz STATIC
    "${SQUARED_PG_THIRD_PARTY}/miniz-3.1.2/miniz.c"
    "${SQUARED_PG_THIRD_PARTY}/miniz-3.1.2/miniz_tdef.c"
    "${SQUARED_PG_THIRD_PARTY}/miniz-3.1.2/miniz_tinfl.c"
    "${SQUARED_PG_THIRD_PARTY}/miniz-3.1.2/miniz_zip.c")
target_include_directories(miniz SYSTEM PUBLIC "${SQUARED_PG_THIRD_PARTY}/miniz-3.1.2")

add_library(yyjson STATIC "${SQUARED_PG_THIRD_PARTY}/yyjson-0.12.0/src/yyjson.c")
target_include_directories(yyjson SYSTEM PUBLIC "${SQUARED_PG_THIRD_PARTY}/yyjson-0.12.0/src")

add_library(squared_pg_sha256 STATIC "${SQUARED_PG_THIRD_PARTY}/crypto-algorithms/sha256.c")
target_include_directories(squared_pg_sha256 SYSTEM PUBLIC
    "${SQUARED_PG_THIRD_PARTY}/crypto-algorithms")

# Lua 5.4. Everything except the standalone interpreter and compiler drivers.
# LUA_USE_POSIX rather than LUA_USE_LINUX: the latter pulls in readline and
# dlopen, neither of which the embedded library needs, and both of which are
# awkward on Termux.
file(GLOB SQUARED_PG_LUA_SOURCES "${SQUARED_PG_THIRD_PARTY}/lua-5.4.8/src/*.c")
list(FILTER SQUARED_PG_LUA_SOURCES EXCLUDE REGEX "/(lua|luac)\\.c$")
add_library(lua STATIC ${SQUARED_PG_LUA_SOURCES})
target_include_directories(lua SYSTEM PUBLIC "${SQUARED_PG_THIRD_PARTY}/lua-5.4.8/src")
target_compile_definitions(lua PRIVATE LUA_USE_POSIX)

# ---------------------------------------------------------------------------
# sqcart
#
# Linked from vendored in-tree source. squared-pg must never acquire sqcart as
# a cartridge: the tool that reads cartridges cannot itself be delivered as one
# without a bootstrap it has no way to complete.
# ---------------------------------------------------------------------------

set(SQCART_USE_EXTERNAL_MINIZ  ON  CACHE BOOL "" FORCE)
set(SQCART_USE_EXTERNAL_YYJSON ON  CACHE BOOL "" FORCE)
set(SQCART_ENABLE_WRITER       ON  CACHE BOOL "" FORCE)
set(SQCART_ENABLE_CLI          OFF CACHE BOOL "" FORCE)
set(SQCART_BUILD_TESTS         OFF CACHE BOOL "" FORCE)
add_subdirectory(sqcart)

# ---------------------------------------------------------------------------
# The engine
# ---------------------------------------------------------------------------

add_subdirectory(engine)

if(SQUARED_PG_BUILD_CLI)
    add_subdirectory(app)
endif()

if(SQUARED_PG_BUILD_TESTS)
    enable_testing()
    add_subdirectory(engine/tests)
endif()

if(PROJECT_IS_TOP_LEVEL)
    message(STATUS "squared-pg ${PROJECT_VERSION}")
    message(STATUS "  cli    : ${SQUARED_PG_BUILD_CLI}")
    message(STATUS "  tests  : ${SQUARED_PG_BUILD_TESTS}")
endif()
