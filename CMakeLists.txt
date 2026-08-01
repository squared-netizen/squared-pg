cmake_minimum_required(VERSION 3.20)

project(squared_pg_toolchain VERSION 0.6.0 LANGUAGES C CXX)

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
set(YYJSON_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/yyjson-0.12.0"
)
set(MINIZ_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniz-3.1.2"
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

if(NOT EXISTS "${YYJSON_ROOT}/src/yyjson.c" OR
   NOT EXISTS "${YYJSON_ROOT}/src/yyjson.h")
    message(FATAL_ERROR
        "yyjson 0.12.0 source is missing. Run: lua5.4 toolchain.lua"
    )
endif()

add_library(yyjson_0_12_0 STATIC
    "${YYJSON_ROOT}/src/yyjson.c"
)
set_target_properties(yyjson_0_12_0 PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
target_include_directories(yyjson_0_12_0
    PUBLIC
        "${YYJSON_ROOT}/src"
)

if(NOT EXISTS "${MINIZ_ROOT}/miniz.h" OR
   NOT EXISTS "${MINIZ_ROOT}/miniz_zip.c")
    message(FATAL_ERROR
        "miniz 3.1.2 source is missing. Run: lua5.4 toolchain.lua"
    )
endif()

set(MINIZ_GENERATED_INCLUDE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/miniz-3.1.2"
)
file(MAKE_DIRECTORY "${MINIZ_GENERATED_INCLUDE}")
file(WRITE "${MINIZ_GENERATED_INCLUDE}/miniz_export.h"
    "#ifndef MINIZ_EXPORT\n"
    "#define MINIZ_EXPORT\n"
    "#endif\n"
)

add_library(miniz_3_1_2 STATIC
    "${MINIZ_ROOT}/miniz.c"
    "${MINIZ_ROOT}/miniz_zip.c"
    "${MINIZ_ROOT}/miniz_tinfl.c"
    "${MINIZ_ROOT}/miniz_tdef.c"
)
set_target_properties(miniz_3_1_2 PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
target_include_directories(miniz_3_1_2
    PUBLIC
        "${MINIZ_ROOT}"
        "${MINIZ_GENERATED_INCLUDE}"
)
target_compile_options(miniz_3_1_2
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)
if(UNIX)
    target_compile_definitions(miniz_3_1_2 PRIVATE _GNU_SOURCE)
endif()
target_compile_definitions(miniz_3_1_2
    PUBLIC
        MINIZ_NO_TIME
    PRIVATE
        MINIZ_NO_ZLIB_APIS
)

add_library(squared_sq_core STATIC
    src/sq/error.cpp
    src/sq/package.cpp
    src/sq/sha256.cpp
    src/sq/writer.cpp
)
set_target_properties(squared_sq_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
target_compile_features(squared_sq_core PUBLIC cxx_std_20)
target_compile_options(squared_sq_core
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)
target_include_directories(squared_sq_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/sq"
)
target_link_libraries(squared_sq_core
    PRIVATE
        miniz_3_1_2
        yyjson_0_12_0
)

add_executable(squared_sq_package_test
    tests/sq-package-test.cpp
)

add_library(squared_sq_lua MODULE
    src/sq/lua_binding.cpp
)
target_compile_features(squared_sq_lua PRIVATE cxx_std_20)
target_compile_options(squared_sq_lua
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)
target_include_directories(squared_sq_lua
    PRIVATE
        "${LUA_SOURCE_DIR}"
)
target_link_libraries(squared_sq_lua
    PRIVATE
        squared_sq_core
)
set_target_properties(squared_sq_lua PROPERTIES
    PREFIX ""
    OUTPUT_NAME "sq"
)
target_compile_features(squared_sq_package_test PRIVATE cxx_std_20)
target_compile_options(squared_sq_package_test
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)
target_link_libraries(squared_sq_package_test
    PRIVATE
        squared_sq_core
)
set_target_properties(squared_sq_package_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(squared_data_json_test
    tests/json-test.cpp
    packages/squared-data/content/modules/squared-data/src/json.cpp
)
target_compile_features(squared_data_json_test PRIVATE cxx_std_20)
target_compile_options(squared_data_json_test
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)
target_include_directories(squared_data_json_test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/packages/squared-data/content/modules/squared-data/include"
)
target_link_libraries(squared_data_json_test
    PRIVATE
        yyjson_0_12_0
)
set_target_properties(squared_data_json_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(squared_application_boundary_test
    tests/application-boundary-test.cpp
)
target_compile_features(
    squared_application_boundary_test
    PRIVATE
        cxx_std_20
)
target_compile_options(squared_application_boundary_test
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)
target_include_directories(squared_application_boundary_test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/packages/squared-application/content/modules/squared-application/include"
)
set_target_properties(
    squared_application_boundary_test
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(squared_graphics_test
    tests/graphics-test.cpp
)
target_compile_features(squared_graphics_test PRIVATE cxx_std_20)
target_compile_options(squared_graphics_test
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)
target_include_directories(squared_graphics_test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/packages/squared-graphics/content/modules/squared-graphics/include"
)
set_target_properties(squared_graphics_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(squared_math_test
    tests/math-test.cpp
    packages/squared-math/content/modules/squared-math/src/matrix4.cpp
)
target_compile_features(squared_math_test PRIVATE cxx_std_20)
target_compile_options(squared_math_test
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)
target_include_directories(squared_math_test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/packages/squared-math/content/modules/squared-math/include"
)
set_target_properties(squared_math_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(squared_scene2d_test
    tests/scene2d-test.cpp
    packages/squared-scene2d/content/modules/squared-scene2d/src/actor.cpp
    packages/squared-scene2d/content/modules/squared-scene2d/src/group.cpp
    packages/squared-scene2d/content/modules/squared-scene2d/src/stage.cpp
)
target_compile_features(squared_scene2d_test PRIVATE cxx_std_20)
target_compile_options(squared_scene2d_test
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)
target_include_directories(squared_scene2d_test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/packages/squared-scene2d/content/modules/squared-scene2d/include"
)
set_target_properties(squared_scene2d_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(squared_time_test
    tests/time-test.cpp
    packages/squared-time/content/modules/squared-time/src/timepiece.cpp
)
target_compile_features(squared_time_test PRIVATE cxx_std_20)
target_compile_options(squared_time_test
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)
target_include_directories(squared_time_test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/packages/squared-time/content/modules/squared-time/include"
)
set_target_properties(squared_time_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(squared_messaging_test
    tests/messaging-test.cpp
    packages/squared-messaging/content/modules/squared-messaging/src/message_dispatcher.cpp
    packages/squared-time/content/modules/squared-time/src/timepiece.cpp
    packages/squared-data/content/modules/squared-data/src/json.cpp
)
target_compile_features(squared_messaging_test PRIVATE cxx_std_20)
target_compile_options(squared_messaging_test
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)
target_include_directories(squared_messaging_test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/packages/squared-messaging/content/modules/squared-messaging/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/packages/squared-data/content/modules/squared-data/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/packages/squared-time/content/modules/squared-time/include"
)
target_link_libraries(squared_messaging_test
    PRIVATE
        yyjson_0_12_0
)
set_target_properties(squared_messaging_test PROPERTIES
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
    TARGETS squared_sq_lua
    LIBRARY DESTINATION lib/lua/5.4/squared
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

    add_test(
        NAME squared_data_strict_json
        COMMAND
            "$<TARGET_FILE:squared_data_json_test>"
    )

    add_test(
        NAME squared_application_boundary
        COMMAND
            "$<TARGET_FILE:squared_application_boundary_test>"
    )

    add_test(
        NAME squared_graphics_color
        COMMAND
            "$<TARGET_FILE:squared_graphics_test>"
    )

    add_test(
        NAME squared_math_primitives
        COMMAND
            "$<TARGET_FILE:squared_math_test>"
    )

    add_test(
        NAME squared_scene2d_hierarchy
        COMMAND
            "$<TARGET_FILE:squared_scene2d_test>"
    )

    add_test(
        NAME squared_time_domains
        COMMAND
            "$<TARGET_FILE:squared_time_test>"
    )

    add_test(
        NAME squared_telegram_messaging
        COMMAND
            "$<TARGET_FILE:squared_messaging_test>"
    )

    add_test(
        NAME squared_sq_package
        COMMAND
            "$<TARGET_FILE:squared_sq_package_test>"
    )

    set_tests_properties(lua_embedding_error PROPERTIES
        WILL_FAIL TRUE
    )
endif()
