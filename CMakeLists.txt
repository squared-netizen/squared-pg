cmake_minimum_required(VERSION 3.20)

project(sdl_project_generator_toolchain VERSION 0.4.0 LANGUAGES C CXX)

include(CTest)

set(LUA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua-5.4.8")
set(LUA_SOURCE_DIR "${LUA_ROOT}/src")
set(LFS_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/luafilesystem-1_8_0"
)
set(PENLIGHT_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Penlight-1.14.0"
)
set(LDOC_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ldoc-1.5.0"
)

if(NOT EXISTS "${LUA_SOURCE_DIR}/lua.h")
    message(FATAL_ERROR
        "Lua 5.4.8 source is missing. Run: lua5.4 bootstrap.lua"
    )
endif()

set(LUA_LIBRARY_SOURCES
    "${LUA_SOURCE_DIR}/lapi.c"
    "${LUA_SOURCE_DIR}/lauxlib.c"
    "${LUA_SOURCE_DIR}/lbaselib.c"
    "${LUA_SOURCE_DIR}/lcode.c"
    "${LUA_SOURCE_DIR}/lcorolib.c"
    "${LUA_SOURCE_DIR}/lctype.c"
    "${LUA_SOURCE_DIR}/ldblib.c"
    "${LUA_SOURCE_DIR}/ldebug.c"
    "${LUA_SOURCE_DIR}/ldo.c"
    "${LUA_SOURCE_DIR}/ldump.c"
    "${LUA_SOURCE_DIR}/lfunc.c"
    "${LUA_SOURCE_DIR}/lgc.c"
    "${LUA_SOURCE_DIR}/linit.c"
    "${LUA_SOURCE_DIR}/liolib.c"
    "${LUA_SOURCE_DIR}/llex.c"
    "${LUA_SOURCE_DIR}/lmathlib.c"
    "${LUA_SOURCE_DIR}/lmem.c"
    "${LUA_SOURCE_DIR}/loadlib.c"
    "${LUA_SOURCE_DIR}/lobject.c"
    "${LUA_SOURCE_DIR}/lopcodes.c"
    "${LUA_SOURCE_DIR}/loslib.c"
    "${LUA_SOURCE_DIR}/lparser.c"
    "${LUA_SOURCE_DIR}/lstate.c"
    "${LUA_SOURCE_DIR}/lstring.c"
    "${LUA_SOURCE_DIR}/lstrlib.c"
    "${LUA_SOURCE_DIR}/ltable.c"
    "${LUA_SOURCE_DIR}/ltablib.c"
    "${LUA_SOURCE_DIR}/ltm.c"
    "${LUA_SOURCE_DIR}/lundump.c"
    "${LUA_SOURCE_DIR}/lutf8lib.c"
    "${LUA_SOURCE_DIR}/lvm.c"
    "${LUA_SOURCE_DIR}/lzio.c"
)

add_library(lua_5_4_8 STATIC ${LUA_LIBRARY_SOURCES})

target_include_directories(lua_5_4_8
    PUBLIC
        "${LUA_SOURCE_DIR}"
)

set_target_properties(lua_5_4_8 PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

if(UNIX)
    target_compile_definitions(lua_5_4_8 PRIVATE LUA_USE_LINUX)
endif()

add_executable(lua_private
    "${LUA_SOURCE_DIR}/lua.c"
)

if(UNIX)
    target_compile_definitions(lua_private PRIVATE LUA_USE_LINUX)
endif()

target_link_libraries(lua_private
    PRIVATE
        lua_5_4_8
        ${CMAKE_DL_LIBS}
        m
)

set_target_properties(lua_private PROPERTIES
    ENABLE_EXPORTS ON
    OUTPUT_NAME "lua-5.4.8"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(lua_embed_smoke
    src/lua_host.cpp
    src/main.cpp
)

target_compile_features(lua_embed_smoke PRIVATE cxx_std_20)

target_compile_options(lua_embed_smoke
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

target_link_libraries(lua_embed_smoke
    PRIVATE
        lua_5_4_8
        ${CMAKE_DL_LIBS}
        m
)

set_target_properties(lua_embed_smoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

install(
    TARGETS
        lua_private
        lua_5_4_8
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
)

install(
    FILES
        "${LUA_SOURCE_DIR}/lua.h"
        "${LUA_SOURCE_DIR}/lua.hpp"
        "${LUA_SOURCE_DIR}/lauxlib.h"
        "${LUA_SOURCE_DIR}/luaconf.h"
        "${LUA_SOURCE_DIR}/lualib.h"
    DESTINATION include/lua5.4
)

if(EXISTS "${LFS_ROOT}/src/lfs.c")
    add_library(lfs MODULE
        "${LFS_ROOT}/src/lfs.c"
    )

    target_include_directories(lfs
        PRIVATE
            "${LUA_SOURCE_DIR}"
    )

    set_target_properties(lfs PROPERTIES
        PREFIX ""
        OUTPUT_NAME "lfs"
    )

    install(
        TARGETS lfs
        LIBRARY DESTINATION lib/lua/5.4
    )
endif()

if(EXISTS "${PENLIGHT_ROOT}/lua/pl/init.lua")
    install(
        DIRECTORY "${PENLIGHT_ROOT}/lua/pl"
        DESTINATION share/lua/5.4
    )
endif()

if(EXISTS "${LDOC_ROOT}/ldoc.lua")
    install(
        FILES "${LDOC_ROOT}/ldoc.lua"
        DESTINATION share/lua/5.4
    )

    install(
        DIRECTORY "${LDOC_ROOT}/ldoc"
        DESTINATION share/lua/5.4
    )
endif()

if(BUILD_TESTING)
    add_test(
        NAME lua_runtime_version
        COMMAND
            "$<TARGET_FILE:lua_private>"
            -e
            "assert(_VERSION == 'Lua 5.4')"
    )

    add_test(
        NAME sha256_known_vectors
        COMMAND
            "$<TARGET_FILE:lua_private>"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/sha256-test.lua"
    )

    add_test(
        NAME lua_embedding_success
        COMMAND
            "$<TARGET_FILE:lua_embed_smoke>"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/smoke-test.lua"
    )

    add_test(
        NAME lua_embedding_error
        COMMAND
            "$<TARGET_FILE:lua_embed_smoke>"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/error-test.lua"
    )

    set_tests_properties(lua_embedding_error PROPERTIES
        WILL_FAIL TRUE
    )
endif()
