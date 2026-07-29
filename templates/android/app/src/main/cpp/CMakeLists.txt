cmake_minimum_required(VERSION 3.20)

project({{PROJECT_ID}} LANGUAGES C CXX)

foreach(required_variable SDL2_ROOT LUA_ROOT YYJSON_ROOT PROJECT_ROOT)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} was not provided")
    endif()
endforeach()

set(SDL2_INCLUDE_DIR "${SDL2_ROOT}/include/SDL2")
set(SDL2_LIBRARY_DIR "${SDL2_ROOT}/lib/arm64-v8a")

foreach(component SDL2 SDL2_ttf SDL2_mixer SDL2_image SDL2_net)
    set(component_library "${SDL2_LIBRARY_DIR}/lib${component}.so")
    if(NOT EXISTS "${component_library}")
        message(FATAL_ERROR "Library not found: ${component_library}")
    endif()

    add_library(${component} SHARED IMPORTED GLOBAL)
    set_target_properties(${component} PROPERTIES
        IMPORTED_LOCATION "${component_library}"
    )
endforeach()

if(NOT EXISTS "${SDL2_INCLUDE_DIR}/SDL.h")
    message(FATAL_ERROR "SDL2 headers not found: ${SDL2_INCLUDE_DIR}")
endif()

find_library(SQUARED_GLES2_LIBRARY NAMES GLESv2)
if(NOT SQUARED_GLES2_LIBRARY)
    foreach(candidate
        "/system/lib64/libGLESv2.so"
        "/system/lib/libGLESv2.so"
    )
        if(EXISTS "${candidate}")
            set(SQUARED_GLES2_LIBRARY "${candidate}")
            break()
        endif()
    endforeach()
endif()
if(NOT SQUARED_GLES2_LIBRARY)
    message(FATAL_ERROR
        "OpenGL ES 2 link library not found; expected an Android NDK "
        "GLESv2 stub or /system/lib64/libGLESv2.so"
    )
endif()
message(STATUS
    "Squared OpenGL ES 2 library: ${SQUARED_GLES2_LIBRARY}"
)

file(GLOB LUA_SOURCES CONFIGURE_DEPENDS "${LUA_ROOT}/src/*.c")
list(FILTER LUA_SOURCES EXCLUDE REGEX "/lua\\.c$")
list(FILTER LUA_SOURCES EXCLUDE REGEX "/luac\\.c$")

add_library(lua_runtime STATIC ${LUA_SOURCES})
set_target_properties(lua_runtime PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
target_compile_definitions(lua_runtime PRIVATE LUA_USE_LINUX)
target_include_directories(lua_runtime PUBLIC "${LUA_ROOT}/src")

if(NOT EXISTS "${YYJSON_ROOT}/src/yyjson.c" OR
   NOT EXISTS "${YYJSON_ROOT}/src/yyjson.h")
    message(FATAL_ERROR
        "yyjson 0.12.0 sources not found: ${YYJSON_ROOT}"
    )
endif()

add_library(yyjson STATIC
    "${YYJSON_ROOT}/src/yyjson.c"
)
set_target_properties(yyjson PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
target_include_directories(yyjson PUBLIC
    "${YYJSON_ROOT}/src"
)

add_library(squared_data STATIC
    squared/data/json.cpp
)
set_target_properties(squared_data PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
target_compile_features(squared_data PUBLIC cxx_std_20)
target_compile_options(squared_data PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
target_include_directories(squared_data PUBLIC
    "${PROJECT_ROOT}/include"
)
target_link_libraries(squared_data PUBLIC
    yyjson
)

add_library(squared_time STATIC
    squared/time/timepiece.cpp
)
set_target_properties(squared_time PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
target_compile_features(squared_time PUBLIC cxx_std_20)
target_compile_options(squared_time PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
target_include_directories(squared_time PUBLIC
    "${PROJECT_ROOT}/include"
)

add_library(squared_messaging STATIC
    squared/messaging/message_dispatcher.cpp
)
set_target_properties(squared_messaging PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
target_compile_features(squared_messaging PUBLIC cxx_std_20)
target_compile_options(squared_messaging PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
target_include_directories(squared_messaging PUBLIC
    "${PROJECT_ROOT}/include"
)
target_link_libraries(squared_messaging PUBLIC
    squared_data
    squared_time
)

add_library(squared_graphics2d STATIC
    squared/graphics/context.cpp
    squared/graphics2d/orthographic_camera.cpp
    squared/graphics2d/sprite_batch.cpp
    squared/graphics2d/texture.cpp
    squared/graphics2d/texture_atlas.cpp
    squared/math/matrix4.cpp
)
set_target_properties(squared_graphics2d PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
target_compile_features(squared_graphics2d PUBLIC cxx_std_20)
target_compile_options(squared_graphics2d PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
target_include_directories(squared_graphics2d PUBLIC
    "${PROJECT_ROOT}/include"
    "${SDL2_INCLUDE_DIR}"
)
target_link_libraries(squared_graphics2d PUBLIC
    SDL2_image
    SDL2
    "${SQUARED_GLES2_LIBRARY}"
)

add_subdirectory(application)

add_library(main SHARED
    platform/sdl_main.cpp
)

target_compile_features(main PRIVATE cxx_std_20)
target_compile_options(main PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
target_include_directories(main PRIVATE
    "${PROJECT_ROOT}/include"
    "${SDL2_INCLUDE_DIR}"
)
target_link_libraries(main PRIVATE
    project_application
    SDL2_image
    SDL2_mixer
    SDL2_ttf
    SDL2_net
    SDL2
    dl
    m
)
set_target_properties(main PROPERTIES OUTPUT_NAME "main")
