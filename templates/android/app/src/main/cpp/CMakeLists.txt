cmake_minimum_required(VERSION 3.20)

project({{PROJECT_ID}} LANGUAGES C CXX)

foreach(required_variable SDL2_ROOT LUA_ROOT PROJECT_ROOT)
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

file(GLOB LUA_SOURCES CONFIGURE_DEPENDS "${LUA_ROOT}/src/*.c")
list(FILTER LUA_SOURCES EXCLUDE REGEX "/lua\\.c$")
list(FILTER LUA_SOURCES EXCLUDE REGEX "/luac\\.c$")

add_library(lua_runtime STATIC ${LUA_SOURCES})
set_target_properties(lua_runtime PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
target_compile_definitions(lua_runtime PRIVATE LUA_USE_LINUX)
target_include_directories(lua_runtime PUBLIC "${LUA_ROOT}/src")

add_library(main SHARED
    main.cpp
    script_runtime.cpp
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
    lua_runtime
    SDL2_image
    SDL2_mixer
    SDL2_ttf
    SDL2_net
    SDL2
    dl
    m
)
set_target_properties(main PROPERTIES OUTPUT_NAME "main")
